#ifndef PRUNETABLE_H
#include "canon.h"
#include "puzdef.h"
#include "rotations.h"
#include "threads.h"
#include "workchunks.h"
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
const int CACHELINESIZE = 64;
const int COMPSIGNATURE = 23;   // start and end of data files
const int UNCOMPSIGNATURE = 24; // start and end of data files
#define UNKNOWNPUZZLE "unknownpuzzle"
extern string inputbasename;
extern int writeprunetables; // 0=always 1=auto 2=never
extern int startprunedepth;
ull fasthash(int n, const setval sv);
struct prunetable;
struct workerparam {
  workerparam(const puzdef &pd_, prunetable &pt_, int tid_)
      : pd(pd_), pt(pt_), tid(tid_) {}
  const puzdef &pd;
  prunetable &pt;
  int tid;
};
int setupthreads(const puzdef &pd, prunetable &pt, vector<ull> &workchunks,
                 vector<workerparam> &workerparams);
const int BLOCKSIZE = 32768; // in long longs
const int FILLCHUNKS = 256;  // long longs
struct fillbuf {
  int nchunks;
  ull chunks[FILLCHUNKS];
};
struct fillworker {
  vector<allocsetval> posns;
  int d;
  fillbuf fillbufs[MEMSHARDS];
  allocsetval *looktmp;
  char pad[256];
  void init(const puzdef &pd, int d_);
  ull fillstart(const puzdef &pd, prunetable &pt, int w);
  ull fillflush(prunetable &pt, int shard);
  void dowork(const puzdef &pd, prunetable &pt);
  ull filltable(const puzdef &pd, prunetable &pt, int togo, int sp, int st);
};
extern fillworker fillworkers[MAXTHREADS];
struct ioworkitem {
  char state;
  ull *mem;
  ull longcnt;
  uchar *buf;
  prunetable *pt;
  unsigned int bytecnt;
};
struct ioqueue {
  void initin(struct prunetable *pt_, istream *f_ = 0);
  void initout(struct prunetable *pt_, ostream *f_ = 0);
  void waitthread(int i);
  void queuepackwork(ull *mem, ull longcnt, uchar *buf, unsigned int bytecnt);
  void queueunpackwork(ull *mem, ull longcnt, uchar *buf, unsigned int bytecnt);
  void finishall();
  int nextthread;
  struct prunetable *pt;
  ioworkitem ioworkitems[MAXTHREADS];
  istream *inf;
  ostream *outf;
};
extern struct ioqueue ioqueue;
struct decompinfo {
  unsigned int d;
  uchar bitwidth, bytewidth;
};
struct prunetable {
  prunetable() {
    amem = 0;
    mem = 0;
    for (int i = 0; i < 7; i++)
      dtabs[i] = 0;
  }
  prunetable(const puzdef &pd, ull maxmem);
  prunetable(const prunetable &) = delete;
  prunetable(prunetable &&) noexcept = delete;
  prunetable &operator=(const prunetable &) = delete;
  prunetable &operator=(prunetable &&) noexcept = delete;
  void filltable(const puzdef &pd, int d);
  void checkextend(const puzdef &pd, int ignorelookups = 0);
  int lookuphindexed(ull h) const {
    int v = 3 & (mem[h >> 5] >> ((h & 31) * 2));
    if (v == 3)
      return (mem[(h >> 5) & ~7] & 15) - 1;
    else
      return 2 - v + baseval;
  }
  ull prefetchindexed(ull h) const {
    prefetch(mem + (h >> 5));
    return h;
  }
  ull indexhash(ull lowb) const {
    ull h = lowb;
    h -= h >> subshift;
    h >>= memshift;
    h ^= 0xff & ((((h & 0xfe) - 2) >> 8) & (lowb | 2));
    return h;
  }
  ull indexhash(int n, const setval sv) const {
    return indexhash(fasthash(n, sv));
  }
  ull gethashforlookup(const setval sv, setval *looktmp) const {
    if ((int)pdp->rotgroup.size() > 1) {
      slowmodm2(*pdp, sv, *looktmp);
      return indexhash(totsize, *looktmp);
    } else {
      return indexhash(totsize, sv);
    }
  }
  int lookup(const setval sv, setval *looktmp) const {
    ull h = gethashforlookup(sv, looktmp);
    return lookuphindexed(h);
  }
  void addlookups(ull lookups) { lookupcnt += lookups; }
  ~prunetable() {
    if (amem) {
      free(amem);
      amem = 0;
      mem = 0;
    }
    for (int i = 0; i < 7; i++)
      if (dtabs[i]) {
        free(dtabs[i]);
        dtabs[i] = 0;
      }
  }
  // if someone set options that affect the hash, we add a suffix to the
  // data file name to reflect this.
  void addsumdat(const puzdef &pd, string &filename) const;
  string makefilename(const puzdef &pd, bool create_dirs) const;
  ull calcblocksize(ull *mem, ull longcnt);
  void packblock(ull *mem, ull longcnt, uchar *buf, ull bytecnt);
  void unpackblock(ull *mem, ull longcnt, uchar *block, int bytecnt);
  void writeblock(ull *mem, ull longcnt);
  void readblock(ull *mem, ull explongcnt, istream *f);
  void writept(const puzdef &pd);
  int readpt(const puzdef &pd);
  const puzdef *pdp;
  ull size, popped, totpop, ptotpop;
  ull subshift, memshift;
  ull lookupcnt;
  ull fillcnt;
  ull *mem, *amem;
  int totsize;
  int shardshift;
  int baseval, hibase; // 0 is less; 1 is this; 2 is this+1; 3 is >=this+2
  int wval, wbval;
  uchar codewidths[544];
  ull codevals[544];
  decompinfo *dtabs[7];
  char justread;
  vector<ull> workchunks;
  vector<workerparam> workerparams;
  int workat;
};
#define PRUNETABLE_H
#endif
