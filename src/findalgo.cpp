#include <map>
#include <iostream>
#include "canon.h"
#include "findalgo.h"
map<ll, int> bestsofar ;
const int HIWR = 4 ;
ll extendkey(ll k, int nwr, int npwr) {
   return k * 10 + nwr * 2 + (npwr == 0 ? 0 : 1) ;
}
ll bigcnt = 0 ;
void recurfindalgo(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      bigcnt++ ;
      int wr = pd.numwrong(posns[sp], pd.solved) ;
      if (wr > HIWR || wr == 0)
         return ;
      ll key = 0 ;
      for (int i=0; i<(int)pd.setdefs.size(); i++) {
         key = extendkey(key,
                          pd.numwrong(posns[sp], pd.solved, 1LL << i),
                          pd.permwrong(posns[sp], pd.solved, 1LL << i)) ;
      }
      int mvs = sp ;
      if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
         return ;
      bestsofar[key] = mvs ;
      cout << key << " " << mvs << " (" ;
      for (int i=0; i<sp; i++) {
         if (i)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << ")" << endl << flush ;
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
      recurfindalgo(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void findalgos(const puzdef &pd) {
   for (int d=1; ; d++) {
      while ((int)posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.solved)) ;
         movehist.push_back(-1) ;
      }
      bigcnt = 0 ;
      recurfindalgo(pd, d, 0, 0) ;
      cout << "At " << d << " big count is " << bigcnt << " in " << duration() << endl ;
   }
}
void recurfindalgo2(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      vector<int> cc = pd.cyccnts(posns[sp]) ;
      ll o = puzdef::order(cc) ;
      for (int pp=2; pp<=3; pp++) {
         if (o % pp == 0) {
            pd.pow(posns[sp], posns[sp+1], o/pp) ;
            int wr = pd.numwrong(posns[sp+1], pd.id) ;
            if (wr > HIWR || wr == 0)
               continue ;
            ll key = 0 ;
            for (int i=0; i<(int)pd.setdefs.size(); i++) {
               key = extendkey(key, pd.numwrong(posns[sp+1], pd.id, 1LL << i),
                                pd.permwrong(posns[sp+1], pd.id, 1LL << i)) ;
            }
            int mvs = o / pp * sp ;
            if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
               continue ;
            bestsofar[key] = mvs ;
            cout << pp << " " << key << " " << mvs << " (" ;
            for (int i=0; i<sp; i++) {
               if (i)
                  cout << " " ;
               cout << pd.moves[movehist[i]].name ;
            }
            cout << ")" << (mvs / sp) << " (" ;
            const char *spacer = "" ;
            for (int i=1; i<(int)cc.size(); i++) {
               if (cc[i]) {
                  cout << spacer ;
                  spacer = " " ;
                  cout << i << ":" << cc[i] ;
               }
            }
            cout << ") " ;
            cout << o << endl << flush ;
         }
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
      recurfindalgo2(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void findalgos2(const puzdef &pd) {
   for (int d=1; ; d++) {
      while ((int)posns.size() <= d + 3) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      recurfindalgo2(pd, d, 0, 0) ;
   }
}
void recurfindalgo3b(const puzdef &pd, int togo, int sp, int st, int fp) {
   if (togo == 0) {
      pd.inv(posns[sp], posns[sp+1]) ;
      pd.mul(posns[fp], posns[sp], posns[sp+2]) ;
      pd.mul(posns[sp+2], posns[fp+1], posns[sp+3]) ;
      pd.mul(posns[sp+3], posns[sp+1], posns[sp+2]) ;
      int wr = pd.numwrong(posns[sp+2], pd.id) ;
      if (wr > HIWR || wr == 0)
         return ;
      ll key = 0 ;
      for (int i=0; i<(int)pd.setdefs.size(); i++) {
         key = extendkey(key, pd.numwrong(posns[sp+2], pd.id, 1LL << i),
                          pd.permwrong(posns[sp+2], pd.id, 1LL << i)) ;
      }
      int mvs = 2 * (fp + (sp - (fp + 2))) ;
      if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
         return ;
      bestsofar[key] = mvs ;
      cout << key << " " << mvs << " [" ;
      for (int i=0; i<fp; i++) {
         if (i)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << "," ;
      for (int i=fp+2; i<sp; i++) {
         if (i != fp+2)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << "]" << endl << flush ;
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
      recurfindalgo3b(pd, togo-1, sp+1, ns[mv.cs], fp) ;
   }
}
void recurfindalgo3a(const puzdef &pd, int togo, int sp, int st, int b) {
   if (togo == 0) {
      pd.inv(posns[sp], posns[sp+1]) ;
      pd.assignpos(posns[sp+2], pd.id) ;
      recurfindalgo3b(pd, b, sp+2, 0, sp) ;
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
      recurfindalgo3a(pd, togo-1, sp+1, ns[mv.cs], b) ;
   }
}
void findalgos3(const puzdef &pd) {
   for (int d=2; ; d++) {
      while ((int)posns.size() <= d + 7) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      for (int a=1; a+a<=d; a++)
         recurfindalgo3a(pd, d-a, 0, 0, a) ;
   }
}
