#include "generatingset.h"
#include "cmds.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
bool generatingset::resolve(const setval p_, int nlev) {
  stacksetval p(pd), t(pd);
  pd.assignpos(p, p_);
  for (int L = 0; L < nlev; L++) {
    const setdef &sd = pd.setdefs[levels[L].first];
    int n = sd.size;
    int j = levels[L].second;
    if (p.dat[sd.off + j] != j || p.dat[sd.off + n + j] != 0) {
      int v = sd.omod * p.dat[sd.off + j] + p.dat[sd.off + n + j];
      int k = (sd.off >> 1) + j;
      if (!sgs[k][v].dat)
        return 0;
      pd.mul(sgsi[k][v], p, t);
      work += pd.totsize;
      swap(p.dat, t.dat);
      if (p.dat[sd.off + j] != j || p.dat[sd.off + n + j] != 0)
        error("! misresolve");
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
  return resolve(premul, nincluded);
}
void generatingset::knutha(int L, const setval &p) {
  int k = slot(L);
  tk[k].push_back(allocsetval(pd, p));
  stacksetval p2(pd);
  for (int i = 0; i < (int)sgs[k].size(); i++)
    if (sgs[k][i].dat && !overbudget()) {
      pd.mul(p, sgs[k][i], p2);
      knuthb(L, p2);
    }
}
void generatingset::knuthb(int L, const setval &p) {
  if (overbudget())
    return;
  work += pd.totsize;
  const setdef &sd = pd.setdefs[levels[L].first];
  int k = slot(L);
  int n = sd.size;
  int k2 = levels[L].second;
  int j = p.dat[sd.off + k2] * sd.omod + p.dat[sd.off + n + k2];
  stacksetval p2(pd);
  if (!sgs[k][j].dat) {
    sgs[k][j] = allocsetval(pd, p);
    sgsi[k][j] = allocsetval(pd, p);
    pd.inv(sgs[k][j], sgsi[k][j]);
    for (int i = 0; i < (int)tk[k].size(); i++) {
      pd.mul(tk[k][i], p, p2);
      knuthb(L, p2);
    }
    return;
  }
  pd.mul(sgsi[k][j], p, p2);
  if (p2.dat[sd.off + k2] != k2 || p2.dat[sd.off + n + k2] != 0) {
    error("! misresolve in knuthb");
  }
  if (!resolve(p2, levels.size())) {
    if (L + 1 >= (int)levels.size())
      error("! fell off end in knuthb");
    knutha(L + 1, p2);
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
    : pd(pd_), e(pd.id), nincluded(0) {
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    included.push_back(eligible(sd));
    if (!included[i])
      warn("Generating set cannot fully check set ",
           sd.name +
               (sd.uniq ? " (orientation wildcards)" : " (identical pieces)") +
               "; checking only piece counts and orientation sums");
    initbasiccheck(i);
  }
  // Every set goes in the chain if we can afford it, since the moves act
  // on all of them as permutations even when the pieces are identical.
  // The included sets are stabilized first, so the remaining levels are a
  // chain for the subgroup that leaves the included sets solved.
  for (int pass = 1; pass >= 0; pass--) {
    for (int i = (int)pd.setdefs.size() - 1; i >= 0; i--)
      if (included[i] == pass)
        for (int j = pd.setdefs[i].size - 1; j >= 0; j--)
          levels.push_back({i, j});
    if (pass)
      nincluded = levels.size();
  }
  fullchain = build();
  if (!fullchain) {
    levels.resize(nincluded);
    build();
  }
}
/*
 *   Build the chain on the current levels.  Gives up if that costs more
 *   than our budget, which only happens with excluded sets in the chain.
 */
bool generatingset::build() {
  sgs.clear();
  sgsi.clear();
  tk.clear();
  for (auto &sd : pd.setdefs)
    for (int j = 0; j < sd.size; j++) {
      sgs.push_back(vector<allocsetval>(sd.size * sd.omod));
      sgsi.push_back(vector<allocsetval>(sd.size * sd.omod));
      tk.push_back(vector<allocsetval>(0));
      sgs.back()[j * sd.omod] = e;
      sgsi.back()[j * sd.omod] = e;
    }
  if (levels.empty())
    return 1;
  work = 0;
  ostringstream msgs;
  msgs.precision(20);
  for (int i = 0; i < (int)pd.moves.size(); i++) {
    if (resolve(pd.moves[i].pos, levels.size()))
      continue;
    knutha(0, pd.moves[i].pos);
    if (overbudget())
      return 0;
    msgs << "Adding move " << pd.moves[i].name << " extends size to "
         << levelsize(0, levels.size()) << endl;
  }
  cout << msgs.str();
  return 1;
}
long double generatingset::levelsize(int from, int to, long double *lg) const {
  long double r = 1;
  if (lg)
    *lg = 0;
  // in set order, the reverse of the base
  for (int L = to - 1; L >= from; L--) {
    const auto &v = sgs[slot(L)];
    int cnt = 0;
    for (int k = 0; k < (int)v.size(); k++)
      if (v[k].dat)
        cnt++;
    r *= cnt;
    if (lg)
      *lg += log2l(cnt);
  }
  return r;
}
/*
 *   Count the distinct values the excluded sets take over the subgroup
 *   that leaves the included sets solved, by walking every element of
 *   that subgroup.  Each state of the puzzle is then a group element on
 *   the included sets times one of these.
 */
void generatingset::walkkernel(int L, const setval &p,
                               set<string> &seen) const {
  if (L == (int)levels.size()) {
    string key;
    for (int i = 0; i < (int)pd.setdefs.size(); i++)
      if (!included[i]) {
        const setdef &sd = pd.setdefs[i];
        key.append((const char *)p.dat + sd.off, 2 * sd.size);
      }
    seen.insert(key);
    return;
  }
  stacksetval p2(pd);
  for (const auto &u : sgs[slot(L)])
    if (u.dat) {
      pd.mul(p, u, p2);
      walkkernel(L + 1, p2, seen);
    }
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
  if (nincluded > 0) {
    long double lg;
    long double gsize = levelsize(0, nincluded, &lg);
    string names;
    for (int i = 0; i < (int)pd.setdefs.size(); i++)
      if (included[i])
        names += " " + pd.setdefs[i].name;
    cout << "Group size for" << names << " is ";
    showsize(gsize, lg);
    cout << endl;
    total *= gsize;
    totallg += lg;
  }
  if (find(included.begin(), included.end(), 0) == included.end()) {
    cout << "State size is ";
    showsize(total, totallg);
    cout << endl;
    cout.precision(oldprec);
    return;
  }
  string names;
  for (int i = 0; i < (int)pd.setdefs.size(); i++)
    if (!included[i])
      names += " " + pd.setdefs[i].name;
  long double klg = 0;
  long double ksize = levelsize(nincluded, levels.size(), &klg);
  if (fullchain && ksize <= KERNELWALKLIMIT) {
    set<string> seen;
    walkkernel(nincluded, pd.solved, seen);
    cout << "Count for" << names << " with the rest solved is " << seen.size()
         << endl;
    total *= seen.size();
    totallg += log2l(seen.size());
    cout << "State size is ";
    showsize(total, totallg);
    cout << endl;
    cout.precision(oldprec);
    return;
  }
  long double estlg = 0;
  long double esttotal = total, esttotallg = totallg;
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
    estlg += lg;
    esttotal *= ssize;
    esttotallg += lg;
  }
  // the excluded sets can take no more values than the subgroup has elements
  if (fullchain && klg < estlg) {
    cout << "Count for" << names << " with the rest solved is at most ";
    showsize(ksize, klg);
    cout << ", the size of the subgroup that leaves the rest solved" << endl;
    esttotal = total * ksize;
    esttotallg = totallg + klg;
  }
  total = esttotal;
  totallg = esttotallg;
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
            "orientation wildcards are counted exactly when the moves\n"
            "that leave the other sets solved form a small enough group;\n"
            "otherwise they are estimated, assuming any arrangement of\n"
            "pieces within each orbit of locations.") {}
  virtual void parse_args(int *, const char ***) {}
  virtual void docommand(puzdef &pd) { (new generatingset(pd))->showsizes(); }
} registerme;
