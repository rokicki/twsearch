#include <iostream>
#include <algorithm>
#include <map>
#include <unordered_set>
#include "canon.h"
#include "util.h"
#include "index.h"
#include "city.h"
vector<ull> canonmask ;
vector<vector<int> > canonnext ;
vector<ull> canonseqcnt ;
vector<ull> canontotcnt ;
template<typename T>
struct hashvector {
   size_t operator()(const vector<T>&v) const {
      return CityHash64((const char *)v.data(), sizeof(T)*v.size()) ;
   }
} ;
template<typename T>
void freeContainer(T& c) {
   T empty;
   swap(c, empty);
}
void makecanonstates(puzdef &pd) {
   int nbase = pd.basemoves.size() ;
   if (quarter) { // rewrite base
      int at = 1 ;
      for (int i=0; i<(int)pd.moves.size(); i++) {
         moove &mv = pd.moves[i] ;
         if (mv.cost > 1)
            mv.cs = 0 ;
         else
            mv.cs = at++ ;
      }
      nbase = at ;
      cout << "For quarter turn, rewrote bases to " << nbase << endl ;
   } else {
      for (int i=0; i<(int)pd.moves.size(); i++) {
         moove &mv = pd.moves[i] ;
         mv.cs = mv.base ;
      }
   }
   if (nbase > 63)
      error("! too many base moves for canonicalization calculation") ;
   pd.ncs = nbase ;
   vector<ull> commutes(nbase) ;
   stacksetval p1(pd), p2(pd) ;
   for (int i=0; i<nbase; i++)
      commutes[i] = (1LL << nbase) - 1 ;
   /*
    *   All moves in a particular class must commute against all moves in
    *   another class, or we treat the enture class as not commuting.
    *   For instance, on the 5x5x5, 3U2 and 3R2 commute, but 3U and 3R does
    *   not, so we mark the entire 3U class as not commuting with the entire
    *   3R class.
    */
   for (int i=0; i<(int)pd.moves.size(); i++)
      for (int j=0; j<i; j++) {
         pd.mul(pd.moves[i].pos, pd.moves[j].pos, p1) ;
         pd.mul(pd.moves[j].pos, pd.moves[i].pos, p2) ;
         if (pd.comparepos(p1, p2) != 0) {
            commutes[pd.moves[i].cs] &= ~(1LL << pd.moves[j].cs) ;
            commutes[pd.moves[j].cs] &= ~(1LL << pd.moves[i].cs) ;
         }
      }
/*
   cout << "Commutes table:\n" ;
   for (int i=0; i<nbase; i++) {
      for (int j=0; j<(int)pd.moves.size(); j++)
         if (pd.moves[j].cs == i) {
            cout << pd.moves[j].name << " " ;
            break ;
         }
      for (int j=0; j<nbase; j++)
         cout << ((commutes[i] >> j) & 1) ;
      cout << endl ;
   }
 */
   pd.commutes = commutes ;
   using trip = tuple<ull, int, int> ;
   map<trip, int> statemap ;
   vector<trip > statebits ;
   trip firststate = make_tuple(0LL, -1, 0) ;
   statemap[firststate] = 0 ;
   statebits.push_back(firststate) ;
   int qg = 0 ;
   int statecount = 1 ;
   while (qg < (int)statebits.size()) {
      vector<int> nextstate(nbase) ;
      for (int i=0; i<nbase; i++)
         nextstate[i] = -1 ;
      trip statev = statebits[qg] ;
      ull stateb = get<0>(statev) ;
      int prevm = get<1>(statev) ;
      int prevcnt = get<2>(statev) ;
      canonmask.push_back(quarter ? 1 : 0) ;
      int fromst = qg++ ;
      int ms = 0 ;
      for (int m=0; m<nbase; m++) {
         // if there's a lesser move in the state that commutes with this
         // move m, we can't move m.
         if ((stateb & commutes[m] & ((1LL << m) - 1)) != 0) { // ordering
            canonmask[fromst] |= 1LL << m ;
            continue ;
         }
         if (!quarter && (((stateb >> m) & 1) != 0)) {
            canonmask[fromst] |= 1LL << m ;
            continue ;
         }
         ull nstb = (stateb & commutes[m]) | (1LL << m) ;
         // if pair of bits are set with the same commutating moves,
         // we can clear out the higher ones.
         // this optimization keeps state count from going exponential
         // for very big cubes.
         for (int i=0; nstb>>i; i++)
            if ((nstb >> i) & 1)
               for (int j=i+1; nstb>>j; j++)
                  if (((nstb >> j) & 1) && commutes[i] == commutes[j])
                     nstb &= ~(1LL << j) ;
         int thism = -1 ;
         int thiscnt = 0 ;
         if (quarter) {
            if (m == 0) {
               canonmask[fromst] |= 1LL << m ;
               continue ;
            }
            while (pd.moves[ms].cs != m)
               ms++ ;
            // don't do opposing moves in a row
            if (prevm >= 0 && ms != prevm &&
                pd.moves[ms].base == pd.moves[prevm].base) {
               canonmask[fromst] |= 1LL << m ;
               continue ;
            }
            if (ms == prevm) {
               if (2*(prevcnt+1)+(pd.moves[ms].twist != 1) >
                   pd.basemoveorders[pd.moves[ms].base]) {
                  canonmask[fromst] |= 1LL << m ;
                  continue ;
               }
            }
            thism = ms ;
            thiscnt = (ms == prevm ? prevcnt + 1 : 1) ;
         }
         trip nsi = make_tuple(nstb, thism, thiscnt) ;
         if (statemap.find(nsi) == statemap.end()) {
            statemap[nsi] = statecount++ ;
            statebits.push_back(nsi) ;
         }
         int nextst = statemap[nsi] ;
         nextstate[m] = nextst ;
      }
      canonnext.push_back(nextstate) ;
   }
   cout << "Found " << statecount << " canonical move states." << endl ;
 /*
   for (int i=0; i<(int)canonnext.size(); i++) {
      cout << i << " " << hex << canonmask[i] << dec ;
      for (int j=0; j<nbase; j++)
         cout << " " << canonnext[i][j] ;
      cout << endl ;
   }
 */
}
map<ull,int> statemap ;
int movebits, ccount, canonlim ;
vector<loosetype> ccenc ;
unordered_set<vector<loosetype>, hashvector<loosetype>> ccseen ;
vector<int> ccnextstate ;
int ccstalloc = 0 ;
int ccnbase = 0 ;
int recurcanonstates2(const puzdef &pd, int togo, ull moveset, int sp) {
   if (togo == 0) {
      loosepack(pd, posns[sp], ccenc.data(), 1) ;
      if (ccseen.find(ccenc) == ccseen.end()) {
         ccseen.insert(ccenc) ;
         if (sp > ccount) {
            ull hibit = (1LL << (ccount * movebits)) ;
            moveset = hibit | (moveset & (hibit - 1)) ;
         }
         if (statemap.find(moveset) == statemap.end()) {
// cout << "Allocating new state for " << hex << moveset << dec << endl ;
            statemap[moveset] = ccstalloc++ ;
         }
         return statemap[moveset] ;
      } else {
         return -1 ;
      }
   }
   ull newmask = 1 ;
   ull oldmask = 1 ;
   if (statemap.find(moveset) == statemap.end()) {
 cout << "Moveset is " << hex << moveset << dec << endl ;
      error("! can't find state number for moveset") ;
   }
   int cs = statemap[moveset] ;
   if (togo > 1) {
      if ((int)canonmask.size() <= cs) {
         cout << "Size of canonmask " << canonmask.size() << " cs " << cs << endl ;
         error("! canonmask not large enough") ;
      }
      oldmask = canonmask[cs] ;
   }
   for (int i=0; i<(int)pd.moves.size(); i++) {
      if ((oldmask >> pd.moves[i].cs) & 1)
         continue ;
      pd.mul(posns[sp], pd.moves[i].pos, posns[sp+1]) ;
      int nextst = recurcanonstates2(pd, togo-1,
                                     (moveset << movebits) + pd.moves[i].cs,
                                     sp+1) ;
      if (nextst < 0)
         newmask |= 1LL << pd.moves[i].cs ;
      if (togo == 1)
         ccnextstate[pd.moves[i].cs] = nextst ;
   }
   if (togo == 1) {
/*
 cout << "Adding values to mask and next; newmask " << hex << newmask << dec << endl ;
 for (int i=0; i<(int)ccnextstate.size(); i++)
   cout << " " << ccnextstate[i] ;
 cout << endl ;
 */
      canonmask.push_back(newmask) ;
      canonnext.push_back(ccnextstate) ;
   }
   return 1 ;
}
/**
 *   Another mechanism for canonical states that eliminates sequences
 *   c d if there's an earlier sequence a b that goes to the same
 *   state.  May be effective for the 2x2x2x2, or for puzzles where we
 *   don't reduce by canonical.
 */
