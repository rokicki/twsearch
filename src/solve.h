#ifndef SOLVE_H
#include "puzdef.h"
#include "prunetable.h"
#include "canon.h"
#include "threads.h"
#include "generatingset.h"
extern ull solutionsfound, solutionsneeded ;
extern int noearlysolutions ;
extern int phase2 ;
struct solveworker {
   vector<allocsetval> posns ;
   vector<int> movehist ;
   long long lookups ;
   int d ;
   char padding[256] ; // kill false sharing
   void init(const puzdef &pd, int d_, const setval &p) ;
   int solverecur(const puzdef &pd, prunetable &pt, int togo, int sp, int st) ;
   int solvestart(const puzdef &pd, prunetable &pt, int w) ;
   void dowork(const puzdef &pd, prunetable &pt) ;
} ;
extern solveworker solveworkers[MAXTHREADS] ;
extern int maxdepth ;
int solve(const puzdef &pd, prunetable &pt, const setval p, generatingset *gs=0) ;
#define SOLVE_H
#endif
