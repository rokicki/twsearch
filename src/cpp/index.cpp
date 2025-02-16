#include "index.h"
#include "city.h"
#include <iostream>
int looseper, looseiper, basebits, usehashenc;
vector<pair<ull, int>> parts;
ull permtoindex(const uchar *perm, int n) {
  int i, j;
  ull r = 0;
  ull m = 1;
  uchar state[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  uchar inverse[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                     12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  for (i = 0; i + 1 < n; i++) {
    j = inverse[perm[i]];
    inverse[state[i]] = j;
    state[j] = state[i];
    r += m * (j - i);
    m *= (n - i);
  }
  return r;
}
void indextoperm(uchar *perm, ull ind, int n) {
  int i, j;
  uchar state[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  for (i = 0; i + 1 < n; i++) {
    ull t = ind / (n - i);
    j = i + ind - t * (n - i);
    ind = t;
    perm[i] = state[j];
    state[j] = state[i];
  }
  perm[n - 1] = state[n - 1];
}
ull permtoindex2(const uchar *perm, int n) {
  int i, j;
  ull r = 0;
  ull m = 1;
  uchar state[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  uchar inverse[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                     12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  for (i = 0; i + 2 < n; i++) {
    j = inverse[perm[i]];
    inverse[state[i]] = j;
    state[j] = state[i];
    r += m * (j - i);
    m *= (n - i);
  }
  return r;
}
void indextoperm2(uchar *perm, ull ind, int n) {
  int i, j;
  uchar state[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  int pars = n;
  for (i = 0; i + 2 < n; i++) {
    ull t = ind / (n - i);
    j = i + ind - t * (n - i);
    if (j == i)
      pars--;
    ind = t;
    perm[i] = state[j];
    state[j] = state[i];
  }
  if (pars & 1) {
    perm[n - 1] = state[n - 2];
    perm[n - 2] = state[n - 1];
  } else {
    perm[n - 2] = state[n - 2];
    perm[n - 1] = state[n - 1];
  }
}
/*
 *   This will work for perms where the total count is less than 2^58,
 *   and the maximum value is <= 64, and the intermediate calculations
 *   don't overflow.  No input value should be > 63.  The highest value
 *   of n should be 64.
 *
 *   We do check for overflow, and bail if it happens.  It will only
 *   happen if the total state space is so large that this routine
 *   will likely not even be used (as with, for instance, 64 pieces
 *   with 32 0's and 32 1's).
 */
ull mpermtoindex(const uchar *perm, int n) {
  ull r = 0;
  int cnt[64], obit[64];
  int cntn = -1;
  for (int i = 0; i < n; i++) {
    cntn = max(cntn, (int)perm[i]);
  }
  cntn++;
  for (int i = 0; i < cntn; i++) {
    cnt[i] = 0;
    obit[i] = 0;
  }
  for (int i = 0; i < n; i++) {
    cnt[perm[i]]++;
  }
  for (int i = 1; i < cntn; i++)
    obit[i] = obit[i - 1] + cnt[i - 1];
  ull seen = ~0ULL;
  ull x = 1;
  for (int i = 0; i < n; i++) {
    int pi = perm[i];
    r = r * (n - i) + popcountll(seen & ((1ULL << obit[pi]) - 1)) * x;
    x = x * cnt[pi]--;
    if (r >= (1ULL << 58)) {
      ull g = gcd(r, x);
      r /= g;
      x /= g;
      if (r >= (1ULL << 58))
        error("! overflow in multiperm calculation", "");
    }
    seen &= ~(1ULL << (obit[pi] + cnt[pi]));
  }
  return r / x;
}
void indextomperm(uchar *perm, ull ind, const vector<int> &cnts) {
  int n = 0;
  int dcnts[64];
  for (int i = 0; i < (int)cnts.size(); i++) {
    dcnts[i] = cnts[i];
    n += cnts[i];
  }
  int dn = n;
  ull x = 1;
  for (int i = 0; i < (int)cnts.size(); i++)
    for (int j = 1; j <= cnts[i]; j++) {
      if (x < (1ULL << 58))
        x = x * dn / j;
      else
        x = x / j * dn + x % j * dn / j;
      dn--;
    }
  for (int i = 0; i < n; i++)
    for (int j = 0; j < (int)cnts.size(); j++) {
      if (dcnts[j] == 0)
        continue;
      ull x2;
      if (x < (1ULL << 58))
        x2 = x * dcnts[j] / (n - i);
      else
        x2 = x / (n - i) * dcnts[j] + x % (n - i) * dcnts[j] / (n - i);
      if (ind < x2) {
        dcnts[j]--;
        perm[i] = j;
        x = x2;
        break;
      }
      ind -= x2;
    }
}
ll ordstoindex(const uchar *p, int omod, int n) {
  ull r = 0;
  ull m = 1;
  for (int i = 0; i + 1 < n; i++) {
    r += m * p[i];
    m *= omod;
  }
  return r + m * p[n - 1];
}
void indextoords(uchar *p, ull v, int omod, int n) {
  for (int i = 0; i < n; i++) {
    ull nv = v / omod;
    p[i] = v - nv * omod;
    v = nv;
  }
}
void indextoords2(uchar *p, ull v, int omod, int n) {
  int s = 0;
  for (int i = 0; i + 1 < n; i++) {
    ull nv = v / omod;
    p[i] = v - nv * omod;
    s += p[i];
    v = nv;
  }
  p[n - 1] = (n * omod - s) % omod;
}
ull densepack(const puzdef &pd, setval pos) {
  ull r = 0;
  ull m = 1;
  uchar *p = pos.dat;
  if (pd.wildo)
    error("! can't call densepack if orientation wildcards used.");
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (n > 1) {
      if (!sd.dense)
        error("! we don't support dense packing of this puzzle");
      if (sd.uniq) {
        if (sd.pparity)
          r += m * permtoindex2(p, n);
        else
          r += m * permtoindex(p, n);
      } else {
        r += m * mpermtoindex(p, n);
      }
      m *= sd.llperms;
    }
    p += n;
    if (sd.omod != 1) {
      if (sd.oparity)
        r += m * ordstoindex(p, sd.omod, n - 1);
      else
        r += m * ordstoindex(p, sd.omod, n);
      m *= sd.llords;
    }
    p += n;
  }
  return r;
}
void denseunpack(const puzdef &pd, ull v, setval pos) {
  uchar *p = pos.dat;
  if (pd.wildo)
    error("! can't call denseunpack if orientation wildcards used.");
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (n > 1) {
      ull nv = v / sd.llperms;
      if (sd.uniq) {
        if (sd.pparity)
          indextoperm2(p, v - nv * sd.llperms, n);
        else
          indextoperm(p, v - nv * sd.llperms, n);
      } else {
        indextomperm(p, v - nv * sd.llperms, sd.cnts);
      }
      v = nv;
    } else {
      *p = 0;
    }
    p += n;
    if (sd.omod != 1) {
      ull nv = v / sd.llords;
      if (sd.oparity)
        indextoords2(p, v - nv * sd.llords, sd.omod, n);
      else
        indextoords(p, v - nv * sd.llords, sd.omod, n);
      v = nv;
    }
    p += n;
  }
}
ull densepack_ordered(const puzdef &pd, setval pos) {
  if (pd.wildo)
    error("! can't call densepack_ordered if orientation wildcards used.");
  ull r = 0;
  for (int ii = 0; ii < (int)parts.size(); ii++) {
    int sdpair = parts[ii].second;
    const setdef &sd = pd.setdefs[sdpair >> 1];
    int n = sd.size;
    if (sdpair & 1) {
      uchar *p = pos.dat + sd.off + sd.size;
      if (sd.oparity)
        r = ordstoindex(p, sd.omod, n - 1) + sd.llords * r;
      else
        r = ordstoindex(p, sd.omod, n) + sd.llords * r;
    } else {
      uchar *p = pos.dat + sd.off;
      if (sd.uniq) {
        if (sd.pparity)
          r = permtoindex2(p, n) + sd.llperms * r;
        else
          r = permtoindex(p, n) + sd.llperms * r;
      } else {
        r = mpermtoindex(p, n) + sd.llperms * r;
      }
    }
  }
  return r;
}
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos) {
  if (pd.wildo)
    error("! can't call denseunpack_ordered if orientation wildcards used.");
  ull r = 0;
  for (int ii = (int)parts.size() - 1; ii >= 0; ii--) {
    int sdpair = parts[ii].second;
    const setdef &sd = pd.setdefs[sdpair >> 1];
    int n = sd.size;
    if (sdpair & 1) {
      uchar *p = pos.dat + sd.off + sd.size;
      ull nv = v / sd.llords;
      if (sd.oparity)
        indextoords2(p, v - nv * sd.llords, sd.omod, n);
      else
        indextoords(p, v - nv * sd.llords, sd.omod, n);
      v = nv;
    } else {
      uchar *p = pos.dat + sd.off;
      ull nv = v / sd.llperms;
      if (sd.uniq) {
        if (sd.pparity)
          indextoperm2(p, v - nv * sd.llperms, n);
        else
          indextoperm(p, v - nv * sd.llperms, n);
      } else {
        indextomperm(p, v - nv * sd.llperms, sd.cnts);
      }
      v = nv;
    }
  }
  return r;
}
void calclooseper(const puzdef &pd) {
  // don't do this more than once.
  if (looseper)
    return;
  int bits = 0, ibits = 0;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    bits += sd.pbits * (n - 1);
    ibits += sd.pibits * (n - 1);
    if (sd.oparity) {
      bits += sd.obits * (n - 1);
      ibits += sd.obits * (n - 1);
    } else {
      bits += sd.obits * n;
      ibits += sd.obits * n;
    }
  }
  // if we are doing symmetry reductions add a single bit to mark
  // symmetric states.  After reduction mod m and uniqification, we
  // will recalculate the symmetries for any symmetric states.
  basebits = bits;
  if (pd.rotgroup.size() > 0)
    bits++;
  looseper = (bits + BITSPERLOOSE - 1) / BITSPERLOOSE;
  looseiper = (ibits + BITSPERLOOSE - 1) / BITSPERLOOSE;
  if (usehashenc && looseper >= 4) {
    looseper = 4;
    looseiper = 4;
    usehashenc += 256;
  }
  if (quiet == 0)
    cout << "Requiring " << bits << " bits " << looseper * sizeof(loosetype)
         << " bytes per entry; " << (looseiper * sizeof(loosetype))
         << " from identity." << endl;
}
void loosepack(const puzdef &pd, setval pos, loosetype *w, int fromid,
               int sym) {
  uchar *p = pos.dat;
  if (usehashenc >= 256) {
    uint128 *wp = (uint128 *)w;
    *wp = CityHash128((const char *)p, pd.totsize);
    return;
  }
  ull accum = 0;
  int storedbits = 0;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (n > 1) {
      int bitsper = (fromid ? sd.pibits : sd.pbits);
      for (int j = 0; j + 1 < n; j++) {
        if (bitsper + storedbits > 64) {
          *w++ = accum;
          accum >>= BITSPERLOOSE;
          storedbits -= BITSPERLOOSE;
        }
        accum += ((ull)p[j]) << storedbits;
        storedbits += bitsper;
      }
    }
    p += n;
    if (sd.wildo) {
      int lim = n;
      int bitsper = sd.obits;
      for (int j = 0; j < lim; j++) {
        if (bitsper + storedbits > 64) {
          *w++ = accum;
          accum >>= BITSPERLOOSE;
          storedbits -= BITSPERLOOSE;
        }
        int v = (p[j] >= sd.omod ? sd.omod : p[j]);
        accum += ((ull)v) << storedbits;
        storedbits += bitsper;
      }
    } else if (sd.omod != 1) {
      int lim = (sd.oparity ? n - 1 : n);
      int bitsper = sd.obits;
      for (int j = 0; j < lim; j++) {
        if (bitsper + storedbits > 64) {
          *w++ = accum;
          accum >>= BITSPERLOOSE;
          storedbits -= BITSPERLOOSE;
        }
        accum += ((ull)p[j]) << storedbits;
        storedbits += bitsper;
      }
    }
    p += n;
  }
  if (pd.rotgroup.size() > 0 && sym) {
    if (1 + storedbits > 64) {
      *w++ = accum;
      accum >>= BITSPERLOOSE;
      storedbits -= BITSPERLOOSE;
    }
    accum += ((ull)(sym - 1)) << storedbits;
    storedbits++;
  }
  while (storedbits > 0) {
    *w++ = accum;
    accum >>= BITSPERLOOSE;
    storedbits -= BITSPERLOOSE;
  }
}
void looseunpack(const puzdef &pd, setval pos, loosetype *r) {
  uchar *p = pos.dat;
  if (usehashenc >= 256) {
    error("! can't use hash encoding if you need to unpack");
  }
  ull accum = 0;
  int storedbits = 0;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (n > 1) {
      int bitsper = sd.pbits;
      ull mask = (1 << bitsper) - 1;
      int msum = 0;
      for (int j = 0; j + 1 < n; j++) {
        if (storedbits < bitsper) {
          accum += ((ull)(*r++)) << storedbits;
          storedbits += BITSPERLOOSE;
        }
        p[j] = accum & mask;
        msum += p[j];
        storedbits -= bitsper;
        accum >>= bitsper;
      }
      p[n - 1] = sd.psum - msum;
    } else {
      *p = 0;
    }
    p += n;
    if (sd.wildo) {
      int lim = n;
      int bitsper = sd.obits;
      ull mask = (1 << bitsper) - 1;
      // int msum = 0 ;
      for (int j = 0; j < lim; j++) {
        if (storedbits < bitsper) {
          accum += ((ull)(*r++)) << storedbits;
          storedbits += BITSPERLOOSE;
        }
        p[j] = accum & mask;
        if (p[j] >= sd.omod)
          p[j] = 2 * sd.omod;
        // msum += sd.omod - p[j] ;
        storedbits -= bitsper;
        accum >>= bitsper;
      }
    } else if (sd.omod != 1) {
      int lim = (sd.oparity ? n - 1 : n);
      int bitsper = sd.obits;
      ull mask = (1 << bitsper) - 1;
      int msum = 0;
      for (int j = 0; j < lim; j++) {
        if (storedbits < bitsper) {
          accum += ((ull)(*r++)) << storedbits;
          storedbits += BITSPERLOOSE;
        }
        p[j] = accum & mask;
        msum += sd.omod - p[j];
        storedbits -= bitsper;
        accum >>= bitsper;
      }
      if (sd.oparity)
        p[n - 1] = msum % sd.omod;
    } else {
      for (int j = 0; j < n; j++)
        p[j] = 0;
    }
    p += n;
  }
}
int looseperone(const puzdef &pd, int sdi, int symm) {
  int bits = 0;
  const setdef &sd = pd.setdefs[sdi];
  int n = sd.size;
  bits += sd.pbits * (n - 1);
  if (sd.oparity) {
    bits += sd.obits * (n - 1);
  } else {
    bits += sd.obits * n;
  }
  if (symm && pd.rotgroup.size() > 0)
    bits++;
  return (bits + BITSPERLOOSE - 1) / BITSPERLOOSE;
}
void loosepackone(const puzdef &pd, setval pos, int sdi, loosetype *w,
                 int fromid, int sym) {
  uchar *p = pos.dat;
  ull accum = 0;
  int storedbits = 0;
  const setdef &sd = pd.setdefs[sdi];
  int n = sd.size;
  p += sd.off ;
  if (n > 1) {
    int bitsper = (fromid ? sd.pibits : sd.pbits);
    for (int j = 0; j + 1 < n; j++) {
      if (bitsper + storedbits > 64) {
        *w++ = accum;
        accum >>= BITSPERLOOSE;
        storedbits -= BITSPERLOOSE;
      }
      accum += ((ull)p[j]) << storedbits;
      storedbits += bitsper;
    }
  }
  p += n;
  if (sd.wildo) {
    int lim = n;
    int bitsper = sd.obits;
    for (int j = 0; j < lim; j++) {
      if (bitsper + storedbits > 64) {
        *w++ = accum;
        accum >>= BITSPERLOOSE;
        storedbits -= BITSPERLOOSE;
      }
      int v = (p[j] >= sd.omod ? sd.omod : p[j]);
      accum += ((ull)v) << storedbits;
      storedbits += bitsper;
    }
  } else if (sd.omod != 1) {
    int lim = (sd.oparity ? n - 1 : n);
    int bitsper = sd.obits;
    for (int j = 0; j < lim; j++) {
      if (bitsper + storedbits > 64) {
        *w++ = accum;
        accum >>= BITSPERLOOSE;
        storedbits -= BITSPERLOOSE;
      }
      accum += ((ull)p[j]) << storedbits;
      storedbits += bitsper;
    }
  }
  if (pd.rotgroup.size() > 0 && sym) {
    if (1 + storedbits > 64) {
      *w++ = accum;
      accum >>= BITSPERLOOSE;
      storedbits -= BITSPERLOOSE;
    }
    accum += ((ull)(sym - 1)) << storedbits;
    storedbits++;
  }
  while (storedbits > 0) {
    *w++ = accum;
    accum >>= BITSPERLOOSE;
    storedbits -= BITSPERLOOSE;
  }
}
void looseunpackone(const puzdef &pd, setval pos, int sdi, loosetype *r) {
  ull accum = 0;
  int storedbits = 0;
  uchar *p = pos.dat;
  const setdef &sd = pd.setdefs[sdi];
  p += sd.off ;
  int n = sd.size;
  if (n > 1) {
    int bitsper = sd.pbits;
    ull mask = (1 << bitsper) - 1;
    int msum = 0;
    for (int j = 0; j + 1 < n; j++) {
      if (storedbits < bitsper) {
        accum += ((ull)(*r++)) << storedbits;  
        storedbits += BITSPERLOOSE;
      }
      p[j] = accum & mask;
      msum += p[j];
      accum >>= bitsper;
      storedbits -= bitsper ;
    }
    p[n - 1] = sd.psum - msum;
  } else {
    *p = 0;
  }
  p += n;
  if (sd.wildo) {
    int lim = n;
    int bitsper = sd.obits;
    ull mask = (1 << bitsper) - 1;
    // int msum = 0 ;
    for (int j = 0; j < lim; j++) {
      if (storedbits < bitsper) {
        accum += ((ull)(*r++)) << storedbits;  
        storedbits += BITSPERLOOSE;
      }
      p[j] = accum & mask;
      if (p[j] >= sd.omod)
        p[j] = 2 * sd.omod;
      // msum += sd.omod - p[j] ;
      accum >>= bitsper;
      storedbits -= bitsper ;
    }
  } else if (sd.omod != 1) {
    int lim = (sd.oparity ? n - 1 : n);
    int bitsper = sd.obits;
    ull mask = (1 << bitsper) - 1;
    int msum = 0;
    for (int j = 0; j < lim; j++) {
      if (storedbits < bitsper) {
        accum += ((ull)(*r++)) << storedbits;  
        storedbits += BITSPERLOOSE;
      }
      p[j] = accum & mask;
      msum += sd.omod - p[j];
      accum >>= bitsper;
      storedbits -= bitsper ;
    }
    if (sd.oparity)
      p[n - 1] = msum % sd.omod;
  } else {
    for (int j = 0; j < n; j++)
      p[j] = 0;
  }
}
