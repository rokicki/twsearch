#include "unrotate.h"
#include "cmdlineops.h"
#include "cmds.h"
#include "index.h"
#include "util.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
vector<loosetype> urenc;
unordered_map<vector<loosetype>, pair<int, int>, hashvector<loosetype>> urseen;
static vector<allocsetval> posns;
void urinsert(const puzdef &pd, int m1, int m2) {
  loosepack(pd, posns[0], urenc.data(), 1);
  auto it = urseen.find(urenc);
  if (it == urseen.end()) {
    urseen[urenc] = {m1, m2};
  }
}
static int ur_inited = 0;
void unrotate_setup(const puzdef &pd) {
  posns.clear();
  while (posns.size() <= 10)
    posns.push_back(allocsetval(pd, pd.id));
  urenc.resize(looseiper);
  pd.assignpos(posns[0], pd.id);
  urinsert(pd, -1, -1);
  int movesn = pd.moves.size();
  for (int i = 0; i < movesn; i++) {
    pd.assignpos(posns[0], pd.moves[i].pos);
    urinsert(pd, i, -1);
  }
  for (int i = 0; i < (int)pd.rotations.size(); i++) {
    pd.assignpos(posns[0], pd.rotations[i].pos);
    urinsert(pd, movesn + i, -1);
  }
  for (int i = 0; i < movesn; i++)
    for (int j = 0; j < (int)pd.rotations.size(); j++) {
      pd.mul(pd.moves[i].pos, pd.rotations[j].pos, posns[0]);
      urinsert(pd, i, movesn + j);
    }
}
vector<int> unrotate(const puzdef &pd, const vector<int> &orig) {
  if (!ur_inited) {
    unrotate_setup(pd);
    ur_inited = 1;
  }
  int movesn = pd.moves.size();
  vector<int> r;
  if (orig.size() < 2) {
    r = orig;
  } else {
    int a = orig[0];
    for (int i = 1; i < (int)orig.size(); i++) {
      int b = orig[i];
      if (a < 0) {
        a = b;
        continue;
      }
      pd.mul(a < movesn ? pd.moves[a].pos : pd.rotations[a - movesn].pos,
             b < movesn ? pd.moves[b].pos : pd.rotations[b - movesn].pos,
             posns[0]);
      loosepack(pd, posns[0], urenc.data(), 1);
      auto it = urseen.find(urenc);
      if (it != urseen.end()) {
        int na = it->second.first;
        int nb = it->second.second;
        if (na < 0) { // these cancel
          a = -1;
        } else if (nb < 0) { // these merge
          a = na;
        } else {
          r.push_back(na);
          a = nb;
        }
      } else { // no match
        r.push_back(a);
        a = b;
      }
    }
    if (a >= 0)
      r.push_back(a);
  }
  return r;
}
void unrotateit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    auto res = unrotate(pd, movelist);
    for (auto mvind : res)
      if (mvind < (int)pd.moves.size())
        cout << " " << pd.moves[mvind].name;
      else
        cout << " " << pd.rotations[mvind - pd.moves.size()].name;
  }
  cout << endl;
}
static struct unrotatecmd : cmd {
  unrotatecmd()
      : cmd("--unrotateseqs",
            "Read a set of move sequences on standard input and attempt\n"
            "to move all rotations to the end of the sequence.") {}
  virtual void docommand(puzdef &pd) { processlines4(pd, unrotateit); };
} registerunrotate;
