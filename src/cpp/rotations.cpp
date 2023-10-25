#include "rotations.h"
#include "index.h"
#include <iostream>
#include <map>
#include <set>
#include <vector>
// so we can use STL we wrap setvals in a vector.
vector<uchar> setvaltovec(puzdef &pd, setval v) {
  return vector<uchar>(v.dat, v.dat + pd.totsize);
}
void calcrotinvs(puzdef &pd) {
  calclooseper(pd);
  stacksetval pw(pd);
  pd.rotinv.clear();
  map<vector<loosetype>, int> rotlook;
  vector<loosetype> enc(looseiper);
  for (int m = 0; m < (int)pd.rotgroup.size(); m++) {
    loosepack(pd, pd.rotgroup[m].pos, enc.data(), 1);
    rotlook[enc] = m;
  }
  for (int m = 0; m < (int)pd.rotgroup.size(); m++) {
    pd.inv(pd.rotgroup[m].pos, pw);
    loosepack(pd, pw, enc.data(), 1);
    pd.rotinv.push_back(rotlook[enc]);
  }
  // rotinvmap is used as follows:
  //   If pd.rotgroup[i] * pd.solved * p1 == p2, then
  //      pd.rotinvmap[i] * (pd.solved * p1) = p2
  //  That is, rotinvmap allows us to symmetry-reduce *positions*,
  //  not just moves.
  //  Note however that pd.rotinvmap entries should *only* be
  //  applied to *positions* and never moves.
  pd.rotinvmap.clear();
  for (int i = 0; i < (int)pd.rotgroup.size(); i++) {
    const setval &roti = pd.rotgroup[pd.rotinv[i]].pos;
    pd.rotinvmap.push_back(allocsetval(pd, roti));
    auto &rotw = pd.rotinvmap[i];
    for (int j = 0; j < (int)pd.setdefs.size(); j++) {
      setdef &sd = pd.setdefs[j];
      int n = sd.size;
      int off = sd.off;
      for (int k = 0; k < n; k++)
        rotw.dat[off + k] = 255;
      for (int k = 0; k < n; k++) {
        rotw.dat[off + pd.solved.dat[off + k]] =
            pd.solved.dat[off + roti.dat[off + k]];
        rotw.dat[off + n + pd.solved.dat[off + k]] =
            (pd.solved.dat[off + n + roti.dat[off + k]] +
             roti.dat[off + n + k] - pd.solved.dat[off + n + k] + sd.omod) %
            sd.omod;
      }
    }
  }
  if (pd.rotinv.size() != pd.rotgroup.size())
    error("! error looking for rotation inverses");
}
void calcrotations(puzdef &pd) {
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    setdef &sd = pd.setdefs[i];
    if (sd.omod != 1 && !sd.uniq) {
      auto solv = pd.solved.dat;
      int n = sd.size;
      for (int j = 0; j < n; j++) {
        if (solv[sd.off + n + j] != 0 && solv[sd.off + n + j] != 2 * sd.omod) {
          warn("Can't use rotations for symmetry reduction when oriented "
               "duplicated pieces that start in a non-zero, non-wildcard "
               "state.");
          return;
        }
      }
    }
  }
  stacksetval pw(pd);
  vector<moove> &q = pd.rotgroup;
  set<vector<uchar>> seen;
  seen.insert(setvaltovec(pd, pd.id));
  moove m(pd, pd.id);
  m.name = "(identity)";
  m.cost = 0;
  m.twist = 0;
  q.push_back(m);
  for (int qg = 0; qg < (int)q.size(); qg++) {
    for (int i = 0; i < (int)pd.rotations.size(); i++) {
      vector<uchar> t(pd.totsize);
      setval w(t.data());
      pd.mul(q[qg].pos, pd.rotations[i].pos, w);
      if (seen.find(t) == seen.end()) {
        seen.insert(t);
        m.name = "(rotation)";
        m.pos = allocsetval(pd, w);
        q.push_back(m);
      }
    }
  }
  calcrotinvs(pd);
  /*
   *   Filter the rotgroup to preserve:
   *      Identical piece chunks from solved
   *      Move restrictions (each move must map to another move)
   */
  vector<moove> filtered;
  int remap[256];
  for (int i = 0; i < (int)q.size(); i++) {
    pd.mul(pd.solved, q[i].pos, pw);
    int good = 1;
    for (int j = 0; good && j < (int)pd.setdefs.size(); j++) {
      setdef &sd = pd.setdefs[j];
      int n = sd.size;
      int off = sd.off;
      for (int k = 0; k < n; k++)
        remap[k] = -1;
      for (int k = 0; k < n; k++) {
        int oldv = pd.solved.dat[off + k];
        int newv = pw.dat[off + k];
        if (remap[oldv] < 0) {
          remap[oldv] = newv;
        } else if (remap[oldv] != newv) {
          good = 0;
        }
      }
    }
    for (int j = 0; good && j < (int)pd.moves.size(); j++) {
      pd.mul3(q[pd.rotinv[i]].pos, pd.moves[j].pos, q[i].pos, pw);
      int found = -1;
      for (int k = 0; k < (int)pd.moves.size(); k++) {
        if (pd.comparepos(pw, pd.moves[k].pos) == 0) {
          found = k;
          break;
        }
      }
      if (found < 0) {
        good = 0;
      }
    }
    if (good)
      filtered.push_back(q[i]);
  }
  swap(q, filtered);
  calcrotinvs(pd);
  if (quiet == 0)
    cout << "Rotation group size is " << q.size() << endl;
  // test that for a random p,
  //  solved * (rotinv * p) == rotinvmap * (solved * p)
  /*
  pd.assignpos(pw, pd.id) ;
  stacksetval p2(pd), p3(pd), p4(pd) ;
  for (int i=0; i<1000; i++) {
     int mv = myrand(pd.moves.size()) ;
     domove(pd, pw, mv) ;
     int r = myrand(pd.rotgroup.size()) ;
     pd.mul(pd.rotgroup[pd.rotinv[r]].pos, pw, p2) ;
     pd.mul(pd.solved, p2, p3) ;
     pd.mul(pd.solved, pw, p2) ;
     pd.mul(pd.rotinvmap[r], p2, p4) ;
     cout << i << " " << pd.comparepos(p3, p4) << endl ;
  }
  cout << "Rotations tested." << endl ;
  */
}
void showpos(const puzdef &pd, const setval s) {
  for (int i = 0; i < pd.totsize; i++)
    cout << " " << (int)s.dat[i];
  cout << endl;
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
  int cnt = -1;
  stacksetval s1(pd), s2(pd);
  int v0 = 1000, v1 = 1000;
  // cout << "Doing " ; showpos(pd, p1) ;
  for (int m1 = 0; m1 < (int)pd.rotgroup.size(); m1++) {
    pd.mul(pd.rotgroup[m1].pos, p1, s1);
    int m2 = pd.rotinv[m1];
    {
      //    for (int m2=0; m2<(int)pd.rotgroup.size(); m2++) {
      int t = s1.dat[pd.rotgroup[m2].pos.dat[0]] - v0;
      if (t > 0)
        continue;
      if (t == 0 && pd.setdefs[0].size > 1) {
        t = s1.dat[pd.rotgroup[m2].pos.dat[1]] - v1;
        if (t > 0)
          continue;
      }
      if (t < 0) {
        pd.mul(s1, pd.rotgroup[m2].pos, p2);
        cnt = 1;
        v0 = p2.dat[0];
        v1 = p2.dat[1];
      } else {
        t = pd.mulcmp(s1, pd.rotgroup[m2].pos, p2);
        if (t <= 0) {
          if (t < 0) {
            cnt = 1;
            v0 = p2.dat[0];
            v1 = p2.dat[1];
          } else
            cnt++;
        }
      }
    }
  }
  // cout << "Returning count of " << cnt << endl ;
  return cnt;
}
int slowmodm(const puzdef &pd, const setval p1, setval p2) {
  int cnt = -1;
  int v0 = 1000, v1 = 1000;
  for (int m1 = 0; m1 < (int)pd.rotgroup.size(); m1++) {
    int m2 = pd.rotinv[m1];
    int t = pd.rotgroup[m1].pos.dat[p1.dat[pd.rotgroup[m2].pos.dat[0]]] - v0;
    if (t > 0)
      continue;
    if (t == 0 && pd.setdefs[0].size > 1) {
      t = pd.rotgroup[m1].pos.dat[p1.dat[pd.rotgroup[m2].pos.dat[1]]] - v1;
      if (t > 0)
        continue;
    }
    if (t < 0) {
      pd.mul3(pd.rotgroup[m1].pos, p1, pd.rotgroup[m2].pos, p2);
      cnt = 1;
      v0 = p2.dat[0];
      v1 = p2.dat[1];
    } else {
      t = pd.mulcmp3(pd.rotgroup[m1].pos, p1, pd.rotgroup[m2].pos, p2);
      if (t <= 0) {
        if (t < 0) {
          cnt = 1;
          v0 = p2.dat[0];
          v1 = p2.dat[1];
        } else
          cnt++;
      }
    }
  }
  // cout << "Returning count of " << cnt << endl ;
  return cnt;
}
int slowmodmip(const puzdef &pd, const setval p1, setval p2,
               const vector<moove> &rotgroup, const vector<int> &rotinv) {
  if (rotgroup.size() == 0) {
    pd.mul(pd.solved, p1, p2);
    return 1;
  }
  int cnt = -1;
  int v0 = 1000, v1 = 1000;
  stacksetval pw(pd);
  for (int m1 = 0; m1 < (int)rotgroup.size(); m1++) {
    int m2 = rotinv[m1];
    int t =
        pd.solved.dat[rotgroup[m1].pos.dat[p1.dat[rotgroup[m2].pos.dat[0]]]] -
        v0;
    if (t > 0)
      continue;
    if (t == 0 && pd.setdefs[0].size > 1) {
      t = pd.solved.dat[rotgroup[m1].pos.dat[p1.dat[rotgroup[m2].pos.dat[1]]]] -
          v1;
      if (t > 0)
        continue;
    }
    if (t < 0) {
      pd.mul(pd.solved, rotgroup[m1].pos, pw);
      pd.mul3(pw, p1, rotgroup[m2].pos, p2);
      cnt = 1;
      v0 = p2.dat[0];
      v1 = p2.dat[1];
    } else {
      pd.mul(pd.solved, rotgroup[m1].pos, pw);
      t = pd.mulcmp3(pw, p1, rotgroup[m2].pos, p2);
      if (t <= 0) {
        if (t < 0) {
          cnt = 1;
          v0 = p2.dat[0];
          v1 = p2.dat[1];
        } else
          cnt++;
      }
    }
  }
  // cout << "Returning count of " << cnt << endl ;
  return cnt;
}
int slowmodmip(const puzdef &pd, const setval p1, setval p2) {
  return slowmodmip(pd, p1, p2, pd.rotgroup, pd.rotinv);
}
//  This should generally work on positions.
int slowmodm2(const puzdef &pd, const setval p1, setval p2) {
  int cnt = 1;
  if (pd.rotgroup.size() <= 64) {
    ull lobits = pd.lowsymmbits(p1);
    int g = ffsll(lobits) - 1;
    pd.mul3(pd.rotinvmap[g], p1, pd.rotgroup[g].pos, p2);
    lobits &= ~(1LL << g);
    while (lobits) {
      g = ffsll(lobits) - 1;
      lobits &= ~(1LL << g);
      int t = pd.mulcmp3(pd.rotinvmap[g], p1, pd.rotgroup[g].pos, p2);
      if (t <= 0) {
        if (t < 0) {
          cnt = 1;
        } else
          cnt++;
      }
    }
  } else {
    int g = pd.lowsymmguess(p1);
    pd.mul3(pd.rotinvmap[g], p1, pd.rotgroup[g].pos, p2);
    for (int m = g + 1; m < (int)pd.rotgroup.size(); m++) {
      if (p2.dat[0] != pd.rotinvmap[m].dat[p1.dat[pd.rotgroup[m].pos.dat[0]]])
        continue;
      int t = pd.mulcmp3(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, p2);
      if (t <= 0) {
        if (t < 0) {
          cnt = 1;
        } else
          cnt++;
      }
    }
  }
  return cnt;
}
