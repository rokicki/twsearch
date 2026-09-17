#include "generatingset.h"
#include "cmds.h"
#include <cmath>
#include <iostream>
bool generatingset::resolve(const setval p_) {
  stacksetval p(pd), t(pd);
  pd.assignpos(p, p_);
  for (int i = (int)pd.setdefs.size() - 1; i >= 0; i--) {
    if (!included[i])
      continue;
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
bool generatingset::basiccheck(const setval p) const {
  vector<int> cnt;
  vector<int> ori;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    if (included[i])
      continue;
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    const uchar *pp = p.dat + sd.off;
    cnt = solvedcnt[i];
    ori.assign(orimod[i].size(), 0);
    for (int j = 0; j < n; j++) {
      if (--cnt[orbit[i][j] * n + pp[j]] < 0)
        return 0;
      ori[orbit[i][j]] += pp[j + n];
    }
    for (int k = 0; k < (int)ori.size(); k++)
      if ((ori[k] - solvedori[i][k]) % orimod[i][k] != 0)
        return 0;
  }
  return 1;
}
bool generatingset::solvable(const setval p) {
  if (!basiccheck(p))
    return 0;
  // excluded sets may not be permutations (identical pieces or orientation
  // wildcards), so replace them with the identity before inverting
  stacksetval sol(pd), pos(pd), solinv(pd), premul(pd);
  pd.assignpos(sol, pd.solved);
  pd.assignpos(pos, p);
  for (int i = 0; i < (int)pd.setdefs.size(); i++)
    if (!included[i]) {
      const setdef &sd = pd.setdefs[i];
      for (int j = 0; j < 2 * sd.size; j++)
        sol.dat[sd.off + j] = pos.dat[sd.off + j] = e.dat[sd.off + j];
    }
  pd.inv(sol, solinv);
  pd.mul(solinv, pos, premul);
  return resolve(premul);
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
      do
        k1--;
      while (k1 >= 0 && !included[k1]);
      if (k1 < 0)
        error("! fell off end in knuthb");
      k2 = pd.setdefs[k1].size - 1;
    }
    knutha(k1, k2, p2);
  }
}
static int findroot(vector<int> &parent, int x) {
  while (parent[x] != x)
    x = parent[x] = parent[parent[x]];
  return x;
}
void generatingset::initbasiccheck(int i) {
  orbit.push_back({});
  solvedcnt.push_back({});
  orimod.push_back({});
  solvedori.push_back({});
  if (included[i])
    return;
  const setdef &sd = pd.setdefs[i];
  int n = sd.size;
  vector<int> parent(n);
  for (int j = 0; j < n; j++)
    parent[j] = j;
  for (auto &mv : pd.moves)
    for (int j = 0; j < n; j++)
      parent[findroot(parent, j)] = findroot(parent, mv.pos.dat[sd.off + j]);
  vector<int> id(n, -1);
  int norbits = 0;
  for (int j = 0; j < n; j++) {
    int r = findroot(parent, j);
    if (id[r] < 0)
      id[r] = norbits++;
    orbit[i].push_back(id[r]);
  }
  const uchar *sp = pd.solved.dat + sd.off;
  solvedcnt[i].assign(norbits * n, 0);
  for (int j = 0; j < n; j++)
    solvedcnt[i][orbit[i][j] * n + sp[j]]++;
  // the orientation sum over an orbit changes by the sum of the move's
  // orientation changes over it, so it is fixed modulo the gcd of those
  orimod[i].assign(norbits, sd.omod);
  solvedori[i].assign(norbits, 0);
  for (int j = 0; j < n; j++) {
    solvedori[i][orbit[i][j]] += sp[j + n];
    if (sp[j + n] >= sd.omod) // wildcard
      orimod[i][orbit[i][j]] = 1;
  }
  for (auto &mv : pd.moves) {
    vector<int> delta(norbits, 0);
    for (int j = 0; j < n; j++)
      delta[orbit[i][j]] += mv.pos.dat[sd.off + n + j];
    for (int k = 0; k < norbits; k++)
      orimod[i][k] = gcd(orimod[i][k], delta[k] % sd.omod);
  }
}
generatingset::generatingset(const puzdef &pd_)
    : pd(pd_), e(pd.id), lastincluded(-1) {
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    included.push_back(eligible(sd));
    if (included[i])
      lastincluded = i;
    else
      warn("Generating set ignores set ",
           sd.name +
               (sd.uniq ? " (orientation wildcards)" : " (identical pieces)") +
               "; checking only piece counts and orientation sums");
    initbasiccheck(i);
    // excluded sets keep their (empty) slots so offsets stay the same
    int sz = included[i] ? sd.size * sd.omod : 0;
    for (int j = 0; j < sd.size; j++) {
      sgs.push_back(vector<allocsetval>(sz));
      sgsi.push_back(vector<allocsetval>(sz));
      tk.push_back(vector<allocsetval>(0));
      if (!included[i])
        continue;
      int at = sgs.size() - 1;
      sgs[at][j * sd.omod] = e;
      sgsi[at][j * sd.omod] = e;
    }
  }
  if (lastincluded < 0)
    return;
  int oldprec = cout.precision();
  cout.precision(20);
  for (int i = 0; i < (int)pd.moves.size(); i++) {
    if (resolve(pd.moves[i].pos))
      continue;
    knutha(lastincluded, pd.setdefs[lastincluded].size - 1, pd.moves[i].pos);
    long double totsize = 1;
    for (int j = 0; j < (int)sgs.size(); j++) {
      int cnt = 0;
      for (int k = 0; k < (int)sgs[j].size(); k++)
        if (sgs[j][k].dat)
          cnt++;
      if (cnt)
        totsize *= cnt;
    }
    cout << "Adding move " << pd.moves[i].name << " extends size to " << totsize
         << endl;
  }
  cout.precision(oldprec);
}
/*
 *   Print the size of the group on the included sets, and an estimate for
 *   the excluded sets assuming their pieces can be arranged in any way
 *   within each orbit, with any orientations whose sum over the orbit
 *   matches what the moves allow.  Sizes are kept as logs too, since
 *   large puzzles overflow a double.
 */
