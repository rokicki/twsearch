/*
 *   Given a sloppy cube position (specific to the exact tws file we
 *   checked in for the sloppy cube), determine what moves are not
 *   blocked.  This assumes sets are corners, edges, centers, and it
 *   assumes cubie and orientation numbering is precisely as given in
 *   that tws file.
 */
static const char cec[24] = {
   0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1,
// 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
} ;
static const char cee[24] = {
   0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0,
} ;
static const char ece[24] = {
   0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
} ;
static const char ecc[24] = {
   0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
// 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1,
} ;
// # 1 U # 2 L # 3 F # 4 R # 5 B # 6 D
#include "puzdef.h"
#include "sloppy.h"
#include <iostream>
#include <map>
#include <fstream>
#include "readksolve.h"
using namespace std ;
static map<ll, int> exc ;
static ll getshape(const setval &sv) {
   ll r = 0 ;
   for (int i=0; i<8; i++) {
      r *= 4 ;
      if (sv.dat[i] == 1 || sv.dat[i] == 5) {
         r += 0 ;
      } else {
         r += 1 + sv.dat[8+i] ;
      }
   }
// S M M L M S L M L M M  S
// 0 1 2 3 4 5 6 7 8 9 10 11
   for (int i=0; i<12; i++) {
      r *= 4 ;
      if (sv.dat[16+i] == 0 || sv.dat[16+i] == 5 || sv.dat[16+i] == 11) {
         r += 0 ;
      } else if (sv.dat[16+i] == 3 || sv.dat[16+i] == 6 || sv.dat[16+i] == 8) {
         r += 1 ;
      } else {
         r += 2 + sv.dat[16+12+i] ;
      }
   }
   return r ;
}
static vector<vector<int>> movemap ;
static void makemovemap(const puzdef &pd) {
   stacksetval p2(pd) ;
   movemap.resize(pd.rotgroup.size()) ;
   for (int m=0; m<(int)pd.rotgroup.size(); m++) {
      movemap[m].resize(pd.moves.size()) ;
      for (int i=0; i<(int)pd.moves.size(); i++) {
         pd.mul3(pd.rotinvmap[m], pd.moves[i].pos, pd.rotgroup[m].pos, p2) ;
         int found = 0 ;
         for (int j=0; j<(int)pd.moves.size(); j++)
            if (pd.comparepos(p2, pd.moves[j].pos) == 0) {
               found = 1 ;
               movemap[m][i] = j ;
            }
         if (!found)
            cout << "Could not find in movemap" << endl ;
      }
   }
}
static void regist(const puzdef &pd, const setval &sv, int realskip) {
   stacksetval p2(pd) ;
   for (int m=0; m<(int)pd.rotgroup.size(); m++) {
      pd.mul3(pd.rotinvmap[m], sv, pd.rotgroup[m].pos, p2) ;
      ll sh = getshape(p2) ;
      int rs = 0 ;
      for (int i=0; i<(int)pd.moves.size(); i++)
         if ((realskip >> i) & 1)
            rs |= 1<<movemap[m][i] ;
//    cout << "For shape " << sh << " setting " << rs << endl ;
      exc[sh] = rs ;
   }
}
static int inited ;
static void initslop(const puzdef &pd) {
   makemovemap(pd) ;
   ifstream safefile ;
   safefile.open("exceptions.safe") ;
   ull cs = 0 ;
   stacksetval p(pd) ;
   while (1) {
      if (safefile.eof())
         break ;
      auto toks = getline(&safefile, cs) ;
      if (toks.size() == 0)
         continue ;
      pd.assignpos(p, pd.solved) ;
      int at = 0 ;
      while (1) {
         if (toks[at] == "allows")
            break ;
         int found = -1 ;
         for (int i=0; i<(int)pd.moves.size(); i++)
            if (toks[at] == pd.moves[i].name)
               found = i ;
         if (found < 0)
            cout << "Could not find move " << toks[at] << endl ;
         else
            domove(pd, p, found) ;
         at++ ;
      }
      at++ ;
      int rskip = (1 << pd.moves.size()) - 1 ;
      while (at < (int)toks.size()) {
         for (int i=0; i<(int)pd.moves.size(); i++)
            if (pd.moves[i].name[0] == toks[at][0])
               rskip &= ~(1 << i) ;
         at++ ;
      }
      regist(pd, p, rskip) ;
   }
   safefile.close() ;
}
int unblocked(const puzdef &pd, const setval &p) {
   int r = 0 ;
   stacksetval p2(pd) ;
   if (!inited) {
      initslop(pd) ;
      inited = 1 ;
   }
   ll sh = getshape(p) ;
   if (exc.find(sh) != exc.end())
      return exc[sh] ;
   for (int m=0; m<(int)pd.blockgroup.size(); m++) {
      pd.mul(p, pd.blockgroup[m].pos, p2) ;
      int leftc = p2.dat[3] * 3 + p2.dat[11] ;
      int rightc = p2.dat[0] * 3 + p2.dat[8] ;
      int me = p2.dat[16] * 2 + p2.dat[28] ;
// cout << "At " << m << " cubies " << leftc << " " << me << " " << rightc << " vals " << 
//(int)cec[leftc] << (int)cee[me] << " " <<
//(int)ece[me] << (int)ecc[rightc] << endl ;
      if (ece[me] != ecc[rightc] || cee[me] != cec[leftc])
         r |= 1 << p2.dat[40] ;
   }
   int r2 = 0 ;
   for (int i=0; i<(int)pd.moves.size(); i++)
      for (int j=0; j<6; j++)
         if (((r >> j) & 1) && pd.moves[i].pos.dat[46+j]) {
            r2 |= 1 << i ;
//          cout << "Move " << pd.moves[i].name << " blocked." << endl ;
         }
   regist(pd, p, r2) ;
   return r2 ;
}
