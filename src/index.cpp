#include "index.h"
#include "city.h"
#include <iostream>
vector<pair<ull, int> > parts ;
long long permtoindex(const uchar *perm, int n) {
   int i, j;
   ull r = 0 ;
   ull m = 1 ;
   uchar state[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   } ;
   uchar inverse[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   } ;
   for (i = 0; i+1 < n; i++) {
      j = inverse[perm[i]];
      inverse[state[i]] = j;
      state[j] = state[i];
      r += m * (j - i) ;
      m *= (n - i) ;
   }
   return r ;
}
void indextoperm(uchar *perm, ull ind, int n) {
   int i, j;
   uchar state[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   };
   for (i = 0; i+1 < n; i++) {
      ull t = ind / (n - i) ;
      j = i + ind - t * (n - i) ;
      ind = t ;
      perm[i] = state[j];
      state[j] = state[i];
   }
   perm[n-1] = state[n-1] ;
}
ull permtoindex2(const uchar *perm, int n) {
   int i, j;
   ull r = 0 ;
   ull m = 1 ;
   uchar state[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   } ;
   uchar inverse[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   } ;
   for (i = 0; i+2 < n; i++) {
      j = inverse[perm[i]];
      inverse[state[i]] = j;
      state[j] = state[i];
      r += m * (j - i) ;
      m *= (n - i) ;
   }
   return r ;
}
void indextoperm2(uchar *perm, ull ind, int n) {
   int i, j;
   uchar state[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
   };
   int pars = n ;
   for (i = 0; i+2 < n; i++) {
      ull t = ind / (n - i) ;
      j = i + ind - t * (n - i) ;
      if (j == i)
              pars-- ;
      ind = t ;
      perm[i] = state[j];
      state[j] = state[i];
   }
   if (pars & 1) {
      perm[n-1] = state[n-2] ;
      perm[n-2] = state[n-1] ;
   } else {
      perm[n-2] = state[n-2] ;
      perm[n-1] = state[n-1] ;
   }
}
ll ordstoindex(const uchar *p, int omod, int n) {
   ull r = 0 ;
   ull m = 1 ;
   for (int i=0; i+1<n; i++) {
      r += m * p[i] ;
      m *= omod ;
   }
   return r + m * p[n-1] ;
}
void indextoords(uchar *p, ull v, int omod, int n) {
   for (int i=0; i<n; i++) {
      ull nv = v / omod ;
      p[i] = v - nv * omod ;
      v = nv ;
   }
}
void indextoords2(uchar *p, ull v, int omod, int n) {
   int s = 0 ;
   for (int i=0; i+1<n; i++) {
      ull nv = v / omod ;
      p[i] = v - nv * omod ;
      s += p[i] ;
      v = nv ;
   }
   p[n-1] = (n * omod - s) % omod ;
}
ull densepack(const puzdef &pd, setval pos) {
   ull r = 0 ;
   ull m = 1 ;
   uchar *p = pos.dat ;
   if (pd.wildo)
      error("! can't call densepack if orientation wildcards used.") ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (n > 1) {
         if (!sd.uniq)
            error("! we don't support dense packing of non-unique yet") ;
         if (sd.pparity)
            r += m * permtoindex2(p, n) ;
         else
            r += m * permtoindex(p, n) ;
         m *= sd.llperms ;
      }
      p += n ;
      if (sd.omod != 1) {
         if (sd.oparity)
            r += m * ordstoindex(p, sd.omod, n-1) ;
         else
            r += m * ordstoindex(p, sd.omod, n) ;
         m *= sd.llords ;
      }
      p += n ;
   }
   return r ;
}
void denseunpack(const puzdef &pd, ull v, setval pos) {
   uchar *p = pos.dat ;
   if (pd.wildo)
      error("! can't call denseunpack if orientation wildcards used.") ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (n > 1) {
         ull nv = v / sd.llperms ;
         if (sd.pparity)
            indextoperm2(p, v - nv * sd.llperms, n) ;
         else
            indextoperm(p, v - nv * sd.llperms, n) ;
         v = nv ;
      } else {
         *p = 0 ;
      }
      p += n ;
      if (sd.omod != 1) {
         ull nv = v / sd.llords ;
         if (sd.oparity)
            indextoords2(p, v - nv * sd.llords, sd.omod, n) ;
         else
            indextoords(p, v - nv * sd.llords, sd.omod, n) ;
         v = nv ;
      }
      p += n ;
   }
}
ull densepack_ordered(const puzdef &pd, setval pos) {
   if (pd.wildo)
      error("! can't call densepack_ordered if orientation wildcards used.") ;
   ull r = 0 ;
   for (int ii=0; ii<(int)parts.size(); ii++) {
      int sdpair = parts[ii].second ;
      const setdef &sd = pd.setdefs[sdpair>>1] ;
      int n = sd.size ;
      if (sdpair & 1) {
         uchar *p = pos.dat + sd.off + sd.size ;
         if (sd.oparity)
            r = ordstoindex(p, sd.omod, n-1) + sd.llords * r ;
         else
            r = ordstoindex(p, sd.omod, n) + sd.llords * r ;
      } else {
         uchar *p = pos.dat + sd.off ;
         if (sd.pparity)
            r = permtoindex2(p, n) + sd.llperms * r ;
         else
            r = permtoindex(p, n) + sd.llperms * r ;
      }
   }
   return r ;
}
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos) {
   if (pd.wildo)
      error("! can't call denseunpack_ordered if orientation wildcards used.") ;
   ull r = 0 ;
   for (int ii=(int)parts.size()-1; ii>=0; ii--) {
      int sdpair = parts[ii].second ;
      const setdef &sd = pd.setdefs[sdpair>>1] ;
      int n = sd.size ;
      if (sdpair & 1) {
         uchar *p = pos.dat + sd.off + sd.size ;
         ull nv = v / sd.llords ;
         if (sd.oparity)
            indextoords2(p, v - nv * sd.llords, sd.omod, n) ;
         else
            indextoords(p, v - nv * sd.llords, sd.omod, n) ;
         v = nv ;
      } else {
         uchar *p = pos.dat + sd.off ;
         ull nv = v / sd.llperms ;
         if (sd.pparity)
            indextoperm2(p, v - nv * sd.llperms, n) ;
         else
            indextoperm(p, v - nv * sd.llperms, n) ;
         v = nv ;
      }
   }
   return r ;
}
void calclooseper(const puzdef &pd) {
   int bits = 0, ibits = 0 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      bits += sd.pbits * (n-1) ;
      ibits += sd.pibits * (n-1) ;
      if (sd.oparity) {
         bits += sd.obits * (n-1) ;
         ibits += sd.obits * (n-1) ;
      } else {
         bits += sd.obits * n ;
         ibits += sd.obits * n ;
      }
   }
   // if we are doing symmetry reductions add a single bit to mark
   // symmetric states.  After reduction mod m and uniqification, we
   // will recalculate the symmetries for any symmetric states.
   basebits = bits ;
   if (pd.rotations.size() > 0)
      bits++ ;
   looseper = (bits + BITSPERLOOSE - 1) / BITSPERLOOSE ;
   looseiper = (ibits + BITSPERLOOSE - 1) / BITSPERLOOSE ;
   if (usehashenc && looseper >= 4) {
      looseper = 4 ;
      looseiper = 4 ;
      usehashenc += 256 ;
   }
   cout << "Requiring " << bits << " bits " << looseper*sizeof(loosetype)
        << " bytes per entry; " << (looseiper*sizeof(loosetype))
        << " from identity." << endl ;
}
void loosepack(const puzdef &pd, setval pos, loosetype *w, int fromid, int sym) {
   uchar *p = pos.dat ;
   if (usehashenc >= 256) {
      uint128 *wp = (uint128*)w ;
      *wp = CityHash128((const char *)p, pd.totsize) ;
      return ;
   }
   ull accum = 0 ;
   int storedbits = 0 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (n > 1) {
         int bitsper = (fromid ? sd.pibits : sd.pbits) ;
         for (int j=0; j+1<n; j++) {
            if (bitsper + storedbits > 64) {
               *w++ = accum ;
               accum >>= BITSPERLOOSE ;
               storedbits -= BITSPERLOOSE ;
            }
            accum += ((ull)p[j]) << storedbits ;
            storedbits += bitsper ;
         }
      }
      p += n ;
      if (sd.wildo) {
         int lim = n ;
         int bitsper = sd.obits ;
         for (int j=0; j<lim; j++) {
            if (bitsper + storedbits > 64) {
               *w++ = accum ;
               accum >>= BITSPERLOOSE ;
               storedbits -= BITSPERLOOSE ;
            }
            int v = (p[j] >= sd.omod ? sd.omod : p[j]) ;
            accum += ((ull)v) << storedbits ;
            storedbits += bitsper ;
         }
      } else if (sd.omod != 1) {
         int lim = (sd.oparity ? n-1 : n) ;
         int bitsper = sd.obits ;
         for (int j=0; j<lim; j++) {
            if (bitsper + storedbits > 64) {
               *w++ = accum ;
               accum >>= BITSPERLOOSE ;
               storedbits -= BITSPERLOOSE ;
            }
            accum += ((ull)p[j]) << storedbits ;
            storedbits += bitsper ;
         }
      }
      p += n ;
   }
   if (pd.rotations.size() > 0 && sym) {
      if (1 + storedbits > 64) {
         *w++ = accum ;
         accum >>= BITSPERLOOSE ;
         storedbits -= BITSPERLOOSE ;
      }
      accum += 1LL << storedbits ;
      storedbits++ ;
   }
   while (storedbits > 0) {
      *w++ = accum ;
      accum >>= BITSPERLOOSE ;
      storedbits -= BITSPERLOOSE ;
   }
}
void looseunpack(const puzdef &pd, setval pos, loosetype *r) {
   uchar *p = pos.dat ;
   if (usehashenc >= 256) {
      error("! can't use hash encoding if you need to unpack") ;
   }
   ull accum = 0 ;
   int storedbits = 0 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (n > 1) {
         int bitsper = sd.pbits ;
         ull mask = (1 << bitsper) - 1 ;
         int msum = 0 ;
         for (int j=0; j+1<n; j++) {
            if (storedbits < bitsper) {
               accum += ((ull)(*r++)) << storedbits ;
               storedbits += BITSPERLOOSE ;
            }
            p[j] = accum & mask ;
            msum += p[j] ;
            storedbits -= bitsper ;
            accum >>= bitsper ;
         }
         p[n-1] = sd.psum - msum ;
      } else {
         *p = 0 ;
      }
      p += n ;
      if (sd.wildo) {
         int lim = n ;
         int bitsper = sd.obits ;
         ull mask = (1 << bitsper) - 1 ;
         int msum = 0 ;
         for (int j=0; j<lim; j++) {
            if (storedbits < bitsper) {
               accum += ((ull)(*r++)) << storedbits ;
               storedbits += BITSPERLOOSE ;
            }
            p[j] = accum & mask ;
            if (p[j] >= sd.omod)
               p[j] = 2*sd.omod ;
            msum += sd.omod - p[j] ;
            storedbits -= bitsper ;
            accum >>= bitsper ;
         }
      } else if (sd.omod != 1) {
         int lim = (sd.oparity ? n-1 : n) ;
         int bitsper = sd.obits ;
         ull mask = (1 << bitsper) - 1 ;
         int msum = 0 ;
         for (int j=0; j<lim; j++) {
            if (storedbits < bitsper) {
               accum += ((ull)(*r++)) << storedbits ;
               storedbits += BITSPERLOOSE ;
            }
            p[j] = accum & mask ;
            msum += sd.omod - p[j] ;
            storedbits -= bitsper ;
            accum >>= bitsper ;
         }
         if (sd.oparity)
            p[n-1] = msum % sd.omod ;
      } else {
         for (int j=0; j<n; j++)
            p[j] = 0 ;
      }
      p += n ;
   }
}
