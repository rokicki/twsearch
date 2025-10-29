#include "prunetable.h"
#include "city.h"
#include <iostream>
#include <set>
int writeprunetables = 1; // default is auto
int startprunedepth = 3;
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
  int togo = d;
  while (initmoves > 1) {
    int mv = initmoves % nmoves;
    pd.mul(posns[sp], pd.moves[mv].pos, posns[sp + 1]);
    if (!pd.legalstate(posns[sp + 1]))
      return 0;
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
    if (!pd.legalstate(posns[sp + 1]))
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
prunetable::prunetable(const puzdef &pd, ull maxmem) {
  pdp = &pd;
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
  if (quiet == 0)
    cout << "For memsize " << maxmem << " sh1 " << shift1 << " mul " << memmul
         << " sh2 " << shift2 << " shardshift " << shardshift << endl;
  totpop = 0;
  ptotpop = 0;
  baseval = 0;
  wval = 0;
  cout << "Trying to allocate "
       << (CACHELINESIZE + (bytesize >> 3) * sizeof(ull)) << endl;
  amem = mem = (ull *)calloc(CACHELINESIZE + (bytesize >> 3) * sizeof(ull), 1);
  if (mem == 0)
    error("! could not allocate main memory buffer");
  // hack memalign
  while (((ull)mem) & (CACHELINESIZE - 1))
    mem++;
  lookupcnt = 0;
  fillcnt = 0;
  justread = 0;
  for (int i = 0; i < 7; i++)
    dtabs[i] = 0;
  if (!readpt(pd)) {
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
}
void prunetable::filltable(const puzdef &pd, int d) {
  popped = 0;
  wbval = min(d, 14);
  ll ofillcnt = fillcnt;
  if (quiet == 0)
    cout << "Filling depth " << d << " val " << wval << flush;
  workchunks = makeworkchunks(pd, d, pd.solved);
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
  double prediction = 0;
  if (ptotpop != 0)
    prediction = totpop * (double)totpop / ptotpop;
  if ((ignorelookup == 0 && lookupcnt < 3 * fillcnt) || baseval > 100 ||
      prediction > size || (pd.logstates <= 50 && prediction > pd.llstates))
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
