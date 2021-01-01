#include "coset.h"
#include "parsemoves.h"
#include "solve.h"
#include "index.h"
#include "antipode.h" // just for antipode count
#include "cmdlineops.h" // for emitposition
#include <map>
#include <algorithm>
#include <iostream>
using namespace std ;
// from command line
const char *cosetmovelist, *cosetmoveseq ;
int listcosets ;
// state from runcoset into callback
setval *cosetsolved, *cosetstart, *cosetmoving, *cosetosolved ;
puzdef *cosetpd ;
ull *cosetbm, *cosetbm2 ;
vector<int> staticv ;
ll cosetsize, cosetbmsize ;
vector<int> cosetmoves, cosetrepmoves ;
// local counters
ll solcnt = 0, searchcnt = 0 ;
const int COSETBUFSIZE = 512 ;
vector<char> remap ;
struct cosetbuf {
   ull buf[COSETBUFSIZE] ;
   int cnt ;
   char pad[256] ;
   void cosetflush(int) {
      get_global_lock() ;
      for (int i=0; i<cnt; i++) {
         ull finalind = buf[i] ;
         ll off = finalind >> 6;
         ull bit = 1LL << (finalind & 63) ;
         if ((cosetbm[off] & bit) == 0) {
            solcnt++ ;
            cosetbm[off] |= bit;
         }
      }
      searchcnt += cnt ;
      cnt = 0 ;
      release_global_lock() ;
   }
} cosetbufs[MAXTHREADS] ;
ull getindex(setval pos) {
   puzdef &pd = *cosetpd ;
   unsigned char perm[256];
   ll finalind = 0 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      int pn = 0 ;
      setdef &sd = pd.setdefs[i] ;
      int off = sd.off ;
      for (int j=0; j<sd.size; j++)
         if (cosetmoving->dat[off+j]) {
            perm[pn++] = pos.dat[off+j]-staticv[i] ;
            finalind *= pn ;
         }
      if (sd.pparity)
         finalind = (finalind >> 1) + permtoindex2(perm, pn) ;
      else
         finalind += permtoindex(perm, pn) ;
   }
   return finalind ;
}
void setindex(ull ind, setval pos) {
   puzdef &pd = *cosetpd ;
   unsigned char perm[256];
   for (int i=((int)pd.setdefs.size())-1; i>=0; i--) {
      setdef &sd = pd.setdefs[i] ;
      int off = sd.off ;
      ull fact = 1 ;
      int pn = 0 ;
      for (int j=0; j<sd.size; j++)
         if (cosetmoving->dat[off+j])
            fact *= ++pn ;
         else
            pos.dat[off+j] = 0 ;
      if (sd.pparity) {
         fact >>= 1 ;
         indextoperm2(perm, ind % fact, pn) ;
      } else {
         indextoperm(perm, ind % fact, pn) ;
      }
      pn = 0 ;
      for (int j=0; j<sd.size; j++)
         if (cosetmoving->dat[off+j])
            pos.dat[off+j] = perm[pn++] + staticv[i] ;
      ind /= fact ;
   }
}
int cosetcallback(setval &pos, const vector<int> &moves, int d, int id) {
   puzdef &pd = *cosetpd ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      setdef &sd = pd.setdefs[i] ;
      int off = sd.off ;
      for (int j=0; j<sd.size; j++)
         if (!cosetmoving->dat[off+j] && cosetsolved->dat[off+j] != pos.dat[off+j])
            return 0 ;
   }
   pd.assignpos(pos, *cosetstart);
   // do this better!
   for (int i=0; i<d; i++)
      domove(pd, pos, moves[i], *(&pos+1)) ;
   struct cosetbuf &cb = cosetbufs[id] ;
   if (cb.cnt >= COSETBUFSIZE)
      cb.cosetflush(d) ;
   cb.buf[cb.cnt++] = getindex(pos) ;
   return solcnt >= cosetsize ;
}
void showcosetantipodes() {
   puzdef &pd = *cosetpd ;
   stacksetval src(pd), tmp(pd) ;
   vector<char> iremap(remap.size()) ;
   for (int i=0; i<(int)remap.size(); i++)
      iremap[remap[i]] = i ;
   for (ll off=0; off<cosetbmsize; off++)
      if (cosetbm[off] != 0xffffffffffffffffull) {
         for (ll bit=0; bit<64; bit++)
            if (0 == ((cosetbm[off] >> bit) & 1)) {
               ull ind = (off << 6) + bit ;
               if (ind >= (ull)cosetsize)
                  continue ;
               setindex(ind, src) ;
               for (int i=((int)pd.setdefs.size())-1; i>=0; i--) {
                  setdef &sd = pd.setdefs[i] ;
                  int off = sd.off ;
                  for (int j=0; j<sd.size; j++) {
                     if (cosetmoving->dat[off+j])
                        src.dat[off+j] = iremap[src.dat[off+j]] ;
                     else
                        src.dat[off+j] = cosetosolved->dat[off+j] ;
                  }
                  // invert the position!
                  for (int j=0; j<sd.size; j++)
                     tmp.dat[j] = 255 ;
                  for (int j=0; j<sd.size; j++)
                     tmp.dat[src.dat[off+j]] = j ;
                  for (int j=0; j<sd.size; j++)
                     if (tmp.dat[j] == 255)
                        error("! bad in show antipodes") ;
                  for (int j=0; j<sd.size; j++)
                     src.dat[off+j] = tmp.dat[j] ;
               }
               for (int i=0; i<(int)cosetrepmoves.size(); i++)
                  domove(pd, src, pd.moves[cosetrepmoves[i]].pos) ;
               emitposition(pd, src, 0) ;
            }
      }
}
int prepass(int d) {
   didprepass = 1 ;
   if (solcnt >= cosetsize)
      return 1 ;
   if (solcnt == 0)
      return 0 ;
   puzdef &pd = *cosetpd ;
   stacksetval src(pd), dst(pd) ;
   memcpy(cosetbm2, cosetbm, cosetbmsize * sizeof(ull)) ;
   int backwards = (solcnt * 2 > cosetsize) ;
   if (backwards) {
      for (ll off=0; off<cosetbmsize; off++)
         if (cosetbm2[off] != 0xffffffffffffffffull) {
            for (ll bit=0; bit<64; bit++)
               if (0 == ((cosetbm2[off] >> bit) & 1)) {
                  ull ind = (off << 6) + bit ;
                  if (ind >= (ull)cosetsize)
                     continue ;
                  setindex(ind, src) ;
                  for (auto mv : cosetmoves) {
                     pd.mul(src, pd.moves[mv].pos, dst) ;
                     ull ind = getindex(dst) ;
                     ull doff = ind >> 6 ;
                     ull dbit = 1LL << (ind & 63) ;
                     if ((cosetbm[doff] & dbit) != 0) {
                        cosetbm2[off] |= 1LL << bit ;
                        solcnt++ ;
                        break ;
                     }
                  }
               }
         }
   } else {
      for (ll off=0; off<cosetbmsize; off++)
         if (cosetbm[off]) {
            for (ll bit=0; bit<64; bit++)
               if ((cosetbm[off] >> bit) & 1) {
                  ull ind = (off << 6) + bit ;
                  setindex(ind, src) ;
                  for (auto mv : cosetmoves) {
                     pd.mul(src, pd.moves[mv].pos, dst) ;
                     ull ind = getindex(dst) ;
                     ull doff = ind >> 6 ;
                     ull dbit = 1LL << (ind & 63) ;
                     if ((cosetbm2[doff] & dbit) == 0) {
                        cosetbm2[doff] |= dbit ;
                        solcnt++ ;
                     }
                  }
               }
         }
   }
   swap(cosetbm, cosetbm2) ;
   cout << "Prepass for depth " << d << " see " << solcnt << " in " << duration() << endl << flush ;
   if (solcnt < cosetsize && solcnt + antipodecount >= cosetsize)
      showcosetantipodes() ;
   return solcnt >= cosetsize ;
}
int cosetflushback(int d) {
   for (int i=0; i<numthreads; i++)
      cosetbufs[i].cosetflush(d) ;
   if (solcnt)
      cout << "At " << d << " total " << solcnt << " (" << searchcnt << ")" << endl << flush ;
   return prepass(d+1) ;
}
// list the cosets, including representative sequences.  There should not be too many cosets;
// the cosets should fit in memory.
ull getcosetindex(setval pos) {
   loosetype u[2] ;
   u[1] = 0 ;
   loosepack(*cosetpd, pos, u) ;
   return u[0] + (((ull)u[1]) << (8*sizeof(loosetype))) ;
}
void setcosetindex(ull state, setval pos) {
   loosetype u[2] ;
   u[0] = state ;
   u[1] = state >> (8*sizeof(loosetype)) ;
   looseunpack(*cosetpd, pos, u) ;
}
struct stateinfo {
   int dist, move ;
   ull predstate ;
} ;
void listthecosets() {
   puzdef &pd = *cosetpd ;
   ull ss = getcosetindex(pd.solved) ;
   map<ull, stateinfo> dist ;
   dist[ss] = {0, -1, 0} ;
   vector<ull> q ;
   int qg = 0 ;
   q.push_back(ss) ;
   vector<int> seq ;
   stacksetval src(pd), dst(pd) ;
   while (qg < (int)q.size()) {
      ull s = q[qg] ;
      seq.clear() ;
      while (s != ss) {
         seq.push_back(dist[s].move) ;
         s = dist[s].predstate ;
      }
      reverse(seq.begin(), seq.end()) ;
      if (seq.size() == 0)
         cout << " " ;
      for (int i=0; i<(int)seq.size(); i++) {
         cout << " " << pd.moves[seq[i]].name ;
      }
      cout << endl ;
      s = q[qg++] ;
      int newd = dist[s].dist + 1 ;
      for (int i=0; i<(int)pd.moves.size(); i++) {
         if (quarter && pd.moves[i].cost > 1)
            continue ;
         setcosetindex(s, src) ;
         pd.mul(src, pd.moves[i].pos, dst) ;
         ull d = getcosetindex(dst) ;
         if (dist.find(d) == dist.end()) {
            dist[d] = {newd, i, s} ;
            q.push_back(d) ;
         }
      }
   }
}
void runcoset(puzdef &pd) {
   /* parse the move list. */
   auto moves = parsemovelist(pd, cosetmovelist) ;
   stacksetval moving(pd), osolved(pd), rsolved(pd) ;
   cosetmoving = &moving ;
   cosetosolved = &osolved ;
   cout << endl ;
   cosetsolved = &rsolved ;
   cosetpd = &pd ;
   pd.addoptionssum("coset") ;
   pd.addoptionssum(cosetmovelist) ;
   pd.assignpos(osolved, pd.solved) ;
   for (int i=0; i<pd.totsize; i++)
      moving.dat[i] = 0;
   ll llperms = 1 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      setdef &sd = pd.setdefs[i] ;
      if (!sd.uniq)
         error("! coset only supports unique elements") ;
      if (sd.omod != 1)
         error("! we don't yet support cosets with orientations") ;
      // check the moves to see what 
      int off = sd.off ;
      for (auto mv : moves) {
         for (int j=0; j<sd.size; j++) {
            if (pd.moves[mv].pos.dat[off+j] != j)
               moving.dat[off+j] = 1;
         }
      }
      int stat = 0;
      int mov = 0;
      for (int j=0; j<sd.size; j++)
         if (moving.dat[off+j])
            mov++ ;
         else
            stat++ ;
      cout << "At set " << i << " static " << stat << " moving " << mov << endl ;
      int stati = 0 ;
      for (int j=0; j<sd.size; j++) {
         if (moving.dat[j+off])
            continue;
         int v = pd.solved.dat[off+j];
         if (v >= (int)remap.size())
            remap.resize(v+1, -1);
         if (remap[v] < 0)
            remap[v] = stati++ ;
      }
      int movi = 0 ;
      for (int j=0; j<sd.size; j++) {
         if (!moving.dat[j+off])
            continue;
         int v = pd.solved.dat[off+j];
         if (v >= (int)remap.size())
            remap.resize(v+1, -1);
         if (remap[v] < 0)
            remap[v] = stati + movi++ ;
         if (remap[v] < stati)
            error("! same value used for moving and static") ;
      }
      for (int j=0; j<sd.size; j++)
        rsolved.dat[off+j] = remap[pd.solved.dat[off+j]] ;
      for (int j=0; j<sd.size; j++)
         if (moving.dat[off+j])
            pd.solved.dat[off+j] = stati ;
         else
            pd.solved.dat[off+j] = remap[pd.solved.dat[off+j]] ;
      if (movi != 0)
         sd.uniq = 0 ;
      sd.cnts.clear() ;
      sd.cnts.resize(stati+1) ;
      sd.psum = 0 ;
      for (int j=0; j<sd.size; j++) {
         sd.cnts[pd.solved.dat[off+j]]++ ;
         sd.psum += pd.solved.dat[off+j] ;
      }
      // calculate the needed bitmap size for the moving pieces
      if (movi) {
        vector<int> cnts(movi) ;
        int left = 0 ;
        for (int j=0; j<sd.size; j++)
           if (moving.dat[off+j]) {
              cnts[remap[osolved.dat[off+j]]-stati]++ ;
              left++ ;
           }
         for (int j=0; j<(int)cnts.size(); j++) {
            for (int k=0; k<cnts[j]; k++) {
               llperms *= left ;
               left-- ;
               llperms /= (k+1) ;
               if ((llperms >> 3) > maxmem)
                  error("! coset needs too much memory") ;
            }
         }
         if (sd.pparity)
            llperms >>= 1 ;
         if ((llperms >> 2) > maxmem)
            error("! coset needs too much memory") ;
         if (left != 0)
            error("! internal error when calculating coset size") ;
      }
      staticv.push_back(stati);
   }
   // calculate cosetmoves.  This will likely be a larger set
   // than those passed in, since it will include move multiples
   // and perhaps some additional ones.
   for (int i=0; i<(int)pd.moves.size(); i++) {    
      if (quarter && pd.moves[i].cost > 1)
         continue ;  
      const setval &mv = pd.moves[i].pos ;
      int good = 1 ;
      for (int j=0; good && j<(int)pd.setdefs.size(); j++) {
         setdef &sd = pd.setdefs[j] ;
         int off = sd.off ;
         for (int k=0; good && k<sd.size; k++) {
            if (!moving.dat[off+k] && (mv.dat[off+k] != k || mv.dat[off+k+sd.size] != 0))
               good = 0 ;
         }
      }
      if (good)
         cosetmoves.push_back(i) ;
   }
   // recalculate things for state space
   calculatesizes(pd) ;
   calclooseper(pd) ;
   cout << "Coset size is " << llperms << endl ;
   cosetsize = llperms ;
   if (listcosets) {
      listthecosets() ;
      return ;
   }
   ull bmsize = (llperms + 63) >> 6 ;
   cosetbmsize = bmsize ;
   ull *bm1 = (ull *)calloc(bmsize, sizeof(ull)) ;
   cosetbm = bm1 ;
   ull *bm2 = (ull *)calloc(bmsize, sizeof(ull)) ;
   cosetbm2 = bm2 ;
   prunetable pt(pd,  maxmem-(llperms >> 2)) ;
   setsolvecallback(cosetcallback, cosetflushback);
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   pd.assignpos(p2, rsolved) ;
   cosetrepmoves = parsemovelist(pd, cosetmoveseq) ;
   for (int i=0; i<(int)cosetrepmoves.size(); i++) {
      domove(pd, p1, cosetrepmoves[i]) ;
      domove(pd, p2, cosetrepmoves[i]) ;
   }
   cout << "Doing solve . . ." << endl ;
   cosetstart = &p2 ;
   solve(pd, pt, p1);
   for (int d=maxdepth+1; solcnt < cosetsize; d++)
      prepass(d+1) ;
}
