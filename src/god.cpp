#include "god.h"
#include "index.h"
#include "antipode.h"
#include "readksolve.h"
#include "canon.h"
#include <cstdlib>
#include <iostream>
/*
 *   God's algorithm using two bits per state.
 */
vector<ull> cnts ;
void dotwobitgod(puzdef &pd) {
   ull nlongs = (pd.llstates + 31) >> 5 ;
   ull memneeded = nlongs * 8 ;
   ull *mem = (ull *)malloc(memneeded) ;
   if (mem == 0)
      error("! not enough memory") ;
   memset(mem, -1, memneeded) ;
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   ull off = densepack(pd, p1) ;
   mem[off >> 5] -= 3LL << (2 * (off & 31)) ;
   cnts.clear() ;
   cnts.push_back(1) ;
   ull tot = 1 ;
   for (int d = 0; ; d++) {
      resetantipodes() ;
      cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
           << duration() << endl << flush ;
      if (cnts[d] == 0 || tot == pd.llstates)
         break ;
      ull newseen = 0 ;
// don't be too aggressive, because we might see parity and this might slow
// things down dramatically; only go backwards after more than 50% full.
      int back = (tot * 2 > pd.llstates) ;
      int seek = d % 3 ;
      int newv = (d + 1) % 3 ;
      if (back) {
         for (ull bigi=0; bigi<nlongs; bigi++) {
            ull checkv = mem[bigi] ;
            checkv = (checkv & 0x5555555555555555LL) &
                     ((checkv >> 1) & 0x5555555555555555LL) ;
#ifdef HAVE_FFSLL
            for (int smi=ffsll(checkv); checkv; smi=ffsll(checkv)) {
#else
            for (int smi=1; checkv; smi++) {
               if (0 == ((checkv >> (smi-1)) & 1))
                  continue ;
#endif
               checkv -= 1LL << (smi-1) ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
               for (int i=0; i<(int)pd.moves.size(); i++) {
                  if (quarter && pd.moves[i].cost > 1)
                     continue ;
                  pd.mul(p1, pd.moves[i].pos, p2) ;
                  off = densepack(pd, p2) ;
                  int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
                  if (v == seek) {
                     newseen++ ;
                     stashantipodedense((bigi << 5) + (smi >> 1)) ;
                     mem[bigi] -= (3LL - newv) << (smi-1) ;
                     break ;
                  }
               }
            }
         }
      } else {
         ull xorv = (3 - seek) * 0x5555555555555555LL ;
         for (ull bigi=0; bigi<nlongs; bigi++) {
            if (mem[bigi] == 0xffffffffffffffffLL)
               continue ;
            ull checkv = mem[bigi] ^ xorv ;
            checkv = (checkv & 0x5555555555555555LL) &
                     ((checkv >> 1) & 0x5555555555555555LL) ;
#ifdef HAVE_FFSLL
            for (int smi=ffsll(checkv); checkv; smi=ffsll(checkv)) {
#else
            for (int smi=1; checkv; smi++) {
               if (0 == ((checkv >> (smi-1)) & 1))
                  continue ;
#endif
               checkv -= 1LL << (smi-1) ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
               for (int i=0; i<(int)pd.moves.size(); i++) {
                  if (quarter && pd.moves[i].cost > 1)
                     continue ;
                  pd.mul(p1, pd.moves[i].pos, p2) ;
                  off = densepack(pd, p2) ;
                  int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
                  if (v == 3) {
                     newseen++ ;
                     stashantipodedense(off) ;
                     mem[off >> 5] -= (3LL - newv) << (2 * (off & 31)) ;
                  }
               }
            }
         }
      }
      cnts.push_back(newseen) ;
      tot += newseen ;
   }
   showantipodesdense(pd, 0) ;
}
/*
 *   God's algorithm using two bits per state, but we also try to decompose
 *   the state so we can use symcoords at the lowest level, for speed.
 */
ull symcoordgoal = 20000 ;
int numsym = 0 ;
ll symcoordsize = 0 ;
int nmoves ;
vector<int> movemap ;
ull newseen ;
unsigned int *symc ;
ull *mem ;
void innerloop(int back, int seek, int newv, ull sofar, vector<ull> &muld) {
   sofar *= symcoordsize ;
   for (int i=0; i<nmoves; i++)
      muld[i] *= symcoordsize ;
   unsigned int *symtab = symc ;
   if (back) {
      for (int smoff=0; smoff<symcoordsize; smoff++, symtab += nmoves) {
         ull off = sofar + smoff ;
         int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
         if (v == 3) {
            for (int m=0; m<nmoves; m++) {
               ull off2 = muld[m] + symtab[m] ;
               int v2 = 3 & (mem[off2 >> 5] >> (2 * (off2 & 31))) ;
               if (v2 == seek) {
                  mem[off >> 5] -= (3LL - newv) << (2 * (off & 31)) ;
                  stashantipodedense(off) ;
                  newseen++ ;
                  break ;
               }
            }
         }
      }
   } else {
      for (int smoff=0; smoff<symcoordsize; smoff++, symtab += nmoves) {
         ull off = sofar + smoff ;
         if (mem[off >> 5] == 0xffffffffffffffffLL) {
            int acc = 31 - (off & 31) ;
            smoff += acc ;
            symtab += acc * nmoves ;
            continue ;
         }
         int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
         if (v == seek) {
            for (int m=0; m<nmoves; m++) {
               ull off2 = muld[m] + symtab[m] ;
               int v2 = 3 & (mem[off2 >> 5] >> (2 * (off2 & 31))) ;
               if (v2 == 3) {
                  mem[off2 >> 5] -= (3LL - newv) << (2 * (off2 & 31)) ;
                  stashantipodedense(off2) ;
// cout << "From " << off << " to " << off2 << endl ;
                  newseen++ ;
               }
            }
         }
      }
   }
}
void recur(puzdef &pd, int at, int back, int seek, int newv, ull sofar, vector<ull> &muld) {
   if (at + numsym == (int)parts.size()) {
      innerloop(back, seek, newv, sofar, muld) ;
      return ;
   }
   int sdpair = parts[at].second ;
   setdef &sd = pd.setdefs[sdpair>>1] ;
   vector<ull> muld2(nmoves) ;
   stacksetval p1(pd) ;
   stacksetval p2(pd) ;
   uchar *wmem = p1.dat ;
   uchar *wmem2 = p2.dat ;
   if (sdpair & 1) {
      ull sz = sd.llords ;
      for (ull val=0; val<sz; val++) {
         if (sd.oparity)
            indextoords2(wmem, val, sd.omod, sd.size) ;
         else
            indextoords(wmem, val, sd.omod, sd.size) ;
         for (int m=0; m<nmoves; m++) {
            sd.mulo(wmem, pd.moves[movemap[m]].pos.dat+sd.off+sd.size, wmem2) ;
            if (sd.oparity)
               muld2[m] = ordstoindex(wmem2, sd.omod, sd.size-1) + sz * muld[m] ;
            else
               muld2[m] = ordstoindex(wmem2, sd.omod, sd.size) + sz * muld[m] ;
         }
         recur(pd, at+1, back, seek, newv, val + sofar * sz, muld2) ;
      }
   } else {
      ull sz = sd.llperms ;
      for (ull val=0; val<sz; val++) {
         if (sd.pparity)
            indextoperm2(wmem, val, sd.size) ;
         else
            indextoperm(wmem, val, sd.size) ;
         for (int m=0; m<nmoves; m++) {
            sd.mulp(wmem, pd.moves[movemap[m]].pos.dat+sd.off, wmem2) ;
            if (sd.pparity)
               muld2[m] = permtoindex2(wmem2, sd.size) + sz * muld[m] ;
            else
               muld2[m] = permtoindex(wmem2, sd.size) + sz * muld[m] ;
         }
         recur(pd, at+1, back, seek, newv, val + sofar * sz, muld2) ;
      }
   }
}
void dotwobitgod2(puzdef &pd) {
   ull nlongs = (pd.llstates + 31) >> 5 ;
   ull memneeded = nlongs * 8 ;
   /*
    *   First, try to develop a strategy.
    */
   parts.clear() ;
   movemap.clear() ;
   for (int i=0; i<(int)pd.moves.size(); i++)
      if (!quarter || pd.moves[i].cost == 1)
         movemap.push_back(i) ;
   nmoves = movemap.size() ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      setdef &sd = pd.setdefs[i] ;
      if (!sd.uniq)
         error("! we don't support dense packing of non-unique yet") ;
      if (sd.llperms > 1)
         parts.push_back(make_pair(sd.llperms, i*2)) ;
      if (sd.llords > 1)
         parts.push_back(make_pair(sd.llords, i*2+1)) ;
   }
   sort(parts.begin(), parts.end()) ;
   // how many parts should we use for the sym coord?
   numsym = 0 ;
   symcoordsize = 1 ;
   ull hicount = (maxmem - memneeded) / (4 * nmoves) ;
   while (numsym < (int)parts.size()) {
      ull tsymcoordsize = symcoordsize * parts[numsym].first ;
      // never go past 32 bits, or past maxmem
      if (tsymcoordsize > 0xffffffffLL || tsymcoordsize > hicount)
         break ;
      if (tsymcoordsize / symcoordgoal > symcoordgoal / symcoordsize)
         break ;
      numsym++ ;
      symcoordsize = tsymcoordsize ;
   }
   // can't split, fall back to simpler way
   if (numsym == 0) {
      dotwobitgod(pd) ;
      return ;
   }
   cout << "Sizes [" ;
   for (int i=0; i<(int)parts.size(); i++) {
      if (i)
         cout << " " ;
      cout << parts[i].first ;
      if (i + 1 == numsym)
         cout << "]" ;
   }
   cout << endl << flush ;
   reverse(parts.begin(), parts.end()) ;
   // consider adding support for shorts here for cache friendliness.
   symc = (unsigned int *)calloc(symcoordsize * nmoves, sizeof(unsigned int)) ;
   if (symc == 0)
      error("! not enough memory") ;
   cout << "Making symcoord lookup table size " << symcoordsize <<
           " x " << nmoves << flush ;
   unsigned int *ss = symc ;
   for (ll i=0; i<symcoordsize; i++, ss += nmoves) {
      stacksetval p1(pd) ;
      stacksetval p2(pd) ;
      uchar *wmem = p1.dat ;
      uchar *wmem2 = p2.dat ;
      ull u = i ;
      ull mul = 1 ;
      for (int j=parts.size()-1; j+numsym>=(int)parts.size(); j--) {
         int sdpair = parts[j].second ;
         setdef &sd = pd.setdefs[sdpair>>1] ;
         if (sdpair & 1) {
            ull sz = sd.llords ;
            ull val = u % sz ;
            u /= sz ;
            for (int m=0; m<nmoves; m++) {
               if (sd.oparity)
                  indextoords2(wmem, val, sd.omod, sd.size) ;
               else
                  indextoords(wmem, val, sd.omod, sd.size) ;
               sd.mulo(wmem, pd.moves[movemap[m]].pos.dat+sd.off+sd.size, wmem2) ;
               if (sd.oparity)
                  ss[m] += mul * ordstoindex(wmem2, sd.omod, sd.size-1) ;
               else
                  ss[m] += mul * ordstoindex(wmem2, sd.omod, sd.size) ;
            }
            mul *= sz ;
         } else {
            ull sz = sd.llperms ;
            ull val = u % sz ;
            u /= sz ;
            for (int m=0; m<nmoves; m++) {
               if (sd.pparity)
                  indextoperm2(wmem, val, sd.size) ;
               else
                  indextoperm(wmem, val, sd.size) ;
               sd.mulp(wmem, pd.moves[movemap[m]].pos.dat+sd.off, wmem2) ;
               if (sd.pparity)
                  ss[m] += mul * permtoindex2(wmem2, sd.size) ;
               else
                  ss[m] += mul * permtoindex(wmem2, sd.size) ;
            }
            mul *= sz ;
         }
      }
   }
   cout << " in " << duration() << endl << flush ;
   mem = (ull *)malloc(memneeded) ;
   if (mem == 0)
      error("! not enough memory") ;
   memset(mem, -1, memneeded) ;
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   ull off = densepack_ordered(pd, p1) ;
   mem[off >> 5] -= 3LL << (2 * (off & 31)) ;
   cnts.clear() ;
   cnts.push_back(1) ;
   ull tot = 1 ;
   for (int d = 0; ; d++) {
      resetantipodes() ;
      cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
           << duration() << endl << flush ;
      if (cnts[d] == 0 || tot == pd.llstates)
         break ;
      newseen = 0 ;
// don't be too aggressive, because we might see parity and this might slow
// things down dramatically; only go backwards after more than 50% full.
      int back = (tot * 2 > pd.llstates) ;
      int seek = d % 3 ;
      int newv = (d + 1) % 3 ;
      vector<ull> muld(nmoves) ;
      recur(pd, 0, back, seek, newv, 0, muld) ;
      cnts.push_back(newseen) ;
      tot += newseen ;
   }
   showantipodesdense(pd, 1) ;
}
static inline int compare(const void *a_, const void *b_) {
   loosetype *a = (loosetype *)a_ ;
   loosetype *b = (loosetype *)b_ ;
   for (int i=0; i<looseper; i++)
      if (a[i] != b[i])
         return (a[i] < b[i] ? -1 : 1) ;
   return 0 ;
}
loosetype *sortuniq(loosetype *s_2, loosetype *s_1,
                    loosetype *beg, loosetype *end, int temp) {
   size_t numel = (end-beg) / looseper ;
   if (verbose > 1 || temp)
      cout << "Created " << numel << " elements in " << duration() << endl << flush ;
   qsort(beg, numel, looseper*sizeof(loosetype), compare) ;
   if (verbose > 1)
      cout << "Sorted " << flush ;
   loosetype *s_0 = beg ;
   loosetype *w = beg ;
   loosetype *r_2 = s_2 ;
   loosetype *r_1 = s_1 ;
   while (beg < end) {
      if (beg + looseper >= end || compare(beg, beg+looseper)) {
         while (r_2 < s_1 && compare(beg, r_2) > 0)
            r_2 += looseper ;
         if (r_2 >= s_1 || compare(beg, r_2)) {
            while (r_1 < s_0 && compare(beg, r_1) > 0)
               r_1 += looseper ;
            if (r_1 >= s_0 || compare(beg, r_1)) {
               memcpy(w, beg, looseper*sizeof(loosetype)) ;
               w += looseper ;
            }
         }
      }
      beg += looseper ;
   }
   if (verbose > 1 || temp)
      cout << "to " << (w - s_0) / looseper << " in " << duration() << endl << flush ;
   return w ;
}
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting.
 */
void doarraygod(const puzdef &pd) {
   ull memneeded = maxmem ;
   loosetype *mem = (loosetype *)malloc(memneeded) ;
   if (mem == 0)
      error("! not enough memory") ;
   stacksetval p1(pd), p2(pd), p3(pd) ;
   pd.assignpos(p1, pd.solved) ;
   loosepack(pd, p1, mem) ;
   cnts.clear() ;
   cnts.push_back(1) ;
   ull tot = 1 ;
   loosetype *lim = mem + memneeded / (sizeof(loosetype) * looseper) * looseper ;
   loosetype *reader = mem ;
   loosetype *writer = mem + looseper ;
   loosetype *s_1 = mem ;
   loosetype *s_2 = mem ;
   for (int d = 0; ; d++) {
      cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
           << duration() << endl << flush ;
      if (cnts[d] == 0 || tot == pd.llstates)
         break ;
      ull newseen = 0 ;
      loosetype *levend = writer ;
      for (loosetype *pr=reader; pr<levend; pr += looseper) {
         looseunpack(pd, p1, pr) ;
         for (int i=0; i<(int)pd.moves.size(); i++) {
            if (quarter && pd.moves[i].cost > 1)
               continue ;
            pd.mul(p1, pd.moves[i].pos, p2) ;
            if (!pd.legalstate(p2))
               continue ;
            loosepack(pd, p2, writer) ;
            writer += looseper ;
            if (writer >= lim)
               writer = sortuniq(s_2, s_1, levend, writer, 1) ;
         }
      }
      writer = sortuniq(s_2, s_1, levend, writer, 0) ;
      newseen = (writer - levend) / looseper ;
      cnts.push_back(newseen) ;
      tot += newseen ;
      s_2 = s_1 ;
      s_1 = levend ;
      reader = levend ;
      if (s_2 != mem) {
         ll drop = s_2 - mem ;
         memmove(mem, s_2, (writer-s_2)*sizeof(loosetype)) ;
         s_1 -= drop ;
         s_2 -= drop ;
         reader -= drop ;
         writer -= drop ;
         levend -= drop ;
      }
   }
   if (s_1 == writer) {
      showantipodes(pd, s_2, s_1) ;
   } else {
      showantipodes(pd, s_1, writer) ;
   }
}
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting, but this time using a recursive
 *   enumeration process rather than using a frontier.
 */
loosetype *s_1, *s_2, *reader, *levend, *writer, *lim ;
void dorecurgod(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      loosepack(pd, posns[sp], writer) ;
      writer += looseper ;
      if (writer >= lim)
         writer = sortuniq(s_2, s_1, levend, writer, 1) ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      if (!pd.legalstate(posns[sp+1]))
         continue ;
      dorecurgod(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void doarraygod2(const puzdef &pd) {
   ull memneeded = maxmem ;
   loosetype *mem = (loosetype *)malloc(memneeded) ;
   if (mem == 0)
      error("! not enough memory") ;
   cnts.clear() ;
   ull tot = 0 ;
   lim = mem + memneeded / (sizeof(loosetype) * looseper) * looseper ;
   reader = mem ;
   writer = mem ;
   s_1 = mem ;
   s_2 = mem ;
   for (int d=0; ; d++) {
      while ((int)posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.solved)) ;
         movehist.push_back(-1) ;
      }
      pd.assignpos(posns[0], pd.solved) ;
      ull newseen = 0 ;
      levend = writer ;
      dorecurgod(pd, d, 0, 0) ;
      writer = sortuniq(s_2, s_1, levend, writer, 0) ;
      newseen = (writer - levend) / looseper ;
      cnts.push_back(newseen) ;
      tot += newseen ;
      cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
           << duration() << endl << flush ;
      if (cnts[d] > 0)
         stashantipodesloose(levend, writer) ;
      if (cnts[d] == 0 || tot == pd.llstates)
         break ;
      if (levend != s_2)
         qsort(s_2, (levend-s_2)/looseper, looseper*sizeof(loosetype), compare) ;
      s_1 = levend ;
      reader = levend ;
   }
   showantipodesloose(pd) ;
}
