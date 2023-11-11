#ifndef SOLVE_H
#include "canon.h"
#include "generatingset.h"
#include "prunetable.h"
#include "puzdef.h"
#include "threads.h"
/*
 *   Routines to use iterated depth-first searching to solve a particular
 *   position (and the required code to distribute the work across
 *   multiple threads).
 */
extern ull solutionsfound, solutionsneeded;
extern int noearlysolutions;
extern int phase2;
extern int optmindepth;
extern int onlyimprovements;
extern int randomstart;
extern string lastsolution;
extern int globalinputmovecount;
struct solvestate {
  int st, mi;
  ull mask, skipbase;
};
const int MAXMICROTHREADING = 16;
extern int requesteduthreading;
extern int workinguthreading;
struct microthread {
  vector<allocsetval> posns;
  vector<solvestate> solvestates;
  vector<int> movehist;
  setval *looktmp;
  int sp, st, d, togo, finished, tid;
  ull h;
  void init(const puzdef &pd, int d_, int tid_, const setval p);
  void innersetup(prunetable &pt);
  int innerfetch(const puzdef &pd, prunetable &pt);
  int possibsolution(const puzdef &pd);
  int solvestart(const puzdef &pd, prunetable &pt, int w);
  int getwork(const puzdef &pd, prunetable &pt);
};
struct solveworker {
  long long lookups, checktarget, checkincrement;
  setval p;
  int d, numuthr, rover, tid;
  struct microthread uthr[MAXMICROTHREADING];
  char padding[256]; // kill false sharing
  void init(int d_, int tid_, const setval p);
  int solveiter(const puzdef &pd, prunetable &pt, const setval p);
};
extern solveworker solveworkers[MAXTHREADS];
extern int maxdepth, didprepass;
void setsolvecallback(int (*)(setval &, const vector<int> &, int, int),
                      int (*)(int));
int solve(const puzdef &pd, prunetable &pt, const setval p,
          generatingset *gs = 0);
#define SOLVE_H
#endif
