#ifndef SOLVE_H
#include "puzdef.h"
#include "prunetable.h"
#include "canon.h"
#include "threads.h"
#include "generatingset.h"
/*
 *   Routines to use iterated depth-first searching to solve a particular
 *   position (and the required code to distribute the work across
 *   multiple threads).
 */
extern ull solutionsfound, solutionsneeded ;
extern int noearlysolutions ;
extern int phase2 ;
extern int optmindepth ;
extern string lastsolution ;
struct solveworker {
   vector<allocsetval> posns ;
   vector<int> movehist ;
   long long lookups ;
   int d, id ;
   char padding[256] ; // kill false sharing
   void init(const puzdef &pd, int d_, int id_, const setval &p) ;
   int solverecur(const puzdef &pd, prunetable &pt, int togo, int sp, int st) ;
   int solvestart(const puzdef &pd, prunetable &pt, int w) ;
   void dowork(const puzdef &pd, prunetable &pt) ;
} ;
extern solveworker solveworkers[MAXTHREADS] ;
extern int maxdepth, didprepass ;
void setsolvecallback(int (*)(setval &, const vector<int> &, int, int),
                      int (*)(int)) ;
int solve(const puzdef &pd, prunetable &pt, const setval p, generatingset *gs=0) ;
#define SOLVE_H
#endif
