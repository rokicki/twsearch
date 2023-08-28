#include "solve.h"
#include "cmdlineops.h"
#include <iostream>
ull solutionsfound = 0;
ull solutionsneeded = 1;
int noearlysolutions;
int onlyimprovements;
int phase2;
int optmindepth;
int randomstart;
string lastsolution;
int didprepass;
solveworker solveworkers[MAXTHREADS];
int (*callback)(setval &pos, const vector<int> &moves, int d, int id);
int (*flushback)(int d);
static vector<vector<int>> randomized;
void setsolvecallback(int (*f)(setval &pos, const vector<int> &moves, int d,
                               int id),
                      int (*g)(int)) {
  callback = f;
  flushback = g;
}
void *threadworker(void *o) {
  workerparam *wp = (workerparam *)o;
  solveworkers[wp->tid].dowork(wp->pd, wp->pt);
  return 0;
}
void solveworker::init(const puzdef &pd, int d_, int id_, const setval &p) {
  if (looktmp) {
    delete[] looktmp->dat;
    delete looktmp;
    looktmp = 0;
  }
  looktmp = new allocsetval(pd, pd.solved);
  // make the position table big to minimize false sharing.
  while (posns.size() <= 100 || (int)posns.size() <= d_ + 10) {
    posns.push_back(allocsetval(pd, pd.solved));
    movehist.push_back(-1);
  }
  pd.assignpos(posns[0], p);
  lookups = 0;
  d = d_;
  id = id_;
}
int solveworker::possibsolution(const puzdef &pd, int sp) {
  if (callback) {
    return callback(posns[sp], movehist, d, id);
  }
  if (pd.comparepos(posns[sp], pd.solved) == 0) {
    int r = 1;
    get_global_lock();
    solutionsfound++;
    lastsolution.clear();
    if (d == 0) // allow null solution to trigger
      cout << " ";
    for (int i = 0; i < d; i++) {
      cout << " " << pd.moves[movehist[i]].name;
      if (i > 0)
        lastsolution += " ";
      lastsolution += pd.moves[movehist[i]].name;
    }
    cout << endl << flush;
    if (solutionsfound < solutionsneeded)
      r = 0;
    release_global_lock();
    return r;
  } else
    return 0;
}
int solveworker::solveiter(const puzdef &pd, prunetable &pt, int togo, int sp,
                           int st) {
  ull h = innersetup(pt, sp);
  while (1) {
    int v = innerfetch(pd, pt, togo, sp, st, h);
    if (v != 3)
      return v;
    h = innersetup(pt, sp);
  }
}
ull solveworker::innersetup(prunetable &pt, int sp) {
  lookups++;
  return pt.prefetchindexed(pt.gethashforlookup(posns[sp], looktmp));
}
int solveworker::innerfetch(const puzdef &pd, prunetable &pt, int &togo,
                            int &sp, int &st, ull h) {
  int v = pt.lookuphindexed(h);
  int m, mi;
  ull mask, skipbase;
  if (v > togo + 1) {
    v = -1;
  } else if (v > togo) {
    v = 0;
  } else if (v == 0 && togo == 1 && didprepass &&
             pd.comparepos(posns[sp], pd.solved) == 0) {
    v = 0;
  } else if (v == 0 && togo > 0 && noearlysolutions &&
             pd.comparepos(posns[sp], pd.solved) == 0) {
    v = 0;
  } else if (togo == 0) {
    v = possibsolution(pd, sp);
  } else {
    mask = canonmask[st];
    skipbase = 0;
    mi = -1;
    goto downstack;
  }
upstack:
  if (solvestates.size() == 0)
    return v;
  {
    auto &ss = solvestates[solvestates.size() - 1];
    togo++;
    sp--;
    st = ss.st;
    mi = ss.mi;
    mask = ss.mask;
    skipbase = ss.skipbase;
  }
  solvestates.pop_back();
  if (v == 1)
    goto upstack;
  if (!quarter && v == -1) {
    m = randomstart ? randomized[togo][mi] : mi;
    if (pd.moves[m].base < 64)
      skipbase |= 1LL << pd.moves[m].base;
  }
downstack:
  mi++;
  if (mi >= (int)pd.moves.size()) {
    v = 0;
    goto upstack;
  }
  m = randomstart ? randomized[togo][mi] : mi;
  const moove &mv = pd.moves[m];
  if (!quarter && mv.base < 64 && ((skipbase >> mv.base) & 1))
    goto downstack;
  if ((mask >> mv.cs) & 1)
    goto downstack;
  pd.mul(posns[sp], mv.pos, posns[sp + 1]);
  if (!pd.legalstate(posns[sp + 1]))
    goto downstack;
  movehist[sp] = m;
  solvestates.push_back({st, mi, mask, skipbase});
  togo--;
  sp++;
  st = canonnext[st][mv.cs];
  return 3;
}
int solveworker::solvestart(const puzdef &pd, prunetable &pt, int w) {
  ull initmoves = workchunks[w];
  int nmoves = pd.moves.size();
  int sp = 0;
  int st = 0;
  int togo = d;
  while (initmoves > 1) {
    int mv = initmoves % nmoves;
    pd.mul(posns[sp], pd.moves[mv].pos, posns[sp + 1]);
    if (!pd.legalstate(posns[sp + 1]))
      return -1;
    movehist[sp] = mv;
    st = canonnext[st][pd.moves[mv].cs];
    sp++;
    togo--;
    initmoves /= nmoves;
  }
  solvestates.clear();
  return solveiter(pd, pt, togo, sp, st);
}
void solveworker::dowork(const puzdef &pd, prunetable &pt) {
  while (1) {
    int w = -1;
    int finished = 0;
    get_global_lock();
    finished = (solutionsfound >= solutionsneeded);
    if (workat < (int)workchunks.size())
      w = workat++;
    release_global_lock();
    if (finished || w < 0)
      return;
    if (solvestart(pd, pt, w) == 1)
      return;
  }
}
int maxdepth = 1000000000;
int solve(const puzdef &pd, prunetable &pt, const setval p, generatingset *gs) {
  solutionsfound = solutionsneeded;
  if (gs && !gs->resolve(p)) {
    if (!phase2)
      cout << "Ignoring unsolvable position." << endl;
    return -1;
  }
  stacksetval looktmp(pd);
  double starttime = walltime();
  ull totlookups = 0;
  int initd = pt.lookup(p, &looktmp);
  solutionsfound = 0;
  int hid = 0;
  randomized.clear();
  for (int d = initd; d <= maxdepth; d++) {
    if (randomstart) {
      while ((int)randomized.size() <= d) {
        randomized.push_back({});
        vector<int> &r = randomized[randomized.size() - 1];
        for (int i = 0; i < (int)pd.moves.size(); i++)
          r.push_back(i);
        for (int i = 0; i < (int)r.size(); i++) {
          int j = i + myrand(r.size() - i);
          swap(r[i], r[j]);
        }
      }
    }
    if (onlyimprovements && d >= globalinputmovecount)
      break;
    if (d < optmindepth)
      continue;
    hid = d;
    if (d - initd > 3)
      makeworkchunks(pd, d, 0);
    else
      makeworkchunks(pd, 0, 0);
    int wthreads = setupthreads(pd, pt);
    for (int t = 0; t < wthreads; t++)
      solveworkers[t].init(pd, d, t, p);
#ifdef USE_PTHREADS
    for (int i = 1; i < wthreads; i++)
      spawn_thread(i, threadworker, &(workerparams[i]));
    threadworker((void *)&workerparams[0]);
    for (int i = 1; i < wthreads; i++)
      join_thread(i);
#else
    threadworker((void *)&workerparams[0]);
#endif
    for (int i = 0; i < wthreads; i++) {
      totlookups += solveworkers[i].lookups;
      pt.addlookups(solveworkers[i].lookups);
    }
    if (solutionsfound >= solutionsneeded) {
      duration();
      double actualtime = start - starttime;
      cout << "Found " << solutionsfound << " solution"
           << (solutionsfound != 1 ? "s" : "") << " at maximum depth " << d
           << " lookups " << totlookups << " in " << actualtime << " rate "
           << (totlookups / actualtime) << endl
           << flush;
      return d;
    }
    double dur = duration();
    if (verbose) {
      if (verbose > 1 || dur > 1)
        cout << "Depth " << d << " finished in " << dur << endl << flush;
    }
    if (flushback)
      if (flushback(d))
        break;
    if (d != maxdepth)
      pt.checkextend(pd); // fill table up a bit more if needed
  }
  if (!phase2 && callback == 0)
    cout << "No solution found in " << hid << endl << flush;
  return -1;
}
