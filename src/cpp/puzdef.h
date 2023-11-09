#ifndef PUZDEF_H
#include "util.h"
#include <math.h>
#include <strings.h>
#include <vector>
using namespace std;
/*
 *   This is the core code, where we store a puzzle definition and
 *   the values for a puzzle.  The puzzle definition is a sequence
 *   of sets.  Each set has a permutation and orientation component.
 *   Values are stored as a sequence of uchars; first comes the
 *   permutation component and then the orientation component.
 *
 *   Right now the code is simple and general; it is likely we can
 *   gain a fair bit by specializing specific cases.
 */
extern double dllstates;
/*
 *   gmoda is used to calculate orientations for a given count of
 *   orientations.  Let's say we're working on a case where there
 *   are o orientations.  The first 2o values are simply mod o.
 *   To support don't care orientations without impacting branch
 *   prediction or instruction count, values 2o..4o-1 are simply 2o.
 *   Setting the orientation *value* then to 2o means that any
 *   twist leaves it at 2o.  This is a bit of a hack but lets us do
 *   don't care orientations without changing any of the normal
 *   straight line code.  Note that this affects indexing, so we
 *   make the indexing functions check and blow up if needed.
 */
extern uchar *gmoda[256];
struct setdef {
  int size, off;
  string name;
  uchar omod;
  int pbits, obits, pibits, psum;
  bool dense, uniq, pparity, oparity, wildo;
  double logstates;
  unsigned long long llperms, llords, llstates;
  vector<int> cnts; // only not empty when not unique.
  setdef()
      : size(0), off(0), name(), omod(0), pbits(0), obits(0), pibits(0),
        psum(0), uniq(1), pparity(0), oparity(0), wildo(0), logstates(0),
        llperms(0), llords(0), llstates(0), cnts() {}
  void mulp(const uchar *ap, const uchar *bp, uchar *cp) const {
    for (int j = 0; j < size; j++)
      cp[j] = ap[bp[j]];
  }
  // the right side must be a move so we can access the permutation part
  void mulo(const uchar *ap, const uchar *bp, uchar *cp) const {
    if (omod > 1) {
      uchar *moda = gmoda[omod];
      for (int j = 0; j < size; j++)
        cp[j] = moda[ap[bp[j - size]] + bp[j]];
    } else {
      for (int j = 0; j < size; j++)
        cp[j] = 0;
    }
  }
};
typedef vector<setdef> setdefs_t;
struct setval {
  setval() : dat(0) {}
  setval(uchar *dat_) : dat(dat_) {}
  uchar *dat;
};
struct illegal_t {
  int pos;
  ull mask;
};
struct puzdef;
//  These cannot be copied or assigned
struct stacksetval : setval {
  stacksetval(const puzdef &pd);
  stacksetval(const puzdef &pd, const setval iv);
  stacksetval(const stacksetval &) = delete;
  stacksetval(stacksetval &&) = delete;
  stacksetval &operator=(const stacksetval &) = delete;
  stacksetval &operator=(stacksetval &&) = delete;
  ~stacksetval() { delete[] dat; }
  const puzdef *owner;
};
struct allocsetval : setval {
  allocsetval() : setval(0), sz(0) {}
  allocsetval(const puzdef &pd, int);
  allocsetval(const puzdef &pd, const setval &iv);
  allocsetval(const allocsetval &);
  allocsetval(allocsetval &&);
  allocsetval &operator=(const allocsetval &);
  allocsetval &operator=(allocsetval &&);
  ~allocsetval() {
    if (dat) {
      delete[] dat;
      dat = 0;
    }
  }
  int sz;
};
struct moove {
  moove(const puzdef &pd, const setval &iv) : name(), pos(pd, iv), cost(1) {}
  string name;
  allocsetval pos;
  int cost, base, twist, cs;
};
extern int origroup;
struct movealias {
  string src, dst;
};
struct puzdef {
  puzdef()
      : name(), setdefs(), solved(), totsize(0), id(), logstates(0),
        llstates(0), checksum(0), haveillegal(0), wildo(0), dense(1), uniq(1) {}
  string name;
  setdefs_t setdefs;
  allocsetval solved;
  vector<moove> basemoves, moves, parsemoves, rotations, expandedrotations,
      rotgroup;
  vector<movealias> aliases;
  vector<movealias> moveseqs;
  vector<allocsetval> rotinvmap;
  vector<int> basemoveorders, baserotorders;
  vector<int> rotinv;
  vector<ull> commutes;
  int totsize;
  int ncs;
  allocsetval id;
  double logstates;
  unsigned long long llstates;
  ull checksum;
  ull optionssum;
  vector<illegal_t> illegal;
  char haveillegal, wildo, dense, uniq;
  int comparepos(const setval a, const setval b) const {
    return memcmp(a.dat, b.dat, totsize);
  }
  int canpackdense() const { return dense; }
  int invertible() const { return uniq; }
  void assignpos(setval a, const setval b) const {
    memcpy(a.dat, b.dat, totsize);
  }
  void addoptionssum(const char *p) {
    while (*p)
      optionssum = 37 * optionssum + *p++;
  }
  int numwrong(const setval a, const setval b, ull mask = -1) const;
  int permwrong(const setval a, const setval b, ull mask = -1) const;
  vector<int> cyccnts(const setval a, ull sets = -1) const;
  static ll order(const vector<int> cc);
  vector<allocsetval> stacksetvals;
  void mul(const setval a, const setval b, setval c) const {
    const uchar *ap = a.dat;
    const uchar *bp = b.dat;
    uchar *cp = c.dat;
    for (int i = 0; i < (int)setdefs.size(); i++) {
      const setdef &sd = setdefs[i];
      int n = sd.size;
      if (sd.omod > 1) {
        uchar *moda = gmoda[sd.omod];
        for (int j = 0; j < n; j++) {
          cp[j] = ap[bp[j]];
          cp[j + n] = moda[ap[bp[j] + n] + bp[j + n]];
        }
      } else {
        for (int j = 0; j < n; j++) {
          cp[j] = ap[bp[j]];
          cp[j + n] = 0;
        }
      }
      ap += 2 * n;
      bp += 2 * n;
      cp += 2 * n;
    }
  }
  void mul3(const setval a, const setval b, const setval c, setval d) const {
    const uchar *ap = a.dat;
    const uchar *bp = b.dat;
    const uchar *cp = c.dat;
    uchar *dp = d.dat;
    memset(dp, 0, totsize);
    for (int i = 0; i < (int)setdefs.size(); i++) {
      const setdef &sd = setdefs[i];
      int n = sd.size;
      if (sd.omod > 1) {
        uchar *moda = gmoda[sd.omod];
        for (int j = 0; j < n; j++) {
          dp[j] = ap[bp[cp[j]]];
          dp[j + n] = moda[ap[bp[cp[j]] + n] + moda[bp[cp[j] + n] + cp[j + n]]];
        }
      } else {
        for (int j = 0; j < n; j++) {
          dp[j] = ap[bp[cp[j]]];
          dp[j + n] = 0;
        }
      }
      ap += 2 * n;
      bp += 2 * n;
      cp += 2 * n;
      dp += 2 * n;
    }
  }
  // get a guess at the lowest symmetry by only looking at the
  // very first value.
  int lowsymmguess(const setval b) const {
    int r = 0;
    const uchar *bp = b.dat;
    int rv = rotinvmap[0].dat[bp[rotgroup[0].pos.dat[0]]];
    if (rv == 0)
      return 0;
    for (int m = 1; m < (int)rotgroup.size(); m++) {
      int t = rotinvmap[m].dat[bp[rotgroup[m].pos.dat[0]]];
      if (t < rv) {
        r = m;
        rv = t;
        if (rv == 0)
          return m;
      }
    }
    return r;
  }
  // If the number of symmetry conjugations is <= 64, we can use this
  // to quickly almost always get the single lowest state.
  ull lowsymmbits(const setval b) const {
    ull r = 1;
    const uchar *bp = b.dat;
    int rv = rotinvmap[0].dat[bp[rotgroup[0].pos.dat[0]]];
    for (int m = 1; m < (int)rotgroup.size(); m++) {
      int t = rotinvmap[m].dat[bp[rotgroup[m].pos.dat[0]]];
      if (t < rv) {
        r = 1LL << m;
        rv = t;
      } else if (t == rv)
        r |= (1LL << m);
    }
    for (int o = 1; o < setdefs[0].size; o++) {
      if ((r & (r - 1)) == 0)
        return r;
      int t = ffsll(r) - 1;
      ull r2 = 1LL << t;
      int rv = rotinvmap[t].dat[bp[rotgroup[t].pos.dat[o]]];
      r &= ~(1LL << t);
      while (r) {
        int m = ffsll(r) - 1;
        r &= ~(1LL << m);
        t = rotinvmap[m].dat[bp[rotgroup[m].pos.dat[o]]];
        if (t < rv) {
          r2 = 1LL << m;
          rv = t;
        } else if (t == rv)
          r2 |= 1LL << m;
      }
      r = r2;
    }
    return r;
  }
  // does a multiplication and a comparison at the same time.
  // c must be initialized already.
  int mulcmp3(const setval a, const setval b, const setval c, setval d) const {
    const uchar *ap = a.dat;
    const uchar *bp = b.dat;
    const uchar *cp = c.dat;
    uchar *dp = d.dat;
    int r = 0;
    for (int i = 0; i < (int)setdefs.size(); i++) {
      const setdef &sd = setdefs[i];
      int n = sd.size;
      for (int j = 0; j < n; j++) {
        int nv = ap[bp[cp[j]]];
        if (r > 0)
          dp[j] = nv;
        else if (nv > dp[j])
          return 1;
        else if (nv < dp[j]) {
          r = 1;
          dp[j] = nv;
        }
      }
      ap += n;
      bp += n;
      cp += n;
      dp += n;
      if (sd.omod > 1) {
        uchar *moda = gmoda[sd.omod];
        for (int j = 0; j < n; j++) {
          int nv = moda[ap[bp[cp[j - n] - n]] + moda[bp[cp[j - n]] + cp[j]]];
          if (r > 0)
            dp[j] = nv;
          else if (nv > dp[j])
            return 1;
          else if (nv < dp[j]) {
            r = 1;
            dp[j] = nv;
          }
        }
      }
      ap += n;
      bp += n;
      cp += n;
      dp += n;
    }
    return -r;
  }
  int mulcmp(const setval a, const setval b, setval c) const {
    const uchar *ap = a.dat;
    const uchar *bp = b.dat;
    uchar *cp = c.dat;
    int r = 0;
    for (int i = 0; i < (int)setdefs.size(); i++) {
      const setdef &sd = setdefs[i];
      int n = sd.size;
      for (int j = 0; j < n; j++) {
        int nv = ap[bp[j]];
        if (r > 0)
          cp[j] = nv;
        else if (nv > cp[j])
          return 1;
        else if (nv < cp[j]) {
          r = 1;
          cp[j] = nv;
        }
      }
      ap += n;
      bp += n;
      cp += n;
      if (sd.omod > 1) {
        uchar *moda = gmoda[sd.omod];
        for (int j = 0; j < n; j++) {
          int nv = moda[ap[bp[j - n]] + bp[j]];
          if (r > 0)
            cp[j] = nv;
          else if (nv > cp[j])
            return 1;
          else if (nv < cp[j]) {
            r = 1;
            cp[j] = nv;
          }
        }
      }
      ap += n;
      bp += n;
      cp += n;
    }
    return -r;
  }
  int legalstate(const setval a) const {
    if (!haveillegal)
      return 1;
    for (auto i : illegal) {
      if ((i.mask >> a.dat[i.pos]) & 1)
        return 0;
    }
    return 1;
  }
  int invmove(int mvind) const {
    const moove &mv = moves[mvind];
    int b = mv.base;
    int o = basemoveorders[b];
    int twist = (o - mv.twist) % o;
    return mvind - mv.twist + twist;
  }
  void addillegal(const string &setname, int pos, int val);
  void pow(const setval a, setval b, ll cnt) const;
  void inv(const setval a, setval b) const;
};
inline stacksetval::stacksetval(const puzdef &pd) : setval(0) {
  dat = new uchar[pd.totsize];
  owner = &pd;
  memcpy(dat, pd.id.dat, pd.totsize);
}
inline stacksetval::stacksetval(const puzdef &pd, const setval iv) : setval(0) {
  dat = new uchar[pd.totsize];
  owner = &pd;
  memcpy(dat, iv.dat, pd.totsize);
}
inline allocsetval::allocsetval(const puzdef &pd, const setval &iv)
    : setval(0) {
  dat = new uchar[pd.totsize];
  sz = pd.totsize;
  memcpy(dat, iv.dat, pd.totsize);
}
inline allocsetval::allocsetval(const puzdef &pd, int) : setval(0) {
  dat = new uchar[pd.totsize];
  sz = pd.totsize;
}
inline allocsetval::allocsetval(const allocsetval &v) : setval(0) {
  dat = new uchar[v.sz];
  sz = v.sz;
  memcpy(dat, v.dat, sz);
}
inline allocsetval::allocsetval(allocsetval &&v) : setval(0) {
  dat = v.dat;
  v.dat = 0;
  sz = v.sz;
}
inline allocsetval &allocsetval::operator=(const allocsetval &v) {
  dat = new uchar[v.sz];
  sz = v.sz;
  memcpy(dat, v.dat, sz);
  return *this;
}
inline allocsetval &allocsetval::operator=(allocsetval &&v) {
  dat = v.dat;
  v.dat = 0;
  sz = v.sz;
  return *this;
}
void calculatesizes(puzdef &pd);
void domove(const puzdef &pd, setval p, setval pos, setval pt);
void domove(const puzdef &pd, setval p, setval pos);
void domove(const puzdef &pd, setval p, int mv);
void domove(const puzdef &pd, setval p, int mv, setval pt);
#define PUZDEF_H
#endif
