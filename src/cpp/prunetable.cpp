#include "prunetable.h"
#include "city.h"
#include "cmds.h"
#include <iostream>
#include <set>
#if !defined(_WIN32) && !defined(WASM)
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
fillworker fillworkers[MAXTHREADS];
string inputbasename;
int writeprunetables = 1; // default is auto
int startprunedepth = 3;
double fillpref = 1;
static doubopt fillprefopt(
    "--fillpref",
    "float If greater than 1, prefer filling table, else prefer searching.\n",
    &fillpref);
ull fasthash(int n, const setval sv) {
  return CityHash64((const char *)sv.dat, n);
}
void setupparams(const puzdef &pd, prunetable &pt, int numthreads,
                 vector<workerparam> &workerparams) {
  workerparams.clear();
  while ((int)workerparams.size() < numthreads) {
    int i = workerparams.size();
    workerparams.push_back(workerparam(pd, pt, i));
  }
}
int setupthreads(const puzdef &pd, prunetable &pt, vector<ull> &workchunks,
                 vector<workerparam> &workerparams) {
  int wthreads = min(numthreads, (int)workchunks.size());
  setupparams(pd, pt, wthreads, workerparams);
  return wthreads;
}
void *fillthreadworker(void *o) {
  workerparam *wp = (workerparam *)o;
  fillworkers[wp->tid].dowork(wp->pd, wp->pt);
  return 0;
}
void fillworker::init(const puzdef &pd, int d_) {
  if (looktmp) {
    delete looktmp;
    looktmp = 0;
  }
  looktmp = new allocsetval(pd, pd.solved);
  while (posns.size() <= 100 || (int)posns.size() <= d_ + 1)
    posns.push_back(allocsetval(pd, pd.solved));
  pd.assignpos(posns[0], pd.solved);
  d = d_;
  for (int i = 0; i < MEMSHARDS; i++)
    fillbufs[i].nchunks = 0;
  wfillcnt = 0;
}
ull fillworker::fillstart(const puzdef &pd, prunetable &pt, int w) {
  ull initmoves = pt.workchunks[w];
  int nmoves = pd.moves.size();
  int sp = 0;
  int st = 0;
  // With rotations, fill chunks are deduplicated up to rotation and carry no
  // canonical state (see makeworkchunks): their fill starts from state 0.
  int canonprefix = pd.rotgroup.size() <= 1;
  int togo = d;
  while (initmoves > 1) {
    int mv = initmoves % nmoves;
    pd.mul(posns[sp], pd.moves[mv].pos, posns[sp + 1]);
    if (!pd.legalstate(posns[sp + 1]) ||
        pd.comparepos(posns[sp], posns[sp + 1]) == 0)
      return 0;
    if (canonprefix)
      st = canonnext[st][pd.moves[mv].cs];
    sp++;
    togo--;
    initmoves /= nmoves;
  }
  ull r = filltable(pd, pt, togo, sp, st);
  for (int i = 0; i < MEMSHARDS; i++)
    r += fillflush(pt, i);
  return r;
}
ull fillworker::fillflush(prunetable &pt, int shard) {
  ull r = 0;
  fillbuf &fb = fillbufs[shard];
  if (fb.nchunks > 0) {
#ifdef USE_PTHREADS
    pthread_mutex_lock(&(memshards[shard].mutex));
#endif
    wfillcnt += fb.nchunks;
    for (int i = 0; i < fb.nchunks; i++) {
      ull h = fb.chunks[i];
      if (((pt.mem[h >> 5] >> (2 * (h & 31))) & 3) == 0) {
        pt.mem[h >> 5] += (3LL - pt.wval) << (2 * (h & 31));
        if ((pt.mem[(h >> 5) & -8] & 15) == 0)
          pt.mem[(h >> 5) & -8] += 1 + pt.wbval;
        r++;
      }
    }
#ifdef USE_PTHREADS
    pthread_mutex_unlock(&(memshards[shard].mutex));
#endif
    fb.nchunks = 0;
  }
  return r;
}
void fillworker::dowork(const puzdef &pd, prunetable &pt) {
  while (1) {
    int w = -1;
    get_global_lock();
    if (pt.workat < (int)pt.workchunks.size())
      w = pt.workat++;
    release_global_lock();
    if (w < 0)
      return;
    ull cnt = fillstart(pd, pt, w);
    get_global_lock();
    pt.popped += cnt;
    release_global_lock();
  }
}
ull fillworker::filltable(const puzdef &pd, prunetable &pt, int togo, int sp,
                          int st) {
  ull r = 0;
  if (togo == 0) {
    ull h;
    if ((int)pd.rotgroup.size() > 1) {
      slowmodm2(pd, posns[sp], *looktmp);
      h = pt.indexhash(pd.totsize, *looktmp);
    } else {
      h = pt.indexhash(pd.totsize, posns[sp]);
    }
    int shard = (h >> pt.shardshift);
    fillbuf &fb = fillbufs[shard];
    fb.chunks[fb.nchunks++] = h;
    if (fb.nchunks >= FILLCHUNKS)
      r += fillflush(pt, shard);
    return r;
  }
  ull mask = canonmask[st];
  const vector<int> &ns = canonnext[st];
  for (int m = 0; m < (int)pd.moves.size(); m++) {
    const moove &mv = pd.moves[m];
    if ((mask >> mv.cs) & 1)
      continue;
    pd.mul(posns[sp], mv.pos, posns[sp + 1]);
    if (!pd.legalstate(posns[sp + 1]) ||
        pd.comparepos(posns[sp], posns[sp + 1]) == 0)
      continue;
    r += filltable(pd, pt, togo - 1, sp + 1, ns[mv.cs]);
  }
  return r;
}
/*
 *   We used to support table sizes that were only powers of two.
 *   Then we supported table sizes that are powers of 2 and also
 *   3/4, 7/8, 15/16, and 31/32 times powers of two.  But now we
 *   support memory sizes that are integer k * 2^n where k <= 255.
 *   We do this with a right shift, then a multiply, then another
 *   right shift, always staying 2^64 bytes.
 */