static void showsize(long double v, long double lg) {
  if (isfinite(v)) {
    cout << v;
  } else {
    long double e = floorl(lg / log2l(10));
    cout << "about " << powl(2, lg - e * log2l(10)) << " x 10^" << (long long)e;
  }
}
void generatingset::showsizes() const {
  int oldprec = cout.precision();
  cout.precision(20);
  long double total = 1, totallg = 0;
  if (lastincluded >= 0) {
    long double gsize = 1, lg = 0;
    string names;
    for (int i = 0; i < (int)pd.setdefs.size(); i++) {
      if (!included[i])
        continue;
      const setdef &sd = pd.setdefs[i];
      names += " " + sd.name;
      for (int j = 0; j < sd.size; j++) {
        const auto &v = sgs[(sd.off >> 1) + j];
        int cnt = 0;
        for (int k = 0; k < (int)v.size(); k++)
          if (v[k].dat)
            cnt++;
        gsize *= cnt;
        lg += log2l(cnt);
      }
    }
    cout << "Group size for" << names << " is ";
    showsize(gsize, lg);
    cout << endl;
    total *= gsize;
    totallg += lg;
  }
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    if (included[i])
      continue;
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    int norbits = orimod[i].size();
    long double ssize = 1, lg = 0;
    bool distinctorbit = false;
    for (int k = 0; k < norbits; k++) {
      // multinomial: orbit size choose the counts of each piece
      int placed = 0, labels = 0;
      for (int lab = 0; lab < n; lab++) {
        int c = solvedcnt[i][k * n + lab];
        for (int t = 1; t <= c; t++) {
          ssize = ssize * (placed + t) / t;
          lg += log2l(placed + t) - log2l(t);
        }
        placed += c;
        if (c)
          labels++;
      }
      if (placed > 1 && labels == placed)
        distinctorbit = true;
      if (sd.omod > 1) {
        for (int j = 0; j < n; j++)
          if (orbit[i][j] == k && pd.solved.dat[sd.off + n + j] < sd.omod) {
            ssize *= sd.omod;
            lg += log2l(sd.omod);
          }
        ssize /= orimod[i][k];
        lg -= log2l(orimod[i][k]);
      }
    }
    cout << "Estimated count for " << sd.name << " (" << norbits
         << (norbits == 1 ? " orbit" : " orbits") << ") is ";
    showsize(ssize, lg);
    cout << endl;
    if (distinctorbit)
      cout << "Note: " << sd.name
           << " has an orbit of distinct pieces; permutation parity is not "
              "accounted for"
           << endl;
    total *= ssize;
    totallg += lg;
  }
  cout << "State size is ";
  showsize(total, totallg);
  cout << endl;
  cout.precision(oldprec);
}
static struct schreiersimscmd : cmd {
  schreiersimscmd()
      : cmd("--schreiersims",
            "Run the Schreier-Sims algorithm to calculate the state\n"
            "space size of the puzzle.  Sets with identical pieces or\n"
            "orientation wildcards are instead estimated, assuming any\n"
            "arrangement of pieces within each orbit of locations.") {}
  virtual void parse_args(int *, const char ***) {}
  virtual void docommand(puzdef &pd) { (new generatingset(pd))->showsizes(); }
} registerme;
