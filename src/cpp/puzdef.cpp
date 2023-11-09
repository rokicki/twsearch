#include "puzdef.h"
#include <iostream>
uchar *gmoda[256];
int origroup;
double dllstates;
void puzdef::pow(const setval a, setval b, ll cnt) const {
  if (cnt == 0) {
    assignpos(b, id);
    return;
  }
  if (cnt == 1) {
    assignpos(b, a);
    return;
  }
  stacksetval s(*this, a), r(*this), t(*this);
  while (cnt > 0) {
    if (cnt & 1) {
      mul(r, s, t);
      assignpos(r, t);
    }
    cnt >>= 1;
    mul(s, s, t);
    assignpos(s, t);
  }
  assignpos(b, r);
}
int puzdef::numwrong(const setval a, const setval b, ull mask) const {
  const uchar *ap = a.dat;
  const uchar *bp = b.dat;
  int r = 0;
  for (int i = 0; i < (int)setdefs.size(); i++) {
    const setdef &sd = setdefs[i];
    int n = sd.size;
    if ((mask >> i) & 1) {
      if (origroup == 0) {
        for (int j = 0; j < n; j++)
          if (ap[j] != bp[j] || ap[j + n] != bp[j + n])
            r++;
      } else {
        for (int j = 0; j < n; j += origroup)
          for (int k = 0; k < origroup; k++)
            if (ap[j + k] != bp[j + k]) {
              r++;
              break;
            }
      }
    }
    ap += 2 * n;
    bp += 2 * n;
  }
  return r;
}
int puzdef::permwrong(const setval a, const setval b, ull mask) const {
  const uchar *ap = a.dat;
  const uchar *bp = b.dat;
  int r = 0;
  for (int i = 0; i < (int)setdefs.size(); i++) {
    const setdef &sd = setdefs[i];
    int n = sd.size;
    if ((mask >> i) & 1) {
      if (origroup == 0) {
        for (int j = 0; j < n; j++)
          if (ap[j] != bp[j])
            r++;
      } else {
        for (int j = 0; j < n; j += origroup) {
          int sa = 0, sb = 0;
          for (int k = 0; k < origroup; k++) {
            sa += ap[j + k];
            sb += bp[j + k];
          }
          if (sa != sb)
            r++;
        }
      }
    }
    ap += 2 * n;
    bp += 2 * n;
  }
  return r;
}
vector<int> puzdef::cyccnts(const setval a, ull sets) const {
  const uchar *ap = a.dat;
  vector<int> r;
  for (int i = 0; i < (int)setdefs.size(); i++) {
    const setdef &sd = setdefs[i];
    int n = sd.size;
    if ((sets >> i) & 1) {
      ull done = 0;
      for (int j = 0; j < n; j++) {
        if (0 == ((done >> j) & 1)) {
          int cnt = 0;
          int ori = 0;
          for (int k = j; 0 == ((done >> k) & 1); k = ap[k]) {
            cnt++;
            ori += ap[k + n];
            done |= 1LL << k;
          }
          ori %= sd.omod;
          if (ori != 0)
            cnt *= sd.omod / gcd(ori, sd.omod);
          if ((int)r.size() <= cnt)
            r.resize(cnt + 1);
          r[cnt]++;
        }
      }
    }
    ap += 2 * n;
  }
  return r;
}
ll puzdef::order(const vector<int> cc) {
  ll r = 1;
  for (int i = 2; i < (int)cc.size(); i++)
    if (cc[i])
      r = lcm(r, i);
  return r;
}
void puzdef::addillegal(const string &setname, int pos, int val) {
  if (val > 64)
    error("! cannot use illegal on sets with more than 64 elements");
  if (val <= 0)
    error("! value in illegal must be strictly positive.");
  haveillegal = 1;
  int rpos = -1;
  for (int i = 0; i < (int)setdefs.size(); i++) {
    const setdef &sd = setdefs[i];
    if (sd.name == setname) {
      if (pos <= 0 || pos > sd.size)
        error("! position out of bounds of set");
      rpos = sd.off + pos - 1;
      break;
    }
  }
  if (rpos < 0)
    error("! did not find set in Illegal command");
  for (auto &i : illegal)
    if (i.pos == rpos) {
      i.mask |= 1ULL << (val - 1);
      return;
    }
  illegal.push_back(illegal_t({rpos, 1ULL << (val - 1)}));
}
void puzdef::inv(const setval a, setval b) const {
  const uchar *ap = a.dat;
  uchar *bp = b.dat;
  for (int i = 0; i < (int)setdefs.size(); i++) {
    const setdef &sd = setdefs[i];
    int n = sd.size;
    if (sd.omod == 1) {
      for (int j = 0; j < n; j++) {
        bp[ap[j]] = j;
        bp[j + n] = 0;
      }
    } else {
      uchar *moda = gmoda[sd.omod];
      for (int j = 0; j < n; j++) {
        bp[ap[j]] = j;
        bp[ap[j] + n] = moda[sd.omod - ap[j + n]];
      }
    }
    ap += 2 * n;
    bp += 2 * n;
  }
}
void calculatesizes(puzdef &pd) {
  ull gllstates = 1;
  double glogstates = 0;
  dllstates = 1;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    ull llperms = 1;
    double tllstates = 1;
    ull llords = 1;
    double logstates = 0;
    setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (sd.uniq) {
      int st = 2;
      if (sd.pparity)
        st = 3;
      for (int i = st; i <= n; i++) {
        llperms *= i;
        logstates += log2(i);
        dllstates *= i;
      }
    } else {
      int left = n;
      for (int j = 0; j < (int)sd.cnts.size(); j++) {
        for (int k = 0; k < sd.cnts[j]; k++) {
          llperms *= left;
          logstates += log2(left);
          tllstates *= left;
          left--;
          llperms /= (k + 1);
          logstates -= log2(k + 1);
          tllstates /= k + 1;
        }
      }
      if (left != 0)
        error("! internal error when calculating sizes");
      // llperms might overflow due to divisions above, but FP should be good
      if (logstates <= 50 && llperms < tllstates)
        llperms = tllstates;
      dllstates *= tllstates;
    }
    if (sd.omod != 1) {
      int st = 0;
      if (sd.oparity)
        st++;
      for (int j = st; j < n; j++) {
        if (sd.wildo && pd.solved.dat[sd.off + n + j] == 2 * sd.omod) {
          // do nothing; this no-value will stay as such forever
        } else {
          llords *= sd.omod;
          logstates += log2(sd.omod);
          dllstates *= sd.omod;
        }
      }
    }
    sd.llperms = llperms;
    sd.llords = llords;
    sd.llstates = llperms * llords;
    sd.logstates = logstates;
    gllstates *= sd.llstates;
    glogstates += logstates;
  }
  pd.llstates = gllstates;
  pd.logstates = glogstates;
  if (glogstates < 64) {
    if (quiet == 0)
      cout << "State size is " << gllstates << " log2 " << glogstates << endl;
  } else {
    double log10v = glogstates / log2(10);
    double expo = floor(log10v);
    double mant = pow(10., log10v - expo);
    if (quiet == 0)
      cout << "State size is about " << mant << " x 10^" << expo << " log2 "
           << glogstates << endl;
  }
}
void domove(const puzdef &pd, setval p, setval pos, setval pt) {
  pd.mul(p, pos, pt);
  pd.assignpos(p, pt);
  if (!pd.legalstate(p))
    warn("illegal position");
}
void domove(const puzdef &pd, setval p, setval pos) {
  stacksetval pt(pd);
  pd.mul(p, pos, pt);
  pd.assignpos(p, pt);
  if (!pd.legalstate(p))
    warn("illegal position");
}
void domove(const puzdef &pd, setval p, int mv) {
  domove(pd, p, pd.moves[mv].pos);
}
void domove(const puzdef &pd, setval p, int mv, setval pt) {
  domove(pd, p, pd.moves[mv].pos, pt);
}
