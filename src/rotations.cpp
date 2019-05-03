#include "rotations.h"
#include <set>
#include <vector>
#include <iostream>
// so we can use STL we wrap setvals in a vector.
vector<uchar> setvaltovec(puzdef &pd, setval v) {
   return vector<uchar>(v.dat, v.dat+pd.totsize) ;
}
void calcrotations(puzdef &pd) {
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      setdef &sd = pd.setdefs[i] ;
      if (sd.omod != 1 && !sd.uniq)
         error("! can't use rotations when oriented duplicated pieces.") ;
      if (!sd.uniq)
         error("! non-uniq not yet supported for rotations") ;
   }
   vector<moove> &q = pd.rotgroup ;
   set<vector<uchar>> seen ;
   seen.insert(setvaltovec(pd, pd.id)) ;
   moove m ;
   m.name = "(identity)" ;
   m.pos = allocsetval(pd, pd.id) ;
   m.cost = 0 ;
   m.twist = 0 ;
   q.push_back(m) ;
   for (int qg=0; qg < (int)q.size(); qg++) {
      for (int i=0; i<(int)pd.rotations.size(); i++) {
         vector<uchar> t(pd.totsize) ;
         setval w(t.data()) ;
         pd.mul(q[qg].pos, pd.rotations[i].pos, w) ;
         if (seen.find(t) == seen.end()) {
            seen.insert(t) ;
            m.name = "(rotation)" ;
            m.pos = allocsetval(pd, w) ;
            q.push_back(m) ;
         }
      }
   }
   cout << "Rotation group size is " << q.size() << endl ;
}
void showpos(const puzdef &pd, const setval s) {
   for (int i=0; i<pd.totsize; i++)
      cout << " " << (int)s.dat[i] ;
   cout << endl ;
}
/*
 *   Need to be able to do premoves, but in general *positions* don't
 *   permit premoves.  However, *rotations* of *normal* puzzles should
 *   allow rotations to be defined since all identical pieces live on
 *   the same face and thus move as a fixed set.  Our calcrotations
 *   above should validate this and then make face "moves" that do
 *   the correct thing.  Only center pieces should be permitted to be
 *   (so omod should be 1 unless the setdef is uniq.)
 */
int slowmodm(const puzdef &pd, const setval p1, setval p2) {
   int cnt = -1 ;
   stacksetval s1(pd), s2(pd) ;
// cout << "Doing " ; showpos(pd, p1) ;
   for (int m1=0; m1<(int)pd.rotgroup.size(); m1++) {
      pd.mul(pd.rotgroup[m1].pos, p1, s1) ;
      for (int m2=0; m2<(int)pd.rotgroup.size(); m2++) {
         if (s1.dat[pd.rotgroup[m2].pos.dat[0]] > s2.dat[0])
            continue ;
         pd.mul(s1, pd.rotgroup[m2].pos, s2) ;
         int t = pd.comparepos(p2, s2) ;
//       cout << "Comparing (" << t << ")" ; showpos(pd, s2) ;
         if (cnt < 0 || t > 0) {
            pd.assignpos(p2, s2) ;
            cnt = 1 ;
         } else if (t == 0) {
            cnt++ ;
         }
      }
   }
// cout << "Returning count of " << cnt << endl ;
   return cnt ;
}
