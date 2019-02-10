#include "workchunks.h"
#include "threads.h"
#include "canon.h"
vector<ull> workchunks ;
vector<int> workstates ;
int workat ;
void makeworkchunks(const puzdef &pd, int d) {
   workchunks.clear() ;
   workstates.clear() ;
   workchunks.push_back(1) ;
   workstates.push_back(0) ;
   int nmoves = pd.moves.size() ;
   int chunkmoves = 0 ;
   if (numthreads > 1 && d >= 3) {
      ull mul = 1 ;
      while (chunkmoves + 3 < d && (int)workchunks.size() < 40 * numthreads) {
         vector<ull> wc2 ;
         vector<int> ws2 ;
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
         swap(wc2, workchunks) ;
         swap(ws2, workstates) ;
         chunkmoves++ ;
         mul *= nmoves ;
      }
   }
}
