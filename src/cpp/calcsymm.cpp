#include "calcsymm.h"
#include <iostream>
void calcsym(const puzdef &pd, int iat, int nmoves, vector<char> &used,
             vector<int> &mapped, vector<int> &basemovemapped,
             const setval &ip1, const setval &ip2, vector<vector<int>> &cc,
             int nmul) {
// cout << "In calcsym at " << at << endl ;
   if (iat == nmoves) {
      cout << "SYM" ;
      for (int i=0; i<nmoves; i++)
         cout << " " << pd.moves[i].name << "-" << pd.moves[mapped[i]].name  ;
      cout << endl ;
      return ;
   }
   int at = iat * nmul % nmoves ;
   stacksetval p1(pd), p2(pd), p3(pd), p4(pd) ;
   for (int i=0; i<nmoves; i++) {
      if (used[i])
         continue ;
      if (basemovemapped[pd.moves[at].base] >= 0 &&
          basemovemapped[pd.moves[at].base] != pd.moves[i].base)
         continue ;
      if (cc[at] != cc[i])
         continue ;
      pd.assignpos(p1, ip1) ;
      pd.assignpos(p2, ip2) ;
      pd.mul(p1, pd.moves[at].pos, p3) ;
      pd.mul(p2, pd.moves[i].pos, p4) ;
      if (pd.cyccnts(p3) != pd.cyccnts(p4))
         continue ;
      if (iat > 0) {
         int r = myrand(iat) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
         r = myrand(iat) ;
         r = r * nmul % nmoves ;
         pd.mul(p1, pd.moves[r].pos, p3) ;
         pd.mul(p2, pd.moves[mapped[r]].pos, p4) ;
         if (pd.cyccnts(p3) != pd.cyccnts(p4))
            continue ;
         r = myrand(iat) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
         r = myrand(iat) ;
         r = r * nmul % nmoves ;
         pd.mul(p1, pd.moves[r].pos, p3) ;
         pd.mul(p2, pd.moves[mapped[r]].pos, p4) ;
         if (pd.cyccnts(p3) != pd.cyccnts(p4))
            continue ;
         r = myrand(iat) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
      }
      int obmm = basemovemapped[pd.moves[at].base] ;
      basemovemapped[pd.moves[at].base] = pd.moves[i].base ;
      mapped[at] = i ;
      used[i] = 1 ;
      calcsym(pd, iat+1, nmoves, used, mapped, basemovemapped, p1, p2, cc, nmul) ;
      used[i] = 0 ;
      basemovemapped[pd.moves[at].base] = obmm ;
   }
}
void gensymm(const puzdef &pd) {
   int nmoves = pd.moves.size() ;
   int nmul = (int)(0.618 * nmoves) ;
   if (nmul == 0)
      nmul = 1 ;
   while (gcd(nmul, nmoves) != 1)
      nmul++ ;
   if (nmul >= nmoves)
      nmul = 1 ;
   vector<char> used(nmoves) ;
   vector<int> mapped(nmoves) ;
   vector<int> basemovemapped(pd.basemoves.size()) ;
   for (int i=0; i<(int)basemovemapped.size(); i++)
      basemovemapped[i] = -1 ;
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.id) ;
   pd.assignpos(p2, pd.id) ;
   vector<vector<int>> cc ;
   for (int i=0; i<nmoves; i++)
      cc.push_back(pd.cyccnts(pd.moves[i].pos)) ;
   calcsym(pd, 0, nmoves, used, mapped, basemovemapped, p1, p2, cc, nmul) ;
}
