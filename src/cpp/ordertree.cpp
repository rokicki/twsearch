#include <map>
#include <iostream>
#include "canon.h"
#include "ordertree.h"
#include "findalgo.h"
void recurorder(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      vector<int> cc = pd.cyccnts(posns[sp]) ;
      ll o = puzdef::order(cc) ;
      if (1) {
         cout << o ;
         for (int i=0; i<sp; i++)
            cout << " " << pd.moves[movehist[i]].name ;
         cout << endl ;
      }
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      if (pd.legalstate(posns[sp+1]))
         recurorder(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void ordertree(const puzdef &pd) {
   for (int d=1; ; d++) {
      posns.clear() ;
      movehist.clear() ;
      while ((int)posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      recurorder(pd, d, 0, 0) ;
   }
}
