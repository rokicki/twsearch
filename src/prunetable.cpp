#include <iostream>
#include <fstream>
#include <set>
#include "prunetable.h"
#include "city.h"
fillworker fillworkers[MAXTHREADS] ;
struct ioqueue ioqueue ;
string inputbasename = "unknownpuzzle" ;
int nowrite ;
ull fasthash(int n, const setval sv) {
   ull r = CityHash64((const char *)sv.dat, n) ;
   // this little hack ensures that at least one of bits 1..7
   // (numbered from zero) is set.
   r ^= ((r | (1LL << 43)) & ((r & 0xfe) - 2)) >> 42 ;
   return r ;
}
vector<workerparam> workerparams ;
void setupparams(const puzdef &pd, prunetable &pt, int numthreads) {
   while ((int)workerparams.size() < numthreads) {
      int i = workerparams.size() ;
      workerparams.push_back(workerparam(pd, pt, i)) ;
   }
}
int setupthreads(const puzdef &pd, prunetable &pt) {
   int wthreads = min(numthreads, (int)workchunks.size()) ;
   workat = 0 ;
   setupparams(pd, pt, wthreads) ;
   return wthreads ;
}
void *fillthreadworker(void *o) {
   workerparam *wp = (workerparam *)o ;
   fillworkers[wp->tid].dowork(wp->pd, wp->pt) ;
   return 0 ;
}
void *unpackworker(void *o) {
   ioworkitem *wi = (ioworkitem *)o ;
   wi->pt->unpackblock(wi->mem, wi->longcnt, wi->buf, wi->bytecnt) ;
   free(wi->buf) ;
   return 0 ;
}
void *packworker(void *o) {
   ioworkitem *wi = (ioworkitem *)o ;
   wi->pt->packblock(wi->mem, wi->longcnt, wi->buf, wi->bytecnt) ;
   return 0 ;
}
void fillworker::init(const puzdef &pd, int d_) {
   while (posns.size() <= 100 || (int)posns.size() <= d_+1)
      posns.push_back(allocsetval(pd, pd.solved)) ;
   pd.assignpos(posns[0], pd.solved) ;
   d = d_ ;
   for (int i=0; i<MEMSHARDS; i++)
      fillbufs[i].nchunks = 0 ;
}
ull fillworker::fillstart(const puzdef &pd, prunetable &pt, int w) {
   ull initmoves = workchunks[w] ;
   int nmoves = pd.moves.size() ;
   int sp = 0 ;
   int st = 0 ;
   int togo = d ;
   while (initmoves > 1) {
      int mv = initmoves % nmoves ;
      pd.mul(posns[sp], pd.moves[mv].pos, posns[sp+1]) ;
      if (!pd.legalstate(posns[sp+1]))
         return 0 ;
      st = canonnext[st][pd.moves[mv].cs] ;
      sp++ ;
      togo-- ;
      initmoves /= nmoves ;
   }
   ull r = filltable(pd, pt, togo, sp, st) ;
   for (int i=0; i<MEMSHARDS; i++)
      r += fillflush(pt, i) ;
   return r ;
}
ull fillworker::fillflush(const prunetable &pt, int shard) {
   ull r = 0 ;
   fillbuf &fb = fillbufs[shard] ;
   if (fb.nchunks > 0) {
#ifdef USE_PTHREADS
      pthread_mutex_lock(&(memshards[shard].mutex)) ;
#endif
      for (int i=0; i<fb.nchunks; i++) {
         ull h = fb.chunks[i] ;
         if (((pt.mem[h>>5] >> (2*(h&31))) & 3) == 3) {
            pt.mem[h>>5] -= (3LL - pt.wval) << (2*(h&31)) ;
            if ((pt.mem[(h>>5)&-8] & 15) == 15)
               pt.mem[(h>>5)&-8] -= 15 - pt.wbval ;
            r++ ;
         }
      }
#ifdef USE_PTHREADS
      pthread_mutex_unlock(&(memshards[shard].mutex)) ;
#endif
      fb.nchunks = 0 ;
   }
   return r ;
}
void fillworker::dowork(const puzdef &pd, prunetable &pt) {
   while (1) {
      int w = -1 ;
      get_global_lock() ;
      if (workat < (int)workchunks.size())
         w = workat++ ;
      release_global_lock() ;
      if (w < 0)
         return ;
      ull cnt = fillstart(pd, pt, w) ;
      get_global_lock() ;
      pt.popped += cnt ;
      release_global_lock() ;
   }
}
ull fillworker::filltable(const puzdef &pd, prunetable &pt, int togo,
                          int sp, int st) {
   ull r = 0 ;
   if (togo == 0) {
      ull h = fasthash(pd.totsize, posns[sp]) & pt.hmask ;
      int shard = (h >> pt.shardshift) ;
      fillbuf &fb = fillbufs[shard] ;
      fb.chunks[fb.nchunks++] = h ;
      if (fb.nchunks >= FILLCHUNKS)
         r += fillflush(pt, shard) ;
      return r ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      if (!pd.legalstate(posns[sp+1]))
         continue ;
      r += filltable(pd, pt, togo-1, sp+1, ns[mv.cs]) ;
   }
   return r ;
}
void ioqueue::initin(struct prunetable *pt_, istream *f_) {
   pt = pt_ ;
   inf = f_ ;
   outf = 0 ;
   for (int i=0; i<numthreads; i++)
      ioworkitems[i].state = 0 ;
   nextthread = 0 ;
}
void ioqueue::initout(struct prunetable *pt_, ostream *f_) {
   pt = pt_ ;
   inf = 0 ;
   outf = f_ ;
   for (int i=0; i<numthreads; i++)
      ioworkitems[i].state = 0 ;
   nextthread = 0 ;
}
void ioqueue::waitthread(int i) {
#ifdef USE_PTHREADS
   join_thread(i) ;
#endif
   if (ioworkitems[i].state == 2) {
      unsigned int bytecnt = ioworkitems[i].bytecnt ;
      unsigned int longcnt = ioworkitems[i].longcnt ;
      outf->put(bytecnt & 255) ;
      outf->put((bytecnt >> 8) & 255) ;
      outf->put((bytecnt >> 16) & 255) ;
      outf->put((bytecnt >> 24) & 255) ;
      outf->put(longcnt & 255) ;
      outf->put((longcnt >> 8) & 255) ;
      outf->put((longcnt >> 16) & 255) ;
      outf->put((longcnt >> 24) & 255) ;
      outf->write((char *)ioworkitems[i].buf, bytecnt);
      if (outf->fail())
         error("! I/O error writing block") ;
      free(ioworkitems[i].buf) ;
   }
}
void ioqueue::queuepackwork(ull *mem, ull longcnt,
                     uchar *buf, unsigned int bytecnt) {
   if (ioworkitems[nextthread].state != 0) {
      waitthread(nextthread) ;
      ioworkitems[nextthread].state = 0 ;
   }
   ioworkitems[nextthread].mem = mem ;
   ioworkitems[nextthread].longcnt = longcnt ;
   ioworkitems[nextthread].buf = buf ;
   ioworkitems[nextthread].bytecnt = bytecnt ;
   ioworkitems[nextthread].pt = pt ;
   ioworkitems[nextthread].state = 2 ;
#ifdef USE_PTHREADS
   spawn_thread(nextthread, packworker, &ioworkitems[nextthread]) ;
#else
   packworker(&ioworkitems[nextthread]) ;
#endif
   nextthread++ ;
   if (nextthread >= numthreads)
      nextthread = 0 ;
}
void ioqueue::queueunpackwork(ull *mem, ull longcnt,
                     uchar *buf, unsigned int bytecnt) {
   if (ioworkitems[nextthread].state != 0) {
      waitthread(nextthread) ;
      ioworkitems[nextthread].state = 0 ;
   }
   ioworkitems[nextthread].mem = mem ;
   ioworkitems[nextthread].longcnt = longcnt ;
   ioworkitems[nextthread].buf = buf ;
   ioworkitems[nextthread].bytecnt = bytecnt ;
   ioworkitems[nextthread].pt = pt ;
   ioworkitems[nextthread].state = 1 ;
#ifdef USE_PTHREADS
   spawn_thread(nextthread, unpackworker, &ioworkitems[nextthread]) ;
#else
   unpackworker(&ioworkitems[nextthread]) ;
#endif
   nextthread++ ;
   if (nextthread >= numthreads)
      nextthread = 0 ;
}
void ioqueue::finishall() {
   for (int i=0; i<numthreads; i++) {
      if (ioworkitems[nextthread].state != 0)
         waitthread(nextthread) ;
      nextthread = (nextthread + 1) % numthreads ;
   }
}
prunetable::prunetable(const puzdef &pd, ull maxmem) {
   totsize = pd.totsize ;
   ull bytesize = 16 ;
   while (2 * bytesize <= maxmem &&
          (pd.logstates > 55 || 8 * bytesize < pd.llstates))
      bytesize *= 2 ;
   size = bytesize * 4 ;
   shardshift = 0 ;
   while ((size >> shardshift) > MEMSHARDS)
      shardshift++ ;
   hmask = size - 1 ;
   totpop = 0 ;
   int base = 1 ;
   while (base + 2 < (int)canontotcnt.size() && canontotcnt[base+2] < size)
      base++ ;
   // hack memalign
   mem = (ull *)malloc(CACHELINESIZE + (bytesize >> 3) * sizeof(ull)) ;
   while (((ull)mem) & (CACHELINESIZE - 1))
      mem++ ;
   lookupcnt = 0 ;
   fillcnt = 0 ;
   hibase = base ;
   justread = 0 ;
   if (!readpt(pd)) {
      memset(mem, -1, bytesize) ;
      baseval = min(hibase, 2) ;
      for (int d=0; d<=baseval+1; d++) {
         int val = 0 ;
         if (d >= baseval)
            val = d - baseval + 1 ;
         wval = val ;
         wbval = min(d, 15) ;
         filltable(pd, d) ;
      }
   }
   writept(pd) ;
}
void prunetable::filltable(const puzdef &pd, int d) {
   popped = 0 ;
   cout << "Filling table at depth " << d << " with val " << wval << flush ;
   makeworkchunks(pd, d) ;
   int wthreads = setupthreads(pd, *this) ;
   for (int t=0; t<wthreads; t++)
      fillworkers[t].init(pd, d) ;
#ifdef USE_PTHREADS
   for (int i=0; i<wthreads; i++)
      spawn_thread(i, fillthreadworker, &(workerparams[i])) ;
   for (int i=0; i<wthreads; i++)
      join_thread(i) ;
#else
   fillthreadworker((void *)&workerparams[0]) ;
#endif
   fillcnt += canonseqcnt[d] ;
   cout << " saw " << popped << " (" << canonseqcnt[d] << ") in "
        << duration() << endl << flush ;
   totpop += popped ;
   justread = 0 ;
}
void prunetable::checkextend(const puzdef &pd) {
   if (lookupcnt < 3 * fillcnt || baseval > 100 || totpop * 2 > size ||
       baseval > hibase ||
       (pd.logstates <= 50 && canonseqcnt[baseval+2] > pd.llstates))
      return ;
   ull longcnt = (size + 31) >> 5 ;
   cout << "Demoting memory values " << flush ;
   for (ull i=0; i<longcnt; i += 8) {
      // decrement 1's and 2's; leave 3's alone
      // watch out for first element; the 0 in the first one is not a mistake
      ull v = mem[i] ;
      mem[i] = v - ((v ^ (v >> 1)) & 0x5555555555555550LL) ;
      v = mem[i+1] ;
      mem[i+1] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+2] ;
      mem[i+2] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+3] ;
      mem[i+3] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+4] ;
      mem[i+4] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+5] ;
      mem[i+5] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+6] ;
      mem[i+6] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      v = mem[i+7] ;
      mem[i+7] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
   }
   cout << "in " << duration() << endl << flush ;
   baseval++ ;
   wval = 2 ;
   wbval = min(15, baseval+1) ;
   filltable(pd, baseval+1) ;
   writept(pd) ;
}
// if someone set options that affect the hash, we add a suffix to the
// data file name to reflect this.
void prunetable::addsumdat(const puzdef &pd, string &filename) const {
   filename.push_back('-') ;
   filename.push_back('o') ;
   ull t = pd.optionssum ;
   while (t) {
      int v = t % 36 ;
      t /= 36 ;
      if (v < 10)
         filename.push_back('0'+v) ;
      else
         filename.push_back('a'+(v-10)) ;
   }
}
string prunetable::makefilename(const puzdef &pd) const {
   string filename = "tws2-" + inputbasename + "-" ;
   if (quarter)
      filename += "q-" ;
   ull bytes = size >> 2 ;
   char suffix = 0 ;
   if (bytes >= 1024) {
      suffix = 'K' ;
      bytes >>= 10 ;
   }
   if (bytes >= 1024) {
      suffix = 'M' ;
      bytes >>= 10 ;
   }
   if (bytes >= 1024) {
      suffix = 'G' ;
      bytes >>= 10 ;
   }
   if (bytes >= 1024) {
      suffix = 'T' ;
      bytes >>= 10 ;
   }
   filename += to_string(bytes) ;
   if (suffix)
      filename += suffix ;
   if (pd.optionssum)
      addsumdat(pd, filename) ;
   filename += ".dat" ;
   return filename ;
}
ull prunetable::calcblocksize(ull *mem, ull longcnt) {
   ull bits = 0 ;
   for (ull i=0; i<longcnt; i++) {
      ull v = mem[i] ;
      for (int j=0; j<8; j++) {
         bits += codewidths[v & 255] ;
         v >>= 8 ;
      }
   }
   return ((bits + 7) >> 3) ;
}
void prunetable::packblock(ull *mem, ull longcnt, uchar *buf, ull bytecnt) {
   ull accum = 0 ;
   int havebits = 0 ;
   ull bytectr = 0 ;
   for (ull i=0; i<longcnt; i++) {
      ull v = mem[i] ;
      for (int j=0; j<8; j++) {
         int cp = v & 255 ;
         int cpw = codewidths[cp] ;
         if (cpw == 0)
            error("! internal error in Huffman encoding") ;
         while (havebits + cpw > 64) {
            buf[bytectr++] = accum >> (havebits - 8) ;
            if (bytectr > bytecnt)
               error("! packing issue") ;
            havebits -= 8 ;
         }
         accum = (accum << cpw) + codevals[cp] ;
         havebits += cpw ;
         v >>= 8 ;
      }
   }
   int extra = (8 - havebits) & 7 ;
   havebits += extra ;
   accum <<= extra ;
   while (havebits > 0) {
      buf[bytectr++] = accum >> (havebits - 8) ;
      if (bytectr > bytecnt)
         error("! packing issue 2") ;
      havebits -= 8 ;
   }
   if (bytectr != bytecnt)
      error("! packing issue 3") ;
}
void prunetable::unpackblock(ull *mem, ull longcnt, uchar *block, int bytecnt) {
   int bytectr = 0 ;
   int havebits = 0 ;
   ull accum = 0 ;
   for (ull i=0; i<longcnt; i++) {
      ull v = 0 ;
      for (int j=0; j<8; j++) {
         int bitsneeded = 8 ;
         int k = 0 ;
         while (1) {
            if (havebits < bitsneeded) {
               int c = 0 ;
               if (bytectr < bytecnt)
                  c = block[bytectr++] ;
               accum = (accum << 8) + c ;
               havebits += 8 ;
            }
            int cp = tabs[k][accum >> (havebits - bitsneeded)] ;
            if (cp >= 0) {
               v += ((ull)cp) << (8 * j) ;
               havebits -= codewidths[cp] ;
               if (havebits > 14)
                  error("! oops; should not have this many bits left") ;
               accum &= ((1LL << havebits) - 1) ;
               break ;
            }
            bitsneeded += 8 ;
            k++ ;
            if (k >= 7)
               error("! failure while decoding") ;
         }
      }
      mem[i] = v ;
   }
   if (bytecnt != bytectr)
      error("! error when unpacking bytes") ;
}
void prunetable::writeblock(ull *mem, ull longcnt) {
   ull bytecnt = calcblocksize(mem, longcnt) ;
   uchar *buf = (uchar *)malloc(bytecnt) ;
   ioqueue.queuepackwork(mem, longcnt, buf, bytecnt) ;
}
void prunetable::readblock(ull *mem, ull explongcnt, istream *inf) {
   unsigned int bytecnt, longcnt ;
   bytecnt = inf->get() ;
   bytecnt += inf->get() << 8 ;
   bytecnt += inf->get() << 16 ;
   bytecnt += inf->get() << 24 ;
   longcnt = inf->get() ;
   longcnt += inf->get() << 8 ;
   longcnt += inf->get() << 16 ;
   longcnt += inf->get() << 24 ;
   if (longcnt != explongcnt || bytecnt <= 0 || bytecnt > 32 * BLOCKSIZE)
      error("! I/O error while reading block") ;
   uchar *buf = (uchar *)malloc(bytecnt) ;
   inf->read((char *)buf, bytecnt);
   if (inf->fail())
      error("! I/O error while reading block") ;
   ioqueue.queueunpackwork(mem, longcnt, buf, bytecnt) ;
}
void prunetable::writept(const puzdef &pd) {
   // only write the table if at least 1 in 100 elements has a value
   if (nowrite || justread || fillcnt * 100 < size)
      return ;
   ll longcnt = (size + 31) >> 5 ;
   if (longcnt % BLOCKSIZE != 0)
      return ; // too small
   // this *could* be calculated more efficiently, but the runtime is
   // dominated by scanning the array so we use simple code.
   // We use optimal huffman coding; for tables that fit on real
   // machines, this should probably never exceed a code length of
   // 56-bits, so we don't use the more complicated length-limited
   // coding.  We use 56-bits so we can use a 64-bit accumulator and
   // still shift things out in byte-sized chunks.
   ll bytecnts[256] ;
   for (int i=0; i<256; i++)
      bytecnts[i] = 0 ;
   cout << "Scanning memory for compression information " << flush ;
   for (ll i=0; i<longcnt; i++) {
      ull v = mem[i] ;
      for (int j=0; j<8; j++) {
         bytecnts[v & 255]++ ;
         v >>= 8 ;
      }
   }
   cout << "in " << duration() << endl << flush ;
   set<pair<ll, int> > codes ;
   vector<pair<int, int> > tree ; // binary tree
   vector<int> depths ; // max depths
   for (int i=0; i<256; i++)
      if (bytecnts[i])
         codes.insert(make_pair(bytecnts[i], i)) ;
   int nextcode = 256 ;
   int maxwidth = 0 ;
   ull bitcost = 0 ;
   while (codes.size() > 1) { // take out least two and insert sum
      auto a = *(codes.begin()) ;
      codes.erase(a) ;
      auto b = *(codes.begin()) ;
      codes.erase(b) ;
      tree.push_back(make_pair(a.second, b.second)) ;
      int dep = 1 ;
      if (a.second >= 256)
         dep = 1 + depths[a.second-256] ;
      if (b.second >= 256)
         dep = max(dep, 1 + depths[b.second-256]) ;
      maxwidth = max(maxwidth, dep) ;
      if (maxwidth > 56)
         error("! exceeded maxwidth in Huffman encoding; fix the code") ;
      depths.push_back(dep) ;
      codes.insert(make_pair(a.first+b.first, nextcode)) ;
      bitcost += a.first + b.first ;
      nextcode++ ;
   }
   cout << "Encoding; max width is " << maxwidth << " bitcost "
      << bitcost << " compression " << ((64.0 * longcnt) / bitcost)
      << " in " << duration() << endl ;
   codewidths[nextcode-1] = 0 ;
   codevals[nextcode-1] = 0 ;
   for (int i=0; i<256; i++) {
      codewidths[i] = 0 ;
      codevals[i] = 0 ;
   }
   int widthcounts[64] ;
   for (int i=0; i<64; i++)
      widthcounts[i] = 0 ;
   codewidths[nextcode-1] = 0 ;
   for (int i=nextcode-1; i>=256; i--) {
      int a = tree[i-256].first ;
      int b = tree[i-256].second ;
      codewidths[a] = codewidths[i] + 1 ;
      codewidths[b] = codewidths[i] + 1 ;
   }
   for (int i=0; i<256; i++)
      widthcounts[codewidths[i]]++ ;
   ull widthbases[64] ;
   ull at = 0 ;
   for (int i=63; i>0; i--) {
      if (widthcounts[i]) {
         widthbases[i] = at >> (maxwidth - i) ;
         at += ((ull)widthcounts[i]) << (maxwidth - i) ;
      }
   }
   if (at != (1ULL << maxwidth))
      error("! Bad calculation in codes") ;
   for (int i=0; i<256; i++)
      if (codewidths[i]) {
         codevals[i] = widthbases[codewidths[i]] ;
         widthbases[codewidths[i]]++ ;
      }
   string filename = makefilename(pd) ;
   cout << "Writing " << filename << " " << flush ;
   ofstream w;
   // do stuff
   w.open(filename, ios::out | ios::trunc) ;
   w.put(SIGNATURE);
   w.write((char *)&pd.checksum, sizeof(pd.checksum)) ;
   w.write((char *)&size, sizeof(size)) ;
   w.write((char *)&hmask, sizeof(hmask)) ;
   w.write((char *)&popped, sizeof(popped)) ;
   w.write((char *)&totpop, sizeof(totpop)) ;
   w.write((char *)&fillcnt, sizeof(fillcnt)) ;
   w.write((char *)&totsize, sizeof(totsize)) ;
   w.write((char *)&baseval, sizeof(baseval)) ;
   w.write((char *)&hibase, sizeof(hibase)) ;
   w.write((char *)codewidths, sizeof(codewidths[0]) * 256) ;
   if (longcnt % BLOCKSIZE != 0)
      error("Size must be a multiple of block size") ;
   ioqueue.initout(this, &w) ;
   for (ll i=0; i<longcnt; i += BLOCKSIZE)
      writeblock(mem+i, BLOCKSIZE) ;
   ioqueue.finishall() ;
   w.put(SIGNATURE);
   w.close() ;
   if (w.fail())
      error("! I/O error") ;
   cout << "written in " << duration() << endl << flush ;
}
int prunetable::readpt(const puzdef &pd) {
   for (int i=0; i<256; i++) {
      codewidths[i] = 0 ;
      codevals[i] = 0 ;
   }
   string filename = makefilename(pd) ;
   ifstream r ;
   r.open(filename, ifstream::in);
   if (r.fail())
      return 0 ;
   cout << "Reading " << filename << " " << flush ;
   if (r.get() != SIGNATURE) {
      warn("! first byte not signature") ;
      return 0 ;
   }
   ull checksum = 0 ;
   r.read((char *)&checksum, sizeof(checksum));
   if (r.fail())
      error("! I/O error reading pruning table") ;
   if (checksum != pd.checksum) {
      cout <<
 "Puzzle definition appears to have changed; recreating pruning table" << endl ;
      r.close() ;
      return 0 ;
   }
   ull temp = 0 ;
   r.read((char *)&temp, sizeof(temp));
      // error("! I/O error in reading pruning table") ;
   if (temp != size) {
      cout <<
 "Pruning table size is different; recreating pruning table" << endl ;
      r.close() ;
      return 0 ;
   }
   r.read((char *)&hmask, sizeof(hmask));
   r.read((char *)&popped, sizeof(popped));
   r.read((char *)&totpop, sizeof(totpop));
   r.read((char *)&fillcnt, sizeof(fillcnt));
   r.read((char *)&totsize, sizeof(totsize));
   r.read((char *)&baseval, sizeof(baseval));
   r.read((char *)&hibase, sizeof(hibase));
   r.read((char *)codewidths, sizeof(codewidths[0]) * 256);
   if (r.fail()) {
      warn("I/O error in reading pruning table") ;
      r.close() ;
      return 0 ;
   }
   int widthcounts[64] ;
   for (int i=0; i<64; i++)
      widthcounts[i] = 0 ;
   int maxwidth = 1 ;
   for (int i=0; i<256; i++) {
      if (codewidths[i] >= 56)
         error("! bad code widths in pruning table file") ;
      maxwidth = max(maxwidth, (int)codewidths[i]) ;
      widthcounts[codewidths[i]]++ ;
   }
   ull widthbases[64] ;
   ull at = 0 ;
   for (int i=63; i>0; i--) {
      if (widthcounts[i]) {
         widthbases[i] = at >> (maxwidth - i) ;
         at += ((ull)widthcounts[i]) << (maxwidth - i) ;
      }
   }
   if (at != (1ULL << maxwidth))
      error("! Bad codewidth sum in codes") ;
   for (int i=0; i<256; i++)
      if (codewidths[i]) {
         codevals[i] = widthbases[codewidths[i]] ;
         widthbases[codewidths[i]]++ ;
      }
   at = 0 ; // restore the widthbases
   int theight[7] ;
   for (int i=63; i>0; i--) {
      if (widthcounts[i]) {
         widthbases[i] = at >> (maxwidth - i) ;
         at += ((ull)widthcounts[i]) << (maxwidth - i) ;
      }
      if ((i & 7) == 1) {
         int t = maxwidth - i - 7 ;
         if (t < 0) {
            theight[i>>3] = (at << -t) ;
         } else {
            theight[i>>3] = (at + (1LL << t) - 1) >> t ;
         }
      }
   }
   for (int i=0; i<7; i++)
      if (theight[i]) {
         tabs[i] = (short *)malloc(theight[i] * sizeof(short)) ;
         memset(tabs[i], -1, theight[i] * sizeof(short)) ;
      }
   at = 0 ;
   int twidth = (maxwidth + 7) & -8 ;
   for (int i=63; i>0; i--) {
      if (widthcounts[i]) {
         for (int cp=0; cp<256; cp++)
            if (codewidths[cp] == i) {
               int k = (i - 1) >> 3 ;
               int incsh = twidth-8*k-8 ;
               ull inc = 1LL << incsh ;
               ull nextat = at + (1LL << (twidth - i)) ;
               while (at < nextat) {
                  tabs[k][at>>incsh] = cp ;
                  at += inc ;
               }
               at = nextat ;
            }
      }
   }
   ll longcnt = (size + 31) >> 5 ;
   if (longcnt % BLOCKSIZE != 0)
      error("! when reading, expected multiple of longcnt") ;
   ioqueue.initin(this, &r) ;
   for (ll i=0; i<longcnt; i += BLOCKSIZE)
      readblock(mem+i, BLOCKSIZE, &r) ;
   ioqueue.finishall() ;
   int tv = r.get() ;
   if (tv != SIGNATURE)
      error("! I/O error reading final signature") ;
   r.close() ;
   cout << "read in " << duration() << endl << flush ;
   justread = 1 ;
   return 1 ;
}
void *unpackworker(void *o) ;
void *packworker(void *o) ;
