#ifndef PRUNETABLE_H
#include "puzdef.h"
#include "threads.h"
#include "workchunks.h"
#include "rotations.h"
#include "canon.h"
/*
 *   This code supports pruning tables for arbitrary puzzles.  Memory
 *   consumption is a power of two.  Each table entry is two bits.
 *   There is an in-cache-line sub-table with four bits.  So each
 *   64-byte cache line has a 4-bit subentry and 510 "regular" entries.
 *
 *   For the two-bit entries, 0 means look at the subentry, 1 means the
 *   value is the current base, 2 means the value is base+1, and 3
 *   means the value is at least base+2.  The base changes as the
 *   pruning table is filled more, level by level.
 *
 *   The hash table permits collisions (stores the minimum).
 */
const int CACHELINESIZE = 64 ;
const int COMPSIGNATURE = 23 ; // start and end of data files
const int UNCOMPSIGNATURE = 24 ; // start and end of data files
extern string inputbasename ;
extern int nowrite ;
extern int startprunedepth ;
ull fasthash(int n, const setval sv) ;
struct prunetable ;
struct workerparam {
   workerparam(const puzdef &pd_, prunetable &pt_, int tid_) :
      pd(pd_), pt(pt_), tid(tid_) {}
   const puzdef &pd ;
   prunetable &pt ;
   int tid ;
} ;
extern vector<workerparam> workerparams ;
int setupthreads(const puzdef &pd, prunetable &pt) ;
const int BLOCKSIZE = 32768 ; // in long longs
const int FILLCHUNKS = 256 ; // long longs
struct fillbuf {
   int nchunks ;
   ull chunks[FILLCHUNKS] ;
} ;
struct fillworker {
   vector<allocsetval> posns ;
   int d ;
   fillbuf fillbufs[MEMSHARDS] ;
   allocsetval *looktmp ;
   char pad[256] ;
   void init(const puzdef &pd, int d_) ;
   ull fillstart(const puzdef &pd, prunetable &pt, int w) ;
   ull fillflush(prunetable &pt, int shard) ;
   void dowork(const puzdef &pd, prunetable &pt) ;
   ull filltable(const puzdef &pd, prunetable &pt, int togo, int sp, int st) ;
} ;
extern fillworker fillworkers[MAXTHREADS] ;
struct ioworkitem {
   char state ;
   ull *mem ;
   ull longcnt ;
   uchar *buf ;
   prunetable *pt ;
   unsigned int bytecnt ;
} ;
struct ioqueue {
   void initin(struct prunetable *pt_, istream *f_ = 0) ;
   void initout(struct prunetable *pt_, ostream *f_ = 0) ;
   void waitthread(int i) ;
   void queuepackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) ;
   void queueunpackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) ;
   void finishall() ;
   int nextthread ;
   struct prunetable *pt ;
   ioworkitem ioworkitems[MAXTHREADS] ;
   istream *inf ;
   ostream *outf ;
} ;
extern struct ioqueue ioqueue ;
struct prunetable {
   prunetable() {}
   prunetable(const puzdef &pd, ull maxmem) ;
   void filltable(const puzdef &pd, int d) ;
   void checkextend(const puzdef &pd, int ignorelookups=0) ;
   int lookuph(ull h) const {
      h = indexhash(h) ;
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 3)
         return (mem[(h >> 5) & ~7] & 15) - 1 ;
      else
         return 2 - v + baseval ;
   }
   void prefetch(ull h) const {
      __builtin_prefetch(mem+((indexhash(h)) >> 5)) ;
   }
   ull indexhash(ull lowb) const {
      ull h = lowb ;
      h -= h >> subshift ;
      h >>= memshift ;
      h ^= 0xff & ((((h & 0xfe) - 2) >> 8) & (lowb | 2)) ;
      return h ;
   }
   ull indexhash(int n, const setval sv) const {
      return indexhash(fasthash(n, sv)) ;
   }
   int lookup(const setval sv, setval *looktmp) const {
      ull h ;
      if ((int)pdp->rotgroup.size() > 1) {
         slowmodm2(*pdp, sv, *looktmp) ;
         h = indexhash(totsize, *looktmp) ;
      } else {
         h = indexhash(totsize, sv) ;
      }
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 3)
         return (mem[(h >> 5) & ~7] & 15) - 1 ;
      else
         return 2 - v + baseval ;
   }
   void addlookups(ull lookups) {
      lookupcnt += lookups ;
   }
   // if someone set options that affect the hash, we add a suffix to the
   // data file name to reflect this.
   void addsumdat(const puzdef &pd, string &filename) const ;
   string makefilename(const puzdef &pd) const ;
   ull calcblocksize(ull *mem, ull longcnt) ;
   void packblock(ull *mem, ull longcnt, uchar *buf, ull bytecnt) ;
   void unpackblock(ull *mem, ull longcnt, uchar *block, int bytecnt) ;
   void writeblock(ull *mem, ull longcnt) ;
   void readblock(ull *mem, ull explongcnt, istream *f) ;
   void writept(const puzdef &pd) ;
   int readpt(const puzdef &pd) ;
   const puzdef *pdp ;
   ull size, popped, totpop, ptotpop ;
   ull subshift, memshift ;
   ull lookupcnt ;
   ull fillcnt ;
   ull *mem ;
   int totsize ;
   int shardshift ;
   int baseval, hibase ; // 0 is less; 1 is this; 2 is this+1; 3 is >=this+2
   int wval, wbval ;
   uchar codewidths[544] ;
   ull codevals[544] ;
   short *tabs[7] ;
   char justread ;
} ;
#define PRUNETABLE_H
#endif