void makecanonstates2(puzdef &pd) {
   int at = 1 ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      moove &mv = pd.moves[i] ;
      if (quarter && mv.cost > 1)
         mv.cs = 0 ;
      else
         mv.cs = at++ ;
   }
   ccnbase = at ;
   statemap[1] = 0 ;
   ccstalloc = 1 ;
   if (ccnbase > 63)
      error("! too many base moves for canonicalization calculation") ;
   movebits = ceillog2(ccnbase) ;
   pd.ncs = ccnbase ;
   ccenc = vector<loosetype>(looseiper) ;
   while (posns.size() <= 100) {
      posns.push_back(allocsetval(pd, pd.id)) ;
      movehist.push_back(-1) ;
   }
   pd.assignpos(posns[0], pd.id) ;
   ccnextstate = vector<int>(ccnbase) ;
   ccnextstate[0] = -1 ;
   for (int j=0; j<ccnbase; j++)
      ccnextstate[j] = -1 ;
   for (int d=0; d<=ccount+1; d++)
      recurcanonstates2(pd, d, 1, 0) ;
   cout << "Canonical states: " << canonmask.size() << endl ;
   freeContainer(statemap) ;
   freeContainer(ccseen) ;
}
vector<int> movestack ;
void showseqs(const puzdef &pd, int togo, int st) {
   if (togo == 0) {
      for (auto m : movestack)
         cout << " " << pd.moves[m].name ;
      cout << endl ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      const moove &mv = pd.moves[i] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      if (mv.twist != 1)
         continue ;
      movestack.push_back(i) ;
      showseqs(pd, togo-1, ns[mv.cs]) ;
      movestack.pop_back() ;
   }
}
/*
 *   Merge moves (with cancellations) in the given sequence.
 */
