#include <iostream>
#include "solve.h"
ull solutionsfound = 0 ;
ull solutionsneeded = 1 ;
int noearlysolutions ;
int phase2 ;
solveworker solveworkers[MAXTHREADS] ;
void *threadworker(void *o) {
   workerparam *wp = (workerparam *)o ;
   solveworkers[wp->tid].dowork(wp->pd, wp->pt) ;
   return 0 ;
}
void solveworker::init(const puzdef &pd, int d_, const setval &p) {
   // make the position table big to minimize false sharing.
   while (posns.size() <= 100 || (int)posns.size() <= d_+1) {
      posns.push_back(allocsetval(pd, pd.solved)) ;
      movehist.push_back(-1) ;
   }
   pd.assignpos(posns[0], p) ;
   lookups = 0 ;
   d = d_ ;
}
int solveworker::solverecur(const puzdef &pd, prunetable &pt, int togo, int sp, int st) {
   lookups++ ;
   int v = pt.lookup(posns[sp]) ;
   if (v > togo + 1)
      return -1 ;
   if (v > togo)
      return 0 ;
   if (v == 0 && togo > 0 && pd.comparepos(posns[sp], pd.solved) == 0 &&
       noearlysolutions)
      return 0 ;
   if (togo == 0) {
      if (pd.comparepos(posns[sp], pd.solved) == 0) {
         int r = 1 ;
         get_global_lock() ;
         solutionsfound++ ;
         if (d == 0) // allow null solution to trigger
            cout << " " ;
         for (int i=0; i<d; i++)
            cout << " " << pd.moves[movehist[i]].name ;
         cout << endl << flush ;
         if (solutionsfound < solutionsneeded)
            r = 0 ;
         release_global_lock() ;
         return r ;
      } else
         return 0 ;
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
      movehist[sp] = m ;
      v = solverecur(pd, pt, togo-1, sp+1, ns[mv.cs]) ;
      if (v == 1)
         return 1 ;
      if (!quarter && v == -1) {
         // skip similar rotations
         while (m+1 < (int)pd.moves.size() && pd.moves[m].base == pd.moves[m+1].base)
            m++ ;
      }
   }
   return 0 ;
}
int solveworker::solvestart(const puzdef &pd, prunetable &pt, int w) {
   ull initmoves = workchunks[w] ;
   int nmoves = pd.moves.size() ;
   int sp = 0 ;
   int st = 0 ;
   int togo = d ;
   while (initmoves > 1) {
      int mv = initmoves % nmoves ;
      pd.mul(posns[sp], pd.moves[mv].pos, posns[sp+1]) ;
      if (!pd.legalstate(posns[sp+1]))
         return -1 ;
      movehist[sp] = mv ;
      st = canonnext[st][pd.moves[mv].cs] ;
      sp++ ;
      togo-- ;
      initmoves /= nmoves ;
   }
   return solverecur(pd, pt, togo, sp, st) ;
}
void solveworker::dowork(const puzdef &pd, prunetable &pt) {
   while (1) {
      int w = -1 ;
      int finished = 0 ;
      get_global_lock() ;
      finished = (solutionsfound >= solutionsneeded) ;
      if (workat < (int)workchunks.size())
         w = workat++ ;
      release_global_lock() ;
      if (finished || w < 0)
         return ;
      if (solvestart(pd, pt, w) == 1)
         return ;
   }
}
int maxdepth = 1000000000 ;
int solve(const puzdef &pd, prunetable &pt, const setval p, generatingset *gs) {
   solutionsfound = solutionsneeded ;
   if (gs && !gs->resolve(p)) {
      if (!phase2)
         cout << "Ignoring unsolvable position." << endl ;
      return -1 ;
   }
   double starttime = walltime() ;
   ull totlookups = 0 ;
   int initd = pt.lookup(p) ;
   for (int d=initd; d <= maxdepth; d++) {
      if (d - initd > 3)
         makeworkchunks(pd, d) ;
      else
         makeworkchunks(pd, 0) ;
      int wthreads = setupthreads(pd, pt) ;
      for (int t=0; t<wthreads; t++)
         solveworkers[t].init(pd, d, p) ;
      solutionsfound = 0 ;
      for (int i=0; i<wthreads; i++)
         spawn_thread(i, threadworker, &(workerparams[i])) ;
      for (int i=0; i<wthreads; i++)
         join_thread(i) ;
      for (int i=0; i<wthreads; i++) {
         totlookups += solveworkers[i].lookups ;
         pt.addlookups(solveworkers[i].lookups) ;
      }
      if (solutionsfound >= solutionsneeded) {
         duration() ;
         double actualtime = start - starttime ;
         cout << "Found " << solutionsfound << " solution" <<
                 (solutionsfound != 1 ? "s" : "") << " at maximum depth " <<
                 d << " lookups " << totlookups << " in " << actualtime <<
                 " rate " << (totlookups/actualtime) << endl << flush ;
         return d ;
      }
      double dur = duration() ;
      if (verbose) {
         if (verbose > 1 || dur > 1)
            cout << "Depth " << d << " finished in " << dur << endl << flush ;
      }
      pt.checkextend(pd) ; // fill table up a bit more if needed
   }
   if (!phase2)
      cout << "No solution found in " << maxdepth << endl << flush ;
   return -1 ;
}
