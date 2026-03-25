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
struct solvestate {
  int st, mi;
  ull mask, skipbase;
};
const int MAXMICROTHREADING = 16;
/*
 *   All parameters that control a solve.  A single global instance g_opts
 *   is written by command-line parsing and other setup code; solve()
 *   snapshots it into solvecontext::opts at the start of each invocation.
 *   This keeps every read during the search local to the context, enabling
 *   future concurrent solves with independent option sets.
 */
struct solveoptions {
  ll solutionsneeded = 1;
  int noearlysolutions = 0;
  int onlyimprovements = 0;
  int alloptimal = 0;
  int phase2 = 0;
  int optmindepth = 0;
  int maxdepth = 1000000000;
  int requesteduthreading = 4;
  int didprepass = 0;
  int randomstart = 0;
  int globalinputmovecount = 0;
  int (*callback)(setval &, const vector<int> &, int, int) = nullptr;
  int (*flushback)(int) = nullptr;
};
extern solveoptions g_opts;

struct solvecontext; // forward declaration

struct microthread {
  vector<allocsetval> posns;
  vector<solvestate> solvestates;
  vector<int> movehist;
  setval *looktmp = nullptr, *invtmp = nullptr;
  int sp, st, d, togo, finished, tid, invflag;
  ull h;
  long long extraprobes, lookups;
  solvecontext *ctx = nullptr;
  void init(const puzdef &pd, int d_, int tid_, const setval p);
  void innersetup(prunetable &pt);
  int innerfetch(const puzdef &pd, prunetable &pt);
  int possibsolution(const puzdef &pd);
  int solvestart(const puzdef &pd, prunetable &pt, int w);
  int getwork(const puzdef &pd, prunetable &pt);
};
struct solveworker {
  long long checktarget, checkincrement;
  setval p;
  int d, numuthr, rover, tid;
  struct microthread uthr[MAXMICROTHREADING];
  char padding[256]; // kill false sharing
  solvecontext *ctx = nullptr;
  void init(int d_, int tid_, const setval p, solvecontext *ctx_);
  int solveiter(const puzdef &pd, prunetable &pt, const setval p);
};
/*
 *   All mutable state for a single solve invocation.  Bundling this into
 *   a struct (rather than keeping it in globals) means two concurrent
 *   solves — e.g. in a multi-phase solver — each own their state with no
 *   interference.  Shared mutable fields (solutionsfound, workchunks, etc.)
 *   are still protected by the process-wide get_global_lock(), which also
 *   covers cout/cerr output.
 */
struct solvecontext {
  solveoptions opts; // snapshotted from g_opts at solve() entry

  // Mutable shared state, protected by get_global_lock()/release_global_lock()
  ll solutionsfound;
  string lastsolution;
  vector<ull> workchunks;
  int workat;

  // Per-solve infrastructure (single-writer before threads start)
  int workinguthreading;
  solveworker workers[MAXTHREADS];
  vector<workerparam> workerparams;
  vector<vector<int>> randomized;

  solvecontext() : solutionsfound(0), workat(0), workinguthreading(0) {}

  solvecontext(const solvecontext &) = delete;
  solvecontext &operator=(const solvecontext &) = delete;

  bool satisfied() const {
    return opts.alloptimal == 0 && solutionsfound >= opts.solutionsneeded;
  }
};

void setsolvecallback(int (*)(setval &, const vector<int> &, int, int),
                      int (*)(int));
int solve(const puzdef &pd, prunetable &pt, const setval p,
          generatingset *gs = 0);
void solveit(const puzdef &pd, prunetable &pt, string scramblename, setval &p,
             generatingset *gs = 0);
void solveitp2(const puzdef &pd, prunetable &pt, string scramblename, setval &p,
               generatingset *gs, const char *s);
#define SOLVE_H
#endif
