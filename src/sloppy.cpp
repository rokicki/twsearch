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
using namespace std ;
int unblocked(const puzdef &pd, const setval &p) {
   int r = 0 ;
   stacksetval p2(pd) ;
   for (int m=0; m<(int)pd.rotgroup.size(); m++) {
      pd.mul(p, pd.rotgroup[m].pos, p2) ;
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
   return r2 ;
}
