#ifndef PRUNETABLE_H
#include "puzdef.h"
#include "threads.h"
#include "workchunks.h"
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
const int SIGNATURE = 22 ; // start and end of data files
extern string inputbasename ;
extern int nowrite ;
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
const int BLOCKSIZE = 8192 ; // in long longs
const int FILLCHUNKS = 256 ; // long longs
struct fillbuf {
   int nchunks ;
   ull chunks[FILLCHUNKS] ;
} ;
struct fillworker {
   vector<allocsetval> posns ;
   int d ;
   fillbuf fillbufs[MEMSHARDS] ;
   char pad[256] ;
   void init(const puzdef &pd, int d_) ;
   ull fillstart(const puzdef &pd, prunetable &pt, int w) ;
   ull fillflush(const prunetable &pt, int shard) ;
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
   void init(struct prunetable *pt_, FILE *f_ = 0) ;
   void waitthread(int i) ;
   void queuepackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) ;
   void queueunpackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) ;
   void finishall() ;
   int nextthread ;
   struct prunetable *pt ;
   ioworkitem ioworkitems[MAXTHREADS] ;
   FILE *f ;
} ;
extern struct ioqueue ioqueue ;
struct prunetable {
   prunetable(const puzdef &pd, ull maxmem) ;
   void filltable(const puzdef &pd, int d) ;
   void checkextend(const puzdef &pd) ;
   int lookuph(ull h) const {
      h &= hmask ;
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 0)
         return mem[(h >> 5) & ~7] & 15 ;
      else
         return v + baseval - 1 ;
   }
   void prefetch(ull h) const {
      __builtin_prefetch(mem+((h & hmask) >> 5)) ;
   }
   int lookup(const setval sv) const {
      ull h = fasthash(totsize, sv) & hmask ;
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 0)
         return mem[(h >> 5) & ~7] & 15 ;
      else
         return v + baseval - 1 ;
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
   void readblock(ull *mem, ull explongcnt, FILE *f) ;
   void writept(const puzdef &pd) ;
   int readpt(const puzdef &pd) ;
   ull size, hmask, popped, totpop ;
   ull lookupcnt ;
   ull fillcnt ;
   ull *mem ;
   int totsize ;
   int shardshift ;
   int baseval, hibase ; // 0 is less; 1 is this; 2 is this+1; 3 is >=this+2
   int wval, wbval ;
   uchar codewidths[512] ;
   ull codevals[512] ;
   short *tabs[7] ;
   char justread ;
} ;
#define PRUNETABLE_H
#endif
