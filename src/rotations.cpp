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
   stacksetval pw(pd) ;
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
   pd.rotinv.clear() ;
   for (int m1=0; m1<(int)pd.rotgroup.size(); m1++) {
      for (int m2=0; m2<(int)pd.rotgroup.size(); m2++) {
         pd.mul(pd.rotgroup[m1].pos, pd.rotgroup[m2].pos, pw) ;
         if (pd.comparepos(pd.id, pw) == 0) {
            pd.rotinv.push_back(m2) ;
            break ;
         }
      }
   }
   if (pd.rotinv.size() != pd.rotgroup.size())
      error("! error looking for rotation inverses") ;
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
int slowmodm0(const puzdef &pd, const setval p1, setval p2) {
   int cnt = -1 ;
   stacksetval s1(pd), s2(pd) ;
   int v0 = 1000, v1=1000 ;
// cout << "Doing " ; showpos(pd, p1) ;
   for (int m1=0; m1<(int)pd.rotgroup.size(); m1++) {
      pd.mul(pd.rotgroup[m1].pos, p1, s1) ;
      int m2 = pd.rotinv[m1] ;
      {
//    for (int m2=0; m2<(int)pd.rotgroup.size(); m2++) {
         int t = s1.dat[pd.rotgroup[m2].pos.dat[0]] - v0 ;
         if (t > 0)
            continue ;
         if (t == 0 && pd.setdefs[0].size > 1) {
            t = s1.dat[pd.rotgroup[m2].pos.dat[1]] - v1 ;
            if (t > 0)
               continue ;
         }
         if (t < 0) {
            pd.mul(s1, pd.rotgroup[m2].pos, p2) ;
            cnt = 1 ;
            v0 = p2.dat[0] ;
            v1 = p2.dat[1] ;
         } else {
            t = pd.mulcmp(s1, pd.rotgroup[m2].pos, p2) ;
            if (t <= 0) {
               if (t < 0) {
                  cnt = 1 ;
                  v0 = p2.dat[0] ;
                  v1 = p2.dat[1] ;
               } else
                  cnt++ ;
            }
         }
      }
   }
// cout << "Returning count of " << cnt << endl ;
   return cnt ;
}
int slowmodm(const puzdef &pd, const setval p1, setval p2) {
   int cnt = -1 ;
   int v0 = 1000, v1=1000 ;
   for (int m1=0; m1<(int)pd.rotgroup.size(); m1++) {
      int m2 = pd.rotinv[m1] ;
      int t = pd.rotgroup[m1].pos.dat[p1.dat[pd.rotgroup[m2].pos.dat[0]]] - v0 ;
      if (t > 0)
         continue ;
      if (t == 0 && pd.setdefs[0].size > 1) {
         t = pd.rotgroup[m1].pos.dat[p1.dat[pd.rotgroup[m2].pos.dat[1]]] - v1 ;
         if (t > 0)
            continue ;
      }
      if (t < 0) {
         pd.mul3(pd.rotgroup[m1].pos, p1, pd.rotgroup[m2].pos, p2) ;
         cnt = 1 ;
         v0 = p2.dat[0] ;
         v1 = p2.dat[1] ;
      } else {
         t = pd.mulcmp3(pd.rotgroup[m1].pos, p1, pd.rotgroup[m2].pos, p2) ;
         if (t <= 0) {
            if (t < 0) {
               cnt = 1 ;
               v0 = p2.dat[0] ;
               v1 = p2.dat[1] ;
            } else
               cnt++ ;
         }
      }
   }
// cout << "Returning count of " << cnt << endl ;
   return cnt ;
}