vector<int> cancelmoves(const puzdef &pd, vector<int> mvseq) {
   // move cancellations need to be handled separately from
   // canonicalization.
   while (1) {
      int didcancel = 0 ;
      for (int i=0; i<(int)mvseq.size(); i++) {
         int j = i+1 ;
         while (j < (int)mvseq.size() &&
                pd.moves[mvseq[i]].base != pd.moves[mvseq[j]].base &&
                (1 & (pd.commutes[pd.moves[mvseq[i]].cs] >> pd.moves[mvseq[j]].cs)))
            j++ ;
         if (j < (int)mvseq.size() && 
             pd.moves[mvseq[i]].base == pd.moves[mvseq[j]].base) {
            int twist = (pd.moves[mvseq[i]].twist + pd.moves[mvseq[j]].twist) %
                        pd.basemoveorders[pd.moves[mvseq[i]].base] ;
            mvseq.erase(mvseq.begin()+j) ;
            if (twist == 0) {
               mvseq.erase(mvseq.begin()+i) ;
            } else {
               int nind = mvseq[i] ;
               while (pd.moves[nind].twist > twist && nind > 0 &&
                      pd.moves[nind-1].base == pd.moves[mvseq[i]].base)
                  nind-- ;
               while (pd.moves[nind].twist < twist &&
                      nind+1 < (int)pd.moves.size() &&
                      pd.moves[nind+1].base == pd.moves[mvseq[i]].base)
                  nind++ ;
               if (pd.moves[nind].twist != twist ||
                   pd.moves[nind].base != pd.moves[mvseq[i]].base)
                  error("! could not find combined move") ;
               mvseq[i] = nind ;
            }
            didcancel = 1 ;
            i-- ; // check this one again
         }
      }
      if (!didcancel)
         break ;
   }
   return mvseq ;
}
vector<int> canonicalize(const puzdef &pd, vector<int> mvseq) {
   mvseq = cancelmoves(pd, mvseq) ;
   vector<int> fwdcnt(mvseq.size()) ;
   for (int i=0; i<(int)mvseq.size(); i++) {
      const moove &mv = pd.moves[mvseq[i]] ;
      for (int j=0; j<i; j++)
         if (((pd.commutes[pd.moves[mvseq[j]].cs] >> mv.cs) & 1) == 0)
            fwdcnt[j]++ ;
   }
   vector<int> r ;
   for (int i=mvseq.size()-1; i>=0; i--) {
      int best = -1 ;
      for (int j=mvseq.size()-1; j>=0; j--)
         if (fwdcnt[j] == 0 && (best < 0 || mvseq[j] < mvseq[best]))
            best = j ;
      for (int j=0; j<best; j++)
         if (((pd.commutes[pd.moves[mvseq[j]].cs] >> pd.moves[mvseq[best]].cs) & 1) == 0)
            fwdcnt[j]-- ;
      fwdcnt[best] = -1 ;
      r.push_back(mvseq[best]) ;
   }
   reverse(r.begin(), r.end()) ;
//  now test.  we can remove this if necessary.
   int cst = 0 ;
   for (int i=0; i<(int)r.size(); i++) {
      const moove &mv = pd.moves[r[i]] ;
      if ((canonmask[cst] >> mv.cs) & 1)
         error("! bad move in canonicalized.") ;
      cst = canonnext[cst][mv.cs] ;
   }
   return r ;
}
void showcanon(const puzdef &pd, int show) {
   cout.precision(16) ;
   int nstates = canonmask.size() ;
   vector<vector<double> > counts ;
   vector<double> zeros(nstates) ;
   counts.push_back(zeros) ;
   counts[0][0] = 1 ;
   double gsum = 0 ;
   double osum = 1 ;
   for (int d=0; d<=canonlim; d++) {
      while ((int)counts.size() <= d+1)
         counts.push_back(zeros) ;
      double sum = 0 ;
      for (int i=0; i<nstates; i++)
         sum += counts[d][i] ;
      canonseqcnt.push_back((ull)sum) ;
      gsum += sum ;
      canontotcnt.push_back((ull)gsum) ;
      if (show) {
         if (d == 0)
            cout << "D " << d << " this " << sum << " total " << gsum
                 << endl << flush ;
         else
            cout << "D " << d << " this " << sum << " total " << gsum
                 << " br " << (sum / osum) << endl << flush ;
      }
      osum = sum ;
      if (sum == 0 || gsum > 1e54)
         break ;
      for (int st=0; st<nstates; st++) {
         ull mask = canonmask[st] ;
         for (int m=0; m<(int)pd.moves.size(); m++) {
            if ((mask >> pd.moves[m].cs) & 1)
               continue ;
            counts[d+1][canonnext[st][pd.moves[m].cs]] += counts[d][st] ;
         }
      }
   }
/*
   for (int d=1; ; d++) {
      cout << "Seqs of length " << d << endl ;
      showseqs(pd, d, 0) ;
   }
 */
}
