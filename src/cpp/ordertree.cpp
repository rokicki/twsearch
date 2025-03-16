#include "ordertree.h"
#include "canon.h"
#include "cmds.h"
#include "findalgo.h"
#include <iostream>
#include <set>
static set<ll> seen;
static vector<allocsetval> posns;
static vector<int> movehist;
// Only consider a single rotation of any given sequence (i.e., if we
// look at ab, don't look at ba too).  This value should be either 0 or
// 1 for the code below to work correctly.
void recurorder(const puzdef &pd, int togo, int move_len, int current_fsm_state,
                int move_index) {
  if (togo == 0) {
    vector<int> cc = pd.cyccnts(posns[move_len]);
    ll o = puzdef::order(cc);
    if (seen.find(o) == seen.end()) {
      seen.insert(o);
      cout << o;
      for (int i = 0; i < move_len; i++)
        cout << " " << pd.moves[movehist[i]].name;
      cout << endl << flush;
    }
    return;
  }
  ull mask = canonmask[current_fsm_state];
  const vector<int> &nest_fsm_state = canonnext[current_fsm_state];
  int next_move_index = move_index + 1;
  int start = (move_index < 0 ? 0 : movehist[move_index]);
  for (int i = start; i < (int)pd.moves.size(); i++) {
    const moove &move_ = pd.moves[i];
    if ((mask >> move_.cs) & 1) {
      next_move_index = 0;
      continue;
    }
    movehist[move_len] = i;
    pd.mul(posns[move_len], move_.pos, posns[move_len + 1]);
    recurorder(pd, togo - 1, move_len + 1, nest_fsm_state[move_.cs],
               next_move_index);
    next_move_index = 0;
  }
}
void ordertree(const puzdef &pd) {
  for (int d = 0;; d++) {
    posns.clear();
    movehist.clear();
    while ((int)posns.size() <= d + 1) {
      posns.push_back(allocsetval(pd, pd.id));
      movehist.push_back(-1);
    }
    recurorder(pd, d, 0, 0, -1);
  }
}
static struct ordertreecmd : cmd {
  ordertreecmd()
      : cmd("--ordertree",
            "Print shortest sequences of a particular order of the "
            "superpuzzle.") {}
  virtual void docommand(puzdef &pd) { ordertree(pd); }
} registerordertree;
