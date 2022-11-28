#include "workchunks.h"
#include "threads.h"
#include "canon.h"
#include "rotations.h"
#include "solve.h"
#include <iostream>
vector<ull> workchunks ;
vector<int> workstates ;
int workat ;
static vector<allocsetval> seen ;
static int lastsize ;
void makeworkchunks(const puzdef &pd, int d, int symmreduce) {
   workchunks.clear() ;
   workstates.clear() ;
   workchunks.push_back(1) ;
   workstates.push_back(0) ;
   if (numthreads > 1 && d >= 3) {
      if (pd.totsize != lastsize) {
         lastsize = pd.totsize ;
         seen.clear() ;
      }
      stacksetval p1(pd), p2(pd), p3(pd) ;
      int nmoves = pd.moves.size() ;
      int chunkmoves = 0 ;
      ull mul = 1 ;
      while (chunkmoves + 3 < d && (int)workchunks.size() < 40 * numthreads) {
         vector<ull> wc2 ;
         vector<int> ws2 ;
         int seensize = 0 ;
         if (symmreduce && pd.rotgroup.size() > 1) {
            for (int i=0; i<(int)workchunks.size(); i++) {
               ull pmv = workchunks[i] ;
               ull t = pmv ;
               pd.assignpos(p1, pd.solved) ;
               while (t > 1) {
                  domove(pd, p1, t % nmoves) ;
                  t /= nmoves ;
               }
               for (int mv=0; mv<nmoves; mv++) {
                  if (quarter && pd.moves[mv].cost > 1)
                     continue ;
                  pd.mul(p1, pd.moves[mv].pos, p2) ;
                  if (!pd.legalstate(p2))
                     continue ;
                  slowmodm2(pd, p2, p3) ;
                  int isnew = 1 ;
                  for (int j=0; j<(int)seensize; j++)
                     if (pd.comparepos(p3, seen[j]) == 0) {
                        isnew = 0 ;
                        break ;
                     }
                  if (isnew) {
                     wc2.push_back(pmv + (nmoves + mv - 1) * mul) ;
                     ws2.push_back(0) ;
                     if (seensize < (int)seen.size()) {
                        pd.assignpos(seen[seensize], p3) ;
                     } else {
                        seen.push_back(allocsetval(pd, p3)) ;
                     }
                     seensize++ ;
                  }
               }
            }
         } else {
            for (int i=0; i<(int)workchunks.size(); i++) {
               ull pmv = workchunks[i] ;
               int st = workstates[i] ;
               ull mask = canonmask[st] ;
               const vector<int> &ns = canonnext[st] ;
               for (int mv=0; mv<nmoves; mv++)
                  if (0 == ((mask >> pd.moves[mv].cs) & 1)) {
                     wc2.push_back(pmv + (nmoves + mv - 1) * mul) ;
                     ws2.push_back(ns[pd.moves[mv].cs]) ;
                  }
            }
         }
         swap(wc2, workchunks) ;
         swap(ws2, workstates) ;
         chunkmoves++ ;
         mul *= nmoves ;
         if (mul >= (1ULL << 62) / nmoves) {
            cout << "Mul got too big." << endl ;
            break ;
         }
      }
      if (randomstart) {
         for (int i=0; i<(int)workchunks.size(); i++) {
            int j = i + (int)((workchunks.size()-i)*drand48()) ;
            swap(workchunks[i], workchunks[j]) ;
            swap(workstates[i], workstates[j]) ;
         }
      }
   }
}
