#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include "canon.h"
#include "ordertree.h"
#include "findalgo.h"
#include "sloppy.h"
#include "prunetable.h"
#include "readksolve.h"
static ll levcnts = 0, thislev = 0, shapecnts = 0 ;
static int globald ;
static map<ll, int> seen ;
static map<ll, int> world ;
static map<ll, int> exc ;
ll getshape(setval &sv) {
   ll r = 0 ;
   for (int i=0; i<8; i++) {
      r *= 4 ;
      if (sv.dat[i] == 1 || sv.dat[i] == 5) {
         r += 0 ;
      } else {
         r += 1 + sv.dat[8+i] ;
      }
   }
// S M M L M S L M L M M  S
// 0 1 2 3 4 5 6 7 8 9 10 11
   for (int i=0; i<12; i++) {
      r *= 4 ;
      if (sv.dat[16+i] == 0 || sv.dat[16+i] == 5 || sv.dat[16+i] == 11) {
         r += 0 ;
      } else if (sv.dat[16+i] == 3 || sv.dat[16+i] == 6 || sv.dat[16+i] == 8) {
         r += 1 ;
      } else {
         r += 2 + sv.dat[16+12+i] ;
      }
   }
   return r ;
}
ll lowshape(const puzdef &pd, setval &sv) {
   stacksetval p2(pd) ;
   ll r = getshape(sv) ;
   for (int m=1; m<(int)pd.rotgroup.size(); m++) {
      pd.mul3(pd.rotinvmap[m], sv, pd.rotgroup[m].pos, p2) ;
      ll t = getshape(p2) ;
      if (t < r) {
         r = t ;
      }
   }
   return r ;
}
static vector<vector<int>> movemap ;
void makemovemap(const puzdef &pd) {
   stacksetval p2(pd) ;
   movemap.resize(pd.rotgroup.size()) ;
   for (int m=0; m<(int)pd.rotgroup.size(); m++) {
      movemap[m].resize(pd.moves.size()) ;
      for (int i=0; i<(int)pd.moves.size(); i++) {
         pd.mul3(pd.rotinvmap[m], pd.moves[i].pos, pd.rotgroup[m].pos, p2) ;
         int found = 0 ;
         for (int j=0; j<(int)pd.moves.size(); j++)
            if (pd.comparepos(p2, pd.moves[j].pos) == 0) {
               found = 1 ;
               movemap[m][i] = j ;
            }
         if (!found)
            cout << "Could not find in movemap" << endl ;
      }
   }
}
void regist(const puzdef &pd, setval &sv, int realskip) {
   stacksetval p2(pd) ;
   for (int m=0; m<(int)pd.rotgroup.size(); m++) {
      pd.mul3(pd.rotinvmap[m], sv, pd.rotgroup[m].pos, p2) ;
      ll sh = getshape(p2) ;
      int rs = 0 ;
      for (int i=0; i<(int)pd.moves.size(); i++)
         if ((realskip >> i) & 1)
            rs |= 1<<movemap[m][i] ;
//    cout << "For shape " << sh << " setting " << rs << endl ;
      exc[sh] = rs ;
   }
}
void recurorder(const puzdef &pd, int togo, int sp, int st, int pm) {
   if (togo == 0) {
/*
      int skip = unblocked(pd, posns[sp]) ;
      if (pm >= 0 && ((skip >> pm) & 1) != 0)
         cout << "Fail; got back to bad place" << endl ;
      ll h = getshape(posns[sp]) ;
      if (world.find(h) == world.end()) {
         shapecnts++ ;
         world[h] = globald ;
         for (int i=0; i<sp; i++)
            cout << " " << pd.moves[movehist[i]].name ;
         cout << " allows" ;
         int cnt = 0 ;
         for (int m=0; m<(int)pd.moves.size(); m++) {
            if (pd.moves[m].twist == 1 && ((skip >> m) & 1) == 0) {
               cout << " " << pd.moves[m].name ;
               cnt++ ;
            }
         }
         cout << endl ;
         if (0 && globald > 1000 && exc.find(h) == exc.end()) {
            string s ;
            getline(cin, s) ;
            if (s.size() > 0) {
               int realskip = skip ;
               for (int m=0; m<(int)pd.moves.size(); m++) {
                  if (((skip >> m) & 1) == 0) {
                     int seen = 0 ;
                     for (int i=0; i<(int)s.size(); i++)
                        if (s[i] == pd.moves[m].name[0])
                           seen = 1 ;
                     if (!seen)
                        realskip |= 1 << m ;
                  }
               }
               regist(pd, posns[sp], realskip) ;
            }
         }
      }
 */
      ull h = fasthash(pd.totsize, posns[sp]) ;
      if (seen.find(h) == seen.end()) {
         seen[h] = pm * 1000 + globald ;
         thislev++ ;
         int skip = unblocked(pd, posns[sp]) ;
         if (skip == 0) {
            for (int i=0; i<sp; i++)
               cout << " " << pd.moves[movehist[i]].name ;
            cout << endl ;
         }
      }
      levcnts++ ;
      return ;
   }
   ull h = fasthash(pd.totsize, posns[sp]) ;
   if (seen[h] != (globald - togo) + pm * 1000)
      return ;
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   int skip = unblocked(pd, posns[sp]) ;
   ll sh = getshape(posns[sp]) ;
   if (exc.find(sh) != exc.end()) {
//    cout << "Noting the default skip of " << skip << " but really " ;
      skip = exc[sh] ;
//    cout << skip << endl ;
   }
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      if ((skip >> m) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      if (pd.legalstate(posns[sp+1]))
         recurorder(pd, togo-1, sp+1, ns[mv.cs], m) ;
   }
}
void ordertree(const puzdef &pd) {
   makemovemap(pd) ;
   ifstream safefile ;
   safefile.open("exceptions.safe") ;
   ull cs = 0 ;
   stacksetval p(pd) ;
   while (1) {
      if (safefile.eof())
         break ;
      auto toks = getline(&safefile, cs) ;
      if (toks.size() == 0)
         continue ;
      for (auto s: toks)
      pd.assignpos(p, pd.solved) ;
      int at = 0 ;
      while (1) {
         if (toks[at] == "allows")
            break ;
         int found = -1 ;
         for (int i=0; i<(int)pd.moves.size(); i++)
            if (toks[at] == pd.moves[i].name)
               found = i ;
         if (found < 0)
            cout << "Could not find move " << toks[at] << endl ;
         else
            domove(pd, p, found) ;
         at++ ;
      }
      at++ ;
      int rskip = (1 << pd.moves.size()) - 1 ;
      while (at < (int)toks.size()) {
         for (int i=0; i<(int)pd.moves.size(); i++)
            if (pd.moves[i].name[0] == toks[at][0])
               rskip &= ~(1 << i) ;
         at++ ;
      }
      regist(pd, p, rskip) ;
   }
   safefile.close() ;
   for (int d=0; ; d++) {
      globald = d ;
      levcnts = 0 ;
      shapecnts = 0 ;
      thislev = 0 ;
      posns.clear() ;
      movehist.clear() ;
      while ((int)posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      recurorder(pd, d, 0, 0, -1) ;
      cout << "At depth " << d << " levcnts " << levcnts << " thislev " << thislev << " world " << world.size() << endl << flush ;
   }
}
