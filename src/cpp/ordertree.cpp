#include "ordertree.h"
#include "canon.h"
#include "findalgo.h"
#include <iostream>
#include <map>
const int MOSN = 6 * 6 * 6 * 6 * 6 * 6 * 6 * 6;
static unsigned char oneside[MOSN], twoside[MOSN];
static int osseq, tsseq;
static int mape[] = {16, 19, 18, 17};
static int mapc[] = {17, 16, 19, 18};
void recurorder(const puzdef &pd, int togo, int sp, int st) {
  if (togo == 0) {
    int o = 0;
    unsigned char *dat = posns[sp].dat;
    for (int i = 8; i < 12; i++) {
      o = 36 * o + 6 * dat[i] + dat[48 + i];
      ;
    }
    if (oneside[o] == 0) {
      oneside[o] = 1;
      osseq++;
      cout << "ONE " << osseq;
      for (int i = 0; i < sp; i++)
        cout << " " << pd.moves[movehist[i]].name;
      cout << endl << flush;
    }
    if (twoside[o] == 0) {
      int good = 1;
      for (int i = 8; i < 12; i++)
        if (dat[i] + dat[mapc[i - 8]] != 5 ||
            dat[48 + i] + dat[mape[i - 8] + 48] != 5)
          good = 0;
      if (good) {
        twoside[o] = 1;
        tsseq++;
        cout << "TWO " << tsseq;
        for (int i = 0; i < sp; i++)
          cout << " " << pd.moves[movehist[i]].name;
        cout << endl << flush;
      }
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
      posns.push_back(allocsetval(pd, pd.solved));
      movehist.push_back(-1);
    }
    recurorder(pd, d, 0, 0);
  }
}
