#include "cmds.h"
#include "index.h"
#include "puzdef.h"
#include <iostream>
using namespace std;
void totalvar(const puzdef &pd) {
  if (!pd.canpackdense())
    error("! Can only calculate total variation on puzzles that are densely "
          "indexable.");
  ll n = pd.llstates;
  vector<double> dist(n);
  vector<double> ndist(n);
  dist[densepack(pd, pd.solved)] = 1.0;
  double expected = 1.0 / n;
  double permov = 1.0 / pd.moves.size();
  stacksetval src(pd), dst(pd);
  for (int nmoves = 0; nmoves < 100; nmoves++) {
    double tv = 0;
    for (auto v : dist)
      tv += abs(expected - v);
    tv *= 0.5;
    cout << nmoves << " " << tv << endl;
    for (ll i = 0; i < n; i++) {
      double per = dist[i] * permov;
      denseunpack(pd, i, src);
      for (auto &mv : pd.moves) {
        pd.mul(src, mv.pos, dst);
        ndist[densepack(pd, dst)] += per;
      }
    }
    swap(dist, ndist);
    for (auto &v : ndist)
      v = 0;
  }
}
static struct totalvarcmd : cmd {
  totalvarcmd() : cmd("--totalvar", "Print total variation for move count.") {}
  virtual void docommand(puzdef &pd) { totalvar(pd); }
} registertotalvar;
