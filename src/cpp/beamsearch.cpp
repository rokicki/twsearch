#include "beamsearch.h"
#include "cmdlineops.h"
#include "cmds.h"
#include "index.h"
#include "prunetable.h" // for fasthash
#include "puzdef.h"
#include <iostream>
ll beamwidth, uniquesize;
using scoretype = short;
int score(const puzdef &pd, setval p) {
  return (pd.totsize >> 1) - pd.numwrong(pd.solved, p);
}
void beamsearch(const puzdef &pd, setval pos, const char *) {
  scoretype *dedup = (scoretype *)malloc(sizeof(scoretype) * uniquesize);
  for (ll i = 0; i < uniquesize; i++)
    dedup[i] = -1;
  vector<loosetype *> poslevels;
  vector<scoretype *> posscores;
  ll looseperlev = looseper * beamwidth;
  poslevels.push_back((loosetype *)calloc(sizeof(loosetype), looseperlev));
  posscores.push_back((scoretype *)malloc(sizeof(scoretype) * beamwidth));
  for (ll i = 0; i < beamwidth; i++)
    posscores[0][i] = -1;
  ull h = fasthash(pd.totsize, pos);
  int sc = score(pd, pos);
  ull bh = h % beamwidth;
  ull dh = h % uniquesize;
  posscores[0][bh] = sc;
  loosepack(pd, pos, poslevels[0] + bh * looseper, 0, 0);
  dedup[dh] = sc;
  int bestscore = sc;
  stacksetval srcpos(pd), dstpos(pd);
  for (int len = 1;; len++) {
    poslevels.push_back((loosetype *)calloc(sizeof(loosetype), looseperlev));
    posscores.push_back((scoretype *)malloc(sizeof(scoretype) * beamwidth));
    loosetype *dstloose = poslevels[len];
    scoretype *dstscore = posscores[len];
    loosetype *srcloose = poslevels[len - 1];
    scoretype *srcscore = posscores[len - 1];
    for (ll i = 0; i < beamwidth; i++)
      dstscore[i] = -1;
    ll seen = 0;
    for (ll i = 0; i < beamwidth; i++) {
      if (srcscore[i] < 0)
        continue;
      seen++;
      looseunpack(pd, srcpos, srcloose + i * looseper);
      for (int m = 0; m < (int)pd.moves.size(); m++) {
        pd.mul(srcpos, pd.moves[m].pos, dstpos);
        h = fasthash(pd.totsize, dstpos);
        sc = score(pd, dstpos);
        dh = h % uniquesize;
        if (dedup[dh] >= sc)
          continue;
        bh = h % beamwidth;
        if (dstscore[bh] >= sc)
          continue;
        loosepack(pd, dstpos, dstloose + bh * looseper, 0, 0);
        dstscore[bh] = sc;
        dedup[dh] = sc;
        if (sc > bestscore) {
          bestscore = sc;
          cout << "New bestscore of " << sc << " at length " << len << endl;
          if (sc * 2 == pd.totsize)
            return;
        }
      }
    }
    if (seen == 0)
      break;
    cout << "At depth " << len << " see " << seen << " in " << duration()
         << endl;
    // make it not use much memory; we will have to revisit this
    free(poslevels[len - 1]);
    free(posscores[len - 1]);
  }
}
static struct beamsearchcmd : cmd {
  beamsearchcmd()
      : cmd("--beamsearch", "Run beamsearch on the input positions") {}
  virtual void docommand(puzdef &pd) { processlines(pd, beamsearch); };
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)++;
    (*argv)++;
    beamwidth = atoll(**argv);
    (*argc)++;
    (*argv)++;
    uniquesize = atoll(**argv);
  }
} registerbeamsearch;