/*
 *   What a table holds before anything has been read into it: the hash
 *   parameters that follow from its size, and counters at zero.  readpt
 *   reads a file straight into these fields, so this is also how a failed
 *   read is undone.
 */
void prunetable::freshtable() {
  shift2 = 2;
  while ((size & (1LL << shift2)) == 0)
    shift2++;
  memmul = size >> shift2;
  shift1 = 0;
  while (memmul >> shift1)
    shift1++;
  ull hi = memmul * (0xffffffffffffffffULL >> shift1);
  shift2 = 0;
  while ((hi >> shift2) > size)
    shift2++;
  shardshift = 0;
  while ((size >> shardshift) > MEMSHARDS)
    shardshift++;
  totsize = pdp->totsize;
  totpop = 0;
  ptotpop = 0;
  popped = 0;
  baseval = 0;
  hibase = 0;
  wval = 0;
  lookupcnt = 0;
  fillcnt = 0;
  justread = 0;
  for (int i = 0; i < 7; i++) {
    if (dtabs[i])
      free(dtabs[i]);
    dtabs[i] = 0;
  }
}

prunetable::prunetable(const puzdef &pd, ull maxmem) {
  pdp = &pd;
  // Before anything else: freshtable() frees these, and this constructor
  // does not go through the default one that clears them.
  amem = 0;
  mem = 0;
  for (int i = 0; i < 7; i++)
    dtabs[i] = 0;
  totsize = pd.totsize;
  ull bytesize = 2048;
  while (2 * bytesize <= maxmem &&
         (pd.logstates > 55 || 8 * bytesize < pd.llstates))
    bytesize *= 2;
  // now add up to 7 additional bits, so long as we don't go over
  // maxmem or the other limits.
  ull subbytesize = bytesize;
  for (int i = 0; i < 7; i++) {
    subbytesize >>= 1;
    if (bytesize + subbytesize <= maxmem &&
        (pd.logstates > 55 || 8 * (subbytesize + bytesize) < pd.llstates))
      bytesize += subbytesize;
  }
  // now calculate the shifts.
  size = 4 * bytesize;
  freshtable();
  if (quiet == 0)
    cout << "For memsize " << maxmem << " sh1 " << shift1 << " mul " << memmul
         << " sh2 " << shift2 << " shardshift " << shardshift << endl;
  if (shmattachname) {
    shmattach(pd);
    return;
  }
  if (shmholdname) {
    shmcreate(bytesize);
  } else {
    cout << "Trying to allocate "
         << (CACHELINESIZE + (bytesize >> 3) * sizeof(ull)) << endl;
    amem = mem =
        (ull *)calloc(CACHELINESIZE + (bytesize >> 3) * sizeof(ull), 1);
    if (mem == 0)
      error("! could not allocate main memory buffer");
    // hack memalign
    while (((ull)mem) & (CACHELINESIZE - 1))
      mem++;
  }
  if (!readpt(pd)) {
    // A read that gave up part way through has left the file's numbers in
    // this table and the file's bytes in memory.  Filling a table on top of
    // either gives answers that are wrong, not just slow, so start over
    // from nothing.
    freshtable();
    memset(mem, 0, (bytesize >> 3) * sizeof(ull));
    if (quiet == 0)
      cout << "Initializing memory in " << duration() << endl << flush;
    baseval = 1;
    filltable(pd, 0);
    filltable(pd, 1);
    filltable(pd, 2);
    if (startprunedepth) {
      for (int i = 3; i <= startprunedepth; i++)
        checkextend(pd, 1);
    } else {
      checkextend(pd, 1);
    }
  }
  if (shmholdname)
    shmpublish(pd);
}
void prunetable::filltable(const puzdef &pd, int d) {
  popped = 0;
  wbval = min(d, 14);
  ll ofillcnt = fillcnt;
  if (quiet == 0)
    cout << "Filling depth " << d << " val " << wval << flush;
  workchunks = makeworkchunks(pd, d, pd.solved, 1, 1);
  workat = 0;
  int wthreads = setupthreads(pd, *this, workchunks, workerparams);
  for (int t = 0; t < wthreads; t++)
    fillworkers[t].init(pd, d);
#ifdef USE_PTHREADS
  for (int i = 0; i < wthreads; i++)
    spawn_thread(i, fillthreadworker, &(workerparams[i]));
  for (int i = 0; i < wthreads; i++)
    join_thread(i);
#else
  fillthreadworker((void *)&workerparams[0]);
#endif
  for (int i = 0; i < wthreads; i++)
    fillcnt += fillworkers[i].wfillcnt;
  if (quiet == 0) {
    double dur = duration();
    double rate = (fillcnt - ofillcnt) / dur / 1e6;
    cout << " saw " << popped << " (" << (fillcnt - ofillcnt) << ") in " << dur
         << " rate " << rate << endl
         << flush;
  }
  ptotpop = totpop;
  totpop += popped;
  justread = 0;
}
void prunetable::checkextend(const puzdef &pd, int ignorelookup) {
  if (shmreadonly)
    return;
  double prediction = 0;
  if (ptotpop != 0)
    prediction = totpop * (double)totpop / ptotpop;
  if ((ignorelookup == 0 && lookupcnt * fillpref < 3 * fillcnt) ||
      baseval > 100 || prediction > size ||
      (pd.logstates <= 50 && prediction > pd.llstates))
    return;
  if (wval == 2) {
    ull longcnt = (size + 31) >> 5;
    if (quiet == 0)
      cout << "Demoting memory values " << flush;
    for (ull i = 0; i < longcnt; i += 8) {
      // increment 1's and 2's; leave 3's alone
      // watch out for first element; the 0 in the first one is not a mistake
      ull v = mem[i];
      mem[i] = v + ((v ^ (v >> 1)) & 0x5555555555555550LL);
      v = mem[i + 1];
      mem[i + 1] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 2];
      mem[i + 2] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 3];
      mem[i + 3] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 4];
      mem[i + 4] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 5];
      mem[i + 5] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 6];
      mem[i + 6] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
      v = mem[i + 7];
      mem[i + 7] = v + ((v ^ (v >> 1)) & 0x5555555555555555LL);
    }
    if (quiet == 0)
      cout << "in " << duration() << endl << flush;
    wval--;
  }
  if (wval <= 0 && prediction < (size >> 9))
    wval = 0;
  else
    wval++;
  baseval++;
  filltable(pd, baseval + 1);
  writept(pd);
}
// if someone set options that affect the hash, we add a suffix to the
// data file name to reflect this.
void prunetable::addsumdat(const puzdef &pd, string &filename) const {
  ull t = pd.optionssum;
  if (inputbasename == UNKNOWNPUZZLE)
    t ^= pd.checksum;
  if (t == 0)
    return;
  filename.push_back('-');
  filename.push_back('o');
  while (t) {
    int v = t % 36;
    t /= 36;
    if (v < 10)
      filename.push_back('0' + v);
    else
      filename.push_back('a' + (v - 10));
  }
}

