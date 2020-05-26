#include <iostream>
#include "cmdlineops.h"
#include "prunetable.h"
#include "parsemoves.h"
#include "solve.h"
#include "readksolve.h"
#include "index.h"
#include "rotations.h"
ll proclim = 1'000'000'000'000'000'000LL ;
void solvecmdline(puzdef &pd, const char *scr) {
   stacksetval p1(pd) ;
   pd.assignpos(p1, pd.solved) ;
   string noname ;
   prunetable pt(pd, maxmem) ;
   vector<setval> movelist = parsemovelist_generously(pd, scr) ;
   for (int i=0; i<(int)movelist.size(); i++)
      domove(pd, p1, movelist[i]) ;
   solveit(pd, pt, noname, p1) ;
}
void processscrambles(istream *f, puzdef &pd) {
   string scramblename ;
   ull checksum = 0 ;
   stacksetval p1(pd) ;
   prunetable pt(pd, maxmem) ;
   while (1) {
      vector<string> toks = getline(f, checksum) ;
      if (toks.size() == 0)
         break ;
      if (toks[0] == "Scramble") {
         expect(toks, 2) ;
         scramblename = twstrdup(toks[1].c_str()) ; ;
         setval p = readposition(pd, 'S', f, checksum) ;
         solveit(pd, pt, scramblename, p) ;
      } else if (toks[0] == "ScrambleAlg") {
         expect(toks, 2) ;
         scramblename = twstrdup(toks[1].c_str()) ; ;
         pd.assignpos(p1, pd.solved) ;
         while (1) {
            toks = getline(f, checksum) ;
            if (toks.size() == 0)
               error("! early end of line while reading ScrambleAlg") ;
            if (toks[0] == "End")
               break ;
            for (int i=0; i<(int)toks.size(); i++)
               domove(pd, p1, findmove_generously(pd, toks[i])) ;
         }
         solveit(pd, pt, scramblename, p1) ;
      } else {
         error("! unsupported command in scramble file") ;
      }
   }
}
void readfirstscramble(istream *f, puzdef &pd, setval sv) {
   string scramblename ;
   ull checksum = 0 ;
   while (1) {
      vector<string> toks = getline(f, checksum) ;
      if (toks.size() == 0)
         break ;
      if (toks[0] == "Scramble") {
         expect(toks, 2) ;
         scramblename = twstrdup(toks[1].c_str()) ; ;
         setval p = readposition(pd, 'S', f, checksum) ;
         pd.assignpos(sv, p) ;
         return ;
      } else if (toks[0] == "ScrambleAlg") {
         expect(toks, 2) ;
         scramblename = twstrdup(toks[1].c_str()) ; ;
         pd.assignpos(sv, pd.solved) ;
         while (1) {
            toks = getline(f, checksum) ;
            if (toks.size() == 0)
               error("! early end of line while reading ScrambleAlg") ;
            if (toks[0] == "End")
               break ;
            for (int i=0; i<(int)toks.size(); i++)
               domove(pd, sv, findmove_generously(pd, toks[i])) ;
         }
         return ;
      } else {
         error("! unsupported command in scramble file") ;
      }
   }
}
vector<loosetype> uniqwork ;
set<vector<loosetype> > uniqseen ;
void uniqit(const puzdef &pd, setval p, const char *s) {
   uniqwork.resize(looseper) ;
   loosepack(pd, p, &uniqwork[0]) ;
   if (uniqseen.find(uniqwork) == uniqseen.end()) {
      uniqseen.insert(uniqwork) ;
      cout << s << endl << flush ;
      proclim-- ;
      if (proclim == 0)
         exit(0) ;
   }
}
void symsit(const puzdef &pd, setval p, const char *s) {
   stacksetval p2(pd) ;
   int symval = slowmodm(pd, p, p2) ;
   cout << s << ": " << symval << endl ;
}
void orderit(const puzdef &pd, setval p, const char *s) {
   stacksetval p2(pd), p3(pd) ;
   pd.assignpos(p2, pd.solved) ;
   pd.mul(p2, p, p3) ;
   int m = 1 ;
   while (1) {
      if (pd.comparepos(p3, pd.solved) == 0) {
         cout << m << " " << s << endl ;
         return ;
      }
      pd.mul(p3, p, p2) ;
      m++ ;
      if (pd.comparepos(p2, pd.solved) == 0) {
         cout << m << " " << s << endl ;
         return ;
      }
      pd.mul(p2, p, p3) ;
      m++ ;
   }
}
void emitmp(const puzdef &pd, setval p, const char *, int fixmoves) {
   uchar *a = p.dat ;
   if (fixmoves)
      cout << "Move noname" << endl ;
   else
      cout << "Scramble noname" << endl ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      cout << "   " << pd.setdefs[i].name << endl ;
      cout << "  " ;
      for (int i=0; i<n; i++)
         cout << " "  << (int)(a[i]+1) ;
      cout << endl ;
      if (sd.omod > 1) {
         cout << "   " ;
         if (fixmoves) {
            vector<int> newori(n) ;
            for (int i=0; i<n; i++)
               newori[a[i]] = a[i+n] ;
            for (int i=0; i<n; i++)
               cout << " "  << newori[i] ;
         } else {
            for (int i=0; i<n; i++)
               cout << " "  << (int)(a[i+n]) ;
         }
         cout << endl ;
      }
      a += 2 * n ;
   }
   cout << "End" << endl ;
}
void emitmove(const puzdef &pd, setval p, const char *s) {
   emitmp(pd, p, s, 1) ;
}
void emitposition(const puzdef &pd, setval p, const char *s) {
   emitmp(pd, p, s, 0) ;
}
void showrandompos(const puzdef &pd) {
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   for (int i=0; i<500; i++) {
      int mv = (int)(pd.moves.size()*drand48()) ;
      pd.mul(p1, pd.moves[mv].pos, p2) ;
      mv = (int)(pd.moves.size()*drand48()) ;
      pd.mul(p2, pd.moves[mv].pos, p1) ;
   }
   emitposition(pd, p1, 0) ;
}
// basic infrastructure for walking a set of sequences
int globalinputmovecount = 0 ;
void processlines(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) {
   string s ;
   stacksetval p1(pd) ;
   while (getline(cin, s)) {
      pd.assignpos(p1, pd.solved) ;
      vector<setval> movelist = parsemovelist_generously(pd, s.c_str()) ;
//    vector<int> moveid = parsemovelist(pd, s.c_str()) ;
      globalinputmovecount = movelist.size() ;
      for (int i=0; i<(int)movelist.size(); i++)
         domove(pd, p1, movelist[i]) ;
      f(pd, p1, s.c_str()) ;
   }
}
void processlines2(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) {
   string s ;
   stacksetval p1(pd) ;
   while (getline(cin, s)) {
      pd.assignpos(p1, pd.id) ;
      vector<setval> movelist = parsemovelist_generously(pd, s.c_str()) ;
//    vector<int> moveid = parsemovelist(pd, s.c_str()) ;
      globalinputmovecount = movelist.size() ;
      for (int i=0; i<(int)movelist.size(); i++)
         domove(pd, p1, movelist[i]) ;
      f(pd, p1, s.c_str()) ;
   }
}
