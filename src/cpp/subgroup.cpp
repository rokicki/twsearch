#include "subgroup.h"
#include "antipode.h"   // just for antipode count
#include "cmdlineops.h" // for emitposition
#include "cmds.h"
#include "index.h"
#include "parsemoves.h"
#include "rotations.h" // for slowmodmip
#include "solve.h"
#include "util.h"
#include <algorithm>
#include <iostream>
#include <map>
using namespace std;
// from command line
const char *subgroupmovelist;
struct subgroupmovelistopt : stringopt {
  subgroupmovelistopt()
      : stringopt(
            "--subgroupmoves",
            "Target subgroup moves.  This changes the solved position\n"
            "from the one in the provided tws file by chasing orbits and\n"
            "using identical pieces and orientation wildcards, to make the\n"
            "target subgroup be that given by the moves listed.  This will\n"
            "not always work correctly if the subgroup has nontrivial\n"
            "structure, like the squares group on the 3x3x3.",
            &subgroupmovelist) {}
} subgroupmovelistoptinst;
/*
 *   This patches up the solved state for the subgroup defined by the
 *   given move set.
 */
void runsubgroup(puzdef &pd) {
  if (!pd.invertible())
    error("! puzzle must be invertible to solve to a subgroup");
  /* parse the move list. */
  auto moves = parsemovelist(pd, subgroupmovelist);
  stacksetval osolved(pd), nsolved(pd);
  pd.addoptionssum("subgroup");
  pd.addoptionssum(subgroupmovelist);
  pd.assignpos(osolved, pd.solved);
  vector<allocsetval> q;
  q.push_back(allocsetval(pd, pd.id));
  stacksetval p2(pd);
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    setdef &sd = pd.setdefs[i];
    // we want to do a BFS on all points/orientations to calculate
    // the orbits of each, so we can rewrite the solved state.
    // Note that there may be multiple independent sets of orbits
    // we rewrite.  We use a vector of a vector of states.
    int sz = sd.omod * sd.size;
    int n = sd.size;
    vector<vector<char>> reach(n, vector<char>(sz, 0));
    int qe = 1;
    uchar *p = q[0].dat + sd.off;
    for (int j = 0; j < n; j++)
      reach[j][p[j] * sd.omod + p[j + n]] = 1;
    for (int qg = 0; qg < qe; qg++) {
      for (int m = 0; m < (int)moves.size(); m++) {
        pd.mul(q[qg], pd.moves[moves[m]].pos, p2);
        int isnew = 0;
        p = p2.dat + sd.off;
        for (int j = 0; j < n; j++)
          if (reach[j][p[j] * sd.omod + p[j + n]] == 0) {
            reach[j][p[j] * sd.omod + p[j + n]] = 1;
            isnew = 1;
          }
        if (isnew) {
          if (qe >= (int)q.size())
            q.push_back(allocsetval(pd, p2));
          pd.assignpos(q[qe], p2);
          qe++;
        }
      }
    }
    if (verbose > 2) {
      cout << "For set " << sd.name << " queue " << qe << endl;
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < sz; k++)
          cout << (int)reach[j][k];
        cout << endl;
      }
    }
    // now we rewrite solved with new values.
    int nv = 0;
    vector<int> remap(n, -1);
    sd.psum = 0;
    sd.oparity = 0;
    for (int j = 0; j < n; j++) {
      if (remap[j] < 0) {
        int enablewo = 0;
        int cnt = 0;
        for (int k = 0; k < sd.omod; k++)
          if (reach[j][j * sd.omod + k])
            cnt++;
        if (cnt != 1 && cnt != sd.omod)
          error("! this move subset can twist, but can't twist all");
        if (cnt > 1)
          enablewo = 1;
        if (enablewo) {
          sd.wildo = 1;
          pd.wildo = 1;
          pd.caninvert = 0;
          pd.doubleprobe = 0;
        }
        for (int k = j; k < n; k++) {
          int hits = 0;
          for (int m = 0; m < sd.omod; m++)
            if (reach[j][k * sd.omod + m])
              hits = 1;
          if (hits) {
            remap[k] = nv;
            pd.solved.dat[sd.off + k] = nv;
            if (enablewo)
              pd.solved.dat[sd.off + k + n] = 2 * sd.omod;
          }
        }
        nv++;
      }
      sd.psum += pd.solved.dat[sd.off + j];
      sd.oparity += pd.solved.dat[sd.off + n + j];
    }
    if (nv != n) {
      sd.uniq = 0;
      pd.uniq = 0;
      pd.caninvert = 0;
      pd.doubleprobe = 0;
      sd.pparity = 0;
      sd.cnts.resize(nv);
      for (int k = 0; k < nv; k++)
        sd.cnts[k] = 0;
      for (int k = 0; k < n; k++)
        sd.cnts[pd.solved.dat[sd.off + k]]++;
      sd.pbits = ceillog2(sd.cnts.size());
    }
    if (sd.omod == 1 || (!sd.wildo && sd.oparity % sd.omod != 0)) {
      sd.oparity = 1;
    } else {
      sd.oparity = 0;
    }
    if (sd.wildo)
      sd.obits = ceillog2(sd.omod + 1);
  }
  if (verbose > 2) {
    for (int i = 0; i < pd.totsize; i++)
      cout << " " << (int)pd.solved.dat[i];
    cout << endl;
  }
  if (verbose)
    emitsolved(pd, pd.solved, 0);
}
