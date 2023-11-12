#include "generatingset.h"
#include "cmds.h"
#include <iostream>
bool generatingset::resolve(const setval p_) {
  stacksetval p(pd), t(pd);
  pd.assignpos(p, p_);
  for (int i = (int)pd.setdefs.size() - 1; i >= 0; i--) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    int s = 0;
    for (int j = 0; j < n; j++)
      s += p.dat[sd.off + j];
    if (s * 2 != n * (n - 1))
      error("! identical pieces during generating set resolve?");
    int off = (sd.off >> 1);
    for (int j = n - 1; j >= 0; j--) {
      if (p.dat[sd.off + j] != j || p.dat[sd.off + n + j] != 0) {
        int v = sd.omod * p.dat[sd.off + j] + p.dat[sd.off + n + j];
        if (!sgs[off + j][v].dat)
          return 0;
        pd.mul(sgsi[off + j][v], p, t);
        swap(p.dat, t.dat);
        if (p.dat[sd.off + j] != j || p.dat[sd.off + n + j] != 0)
          error("! misresolve");
      }
    }
  }
  return 1;
}
void generatingset::knutha(int k1, int k2, const setval &p) {
  int k = k2 + (pd.setdefs[k1].off >> 1);
  tk[k].push_back(allocsetval(pd, p));
  stacksetval p2(pd);
  for (int i = 0; i < (int)sgs[k].size(); i++)
    if (sgs[k][i].dat) {
      pd.mul(p, sgs[k][i], p2);
      knuthb(k1, k2, p2);
    }
}
void generatingset::knuthb(int k1, int k2, const setval &p) {
  const setdef &sd = pd.setdefs[k1];
  int k = k2 + (sd.off >> 1);
  int n = sd.size;
  int j = p.dat[sd.off + k2] * sd.omod + p.dat[sd.off + n + k2];
  stacksetval p2(pd);
  if (!sgs[k][j].dat) {
    sgs[k][j] = allocsetval(pd, p);
    sgsi[k][j] = allocsetval(pd, p);
    pd.inv(sgs[k][j], sgsi[k][j]);
    for (int i = 0; i < (int)tk[k].size(); i++) {
      pd.mul(tk[k][i], p, p2);
      knuthb(k1, k2, p2);
    }
    return;
  }
  pd.mul(sgsi[k][j], p, p2);
  if (p2.dat[sd.off + k2] != k2 || p2.dat[sd.off + n + k2] != 0) {
    error("! misresolve in knuthb");
  }
  if (!resolve(p2)) {
    if (k2 > 0)
      k2--;
    else {
      k1--;
      if (k1 < 0)
        error("! fell off end in knuthb");
      k2 = pd.setdefs[k1].size - 1;
    }
    knutha(k1, k2, p2);
  }
}
generatingset::generatingset(const puzdef &pd_) : pd(pd_), e(pd.id) {
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int sz = sd.size * sd.omod;
    for (int j = 0; j < sd.size; j++) {
      sgs.push_back(vector<allocsetval>(sz));
      sgsi.push_back(vector<allocsetval>(sz));
      tk.push_back(vector<allocsetval>(0));
      int at = sgs.size() - 1;
      sgs[at][j * sd.omod] = e;
      sgsi[at][j * sd.omod] = e;
    }
  }
  int oldprec = cout.precision();
  cout.precision(20);
  for (int i = 0; i < (int)pd.moves.size(); i++) {
    if (resolve(pd.moves[i].pos))
      continue;
    knutha(pd.setdefs.size() - 1, pd.setdefs[pd.setdefs.size() - 1].size - 1,
           pd.moves[i].pos);
    long double totsize = 1;
    for (int j = 0; j < (int)sgs.size(); j++) {
      int cnt = 0;
      for (int k = 0; k < (int)sgs[j].size(); k++)
        if (sgs[j][k].dat)
          cnt++;
      totsize *= cnt;
    }
    cout << "Adding move " << pd.moves[i].name << " extends size to " << totsize
         << endl;
  }
  cout.precision(oldprec);
}
static struct schreiersimscmd : cmd {
  schreiersimscmd()
      : cmd(0, "--schreiersims",
            "Run the Schreier-Sims algorithm to calculate the state\n"
            "space size of the puzzle.") {}
  virtual void parse_args(int *, const char ***) {}
  virtual void docommand(puzdef &pd) { new generatingset(pd); }
} registerme;
