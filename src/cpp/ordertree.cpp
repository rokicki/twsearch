#include "ordertree.h"
#include "canon.h"
#include "cmds.h"
#include "findalgo.h"
#include <iostream>
#include <set>
static set<ll> seen;
static vector<allocsetval> posns;
static vector<int> movehist;
void recurorder(const puzdef &pd, int togo, int sp, int st) {
  if (togo == 0) {
    vector<int> cc = pd.cyccnts(posns[sp]);
    ll o = puzdef::order(cc);
    if (seen.find(o) == seen.end()) {
      seen.insert(o);
      cout << o;
      for (int i = 0; i < sp; i++)
        cout << " " << pd.moves[movehist[i]].name;
      cout << endl << flush;
    }
    return;
  }
  ull mask = canonmask[st];
  const vector<int> &ns = canonnext[st];
  for (int m = 0; m < (int)pd.moves.size(); m++) {
    const moove &mv = pd.moves[m];
    if ((mask >> mv.cs) & 1)
      continue;
    movehist[sp] = m;
    pd.mul(posns[sp], mv.pos, posns[sp + 1]);
    if (pd.legalstate(posns[sp + 1]))
      recurorder(pd, togo - 1, sp + 1, ns[mv.cs]);
  }
}
void ordertree(const puzdef &pd) {
  for (int d = 1;; d++) {
    posns.clear();
    movehist.clear();
    while ((int)posns.size() <= d + 1) {
      posns.push_back(allocsetval(pd, pd.id));
      movehist.push_back(-1);
    }
    recurorder(pd, d, 0, 0);
  }
}
static struct ordertreecmd : cmd {
  ordertreecmd()
      : cmd(0, "--ordertree",
            "Print shortest sequences of a particular order of the "
            "superpuzzle.") {}
  virtual void docommand(puzdef &pd) { ordertree(pd); }
} registerordertree;