/*
 *   Shared-memory tables.  --shmhold NAME: this process creates the POSIX
 *   shared memory object /NAME (it must not exist), builds or reads the table
 *   in it (a 16 KB header page, then the table), writes the header with the
 *   magic last, prints a "ready" line and holds the object until SIGINT,
 *   SIGTERM or SIGHUP, which remove it.  --shmattach NAME maps it read-only
 *   and takes every field a lookup needs from the header; such a table is
 *   never filled, extended or written.  --shmremove NAME removes a leftover
 *   object (after a SIGKILL).
 */
const char *shmholdname, *shmattachname, *shmremovename;
#if !defined(_WIN32) && !defined(WASM)
static const ull SHMMAGIC = 0x7477736d74616231ULL; // "twsmtab1"
static const size_t SHMHEADER = 16384;
struct shmheader {
  ull magic, bytesize, size, shift1, shift2, memmul, fillcnt, totpop, ptotpop,
      popped, checksum, optionssum;
  int totsize, shardshift, baseval, hibase, wval, wbval;
};
static char shmownedname[256];
static void shmteardown(int) {
  if (shmownedname[0])
    shm_unlink(shmownedname);
  const char msg[] = "Shared memory table removed\n";
  ssize_t r = write(1, msg, sizeof(msg) - 1);
  (void)r;
  _exit(0);
}
static string shmname(const char *n) {
  string s = n;
  if (s.size() == 0 || s[0] != '/')
    s = "/" + s;
  return s;
}
void shmremove(const char *name) {
  string n = shmname(name);
  if (shm_unlink(n.c_str()) != 0)
    error("! could not remove shared memory ", n.c_str());
  cout << "Removed shared memory " << n << endl;
}
void prunetable::shmcreate(ull bytesize) {
  string n = shmname(shmholdname);
  int fd = shm_open(n.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    error("! could not create shared memory ", n.c_str());
  shmbytes = SHMHEADER + (bytesize >> 3) * sizeof(ull);
  if (ftruncate(fd, shmbytes) != 0) {
    close(fd);
    shm_unlink(n.c_str());
    error("! could not size shared memory ", n.c_str());
  }
  shmbase = mmap(0, shmbytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (shmbase == MAP_FAILED) {
    shmbase = 0;
    shm_unlink(n.c_str());
    error("! could not map shared memory ", n.c_str());
  }
  strncpy(shmownedname, n.c_str(), sizeof(shmownedname) - 1);
  signal(SIGINT, shmteardown);
  signal(SIGTERM, shmteardown);
  signal(SIGHUP, shmteardown);
  cout << "Created shared memory " << n << " of " << shmbytes
       << " bytes (held by this process; Control-C removes it)" << endl;
  amem = mem = (ull *)((char *)shmbase + SHMHEADER);
}
void prunetable::shmpublish(const puzdef &pd) {
  shmheader *hd = (shmheader *)shmbase;
  hd->bytesize = shmbytes - SHMHEADER;
  hd->size = size;
  hd->shift1 = shift1;
  hd->shift2 = shift2;
  hd->memmul = memmul;
  hd->fillcnt = fillcnt;
  hd->totpop = totpop;
  hd->ptotpop = ptotpop;
  hd->popped = popped;
  hd->checksum = pd.checksum;
  hd->optionssum = pd.optionssum;
  hd->totsize = totsize;
  hd->shardshift = shardshift;
  hd->baseval = baseval;
  hd->hibase = hibase;
  hd->wval = wval;
  hd->wbval = wbval;
  __sync_synchronize();
  hd->magic = SHMMAGIC;
  cout << "Shared memory table " << shmname(shmholdname) << " ready (baseval "
       << baseval << "); holding it until Control-C" << endl;
  while (1)
    pause();
}
void prunetable::shmattach(const puzdef &pd) {
  string n = shmname(shmattachname);
  int fd = shm_open(n.c_str(), O_RDONLY, 0);
  if (fd < 0)
    error("! could not open shared memory ", n.c_str());
  struct stat sb;
  if (fstat(fd, &sb) != 0 || (size_t)sb.st_size < SHMHEADER) {
    close(fd);
    error("! shared memory too small ", n.c_str());
  }
  shmbytes = sb.st_size;
  shmbase = mmap(0, shmbytes, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (shmbase == MAP_FAILED) {
    shmbase = 0;
    error("! could not map shared memory ", n.c_str());
  }
  const shmheader *hd = (const shmheader *)shmbase;
  if (hd->magic != SHMMAGIC)
    error("! shared memory table not ready: ", n.c_str());
  if (hd->checksum != pd.checksum || hd->optionssum != pd.optionssum ||
      hd->totsize != pd.totsize)
    error("! shared memory table is for another puzzle or options: ",
          n.c_str());
  if (SHMHEADER + hd->bytesize != shmbytes)
    error("! shared memory size mismatch ", n.c_str());
  size = hd->size;
  shift1 = hd->shift1;
  shift2 = hd->shift2;
  memmul = hd->memmul;
  fillcnt = hd->fillcnt;
  totpop = hd->totpop;
  ptotpop = hd->ptotpop;
  popped = hd->popped;
  totsize = hd->totsize;
  shardshift = hd->shardshift;
  baseval = hd->baseval;
  hibase = hd->hibase;
  wval = hd->wval;
  wbval = hd->wbval;
  lookupcnt = 0;
  justread = 1;
  shmreadonly = 1;
  amem = mem = (ull *)((char *)shmbase + SHMHEADER);
  cout << "Attached shared memory table " << n << " (" << shmbytes
       << " bytes, baseval " << baseval << ", read-only)" << endl;
}
void prunetable::shmrelease() {
  if (shmbase)
    munmap(shmbase, shmbytes);
  shmbase = 0;
  amem = mem = 0;
}
#else
static void shmunsupported() {
  error("! shared memory tables are not supported on Windows or in "
        "WebAssembly");
}
void shmremove(const char *) { shmunsupported(); }
void prunetable::shmcreate(ull) { shmunsupported(); }
void prunetable::shmpublish(const puzdef &) {}
void prunetable::shmattach(const puzdef &) { shmunsupported(); }
void prunetable::shmrelease() {}
#endif
