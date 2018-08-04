#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
using namespace std ;
typedef long long ll ;
typedef unsigned long long ull ;
typedef unsigned char uchar ;
struct setdef {
   int size, off ;
   const char *name ;
   uchar omod ;
   bool uniq, pparity, oparity ;
} ;
struct setval {
   setval(uchar *dat_) : dat(dat_) {}
   uchar *dat ;
} ;
typedef setval setvals ;
typedef vector<setdef> setdefs ;
struct puzdef {
   puzdef() : name(0), setdefs(), solved(0), totsize(0) {}
   const char *name ;
   setdefs setdefs ;
   setvals solved ;
   vector<setvals> moves ;
   int totsize ;
} ;
int verbose ;
string curline ;
void error(string msg, string extra="") {
   cerr << msg << extra << endl ;
   if (curline.size() > 0)
      cerr << "At: " << curline << endl ;
   exit(10) ;
}
vector<string> getline(FILE *f) {
   string s ;
   int c ;
   while (1) {
      s.clear() ;
      while (1) {
         c = getc(f) ;
         if (c == EOF || c == 10 || c == 13) {
            if (c == EOF || s.size() > 0)
               break ;
            else
               continue ;
         }
         s.push_back((char)c) ;
      }
      vector<string> toks ;
      if (s.size() == 0) {
         curline = s ;
         return toks ;
      }
      if (verbose > 1)
         cout << ">> " << s << endl ;
      if (s[0] == '#')
         continue ;
      string tok ;
      for (int i=0; i<s.size(); i++) {
         if (s[i] <= ' ') {
            if (tok.size() > 0) {
               toks.push_back(tok) ;
               tok.clear() ;
            }
         } else {
            tok.push_back(s[i]) ;
         }
      }
      if (tok.size() > 0)
         toks.push_back(tok) ;
      if (toks.size() == 0)
         continue ;
      curline = s ;
      return toks ;
   }
}
void expect(const vector<string> &toks, int cnt) {
   if (cnt != toks.size())
      error("! wrong number of tokens on line") ;
}
// must be a number under 256.
int getnumber(int minval, const string &s) {
   int r = 0 ;
   for (int i=0; i<s.size(); i++) {
      if ('0' <= s[i] && s[i] <= '9')
         r = r * 10 + s[i] - '0' ;
      else
         error("! bad character while parsing number in ", s) ;
   }
   if (r < minval || r > 255)
      error("! value out of range in ", s) ;
   return r ;
}
int isnumber(const string &s) {
   return s.size() > 0 && '0' <= s[0] && s[0] <= '9' ;
}
int oddperm(uchar *p, int n) {
   return 0 ;
}
setvals readposition(puzdef &pz, char typ, FILE *f) {
   setvals r((uchar *)calloc(pz.totsize, 1)) ;
   int curset = -1 ;
   int numseq = 0 ;
   while (1) {
      vector<string> toks = getline(f) ;
      if (toks.size() == 0)
         error("! premature end while reading position") ;
      if (toks[0] == "End") {
         if (curset >= 0 && numseq == 0)
            error("! empty set def?") ;
         expect(toks, 1) ;
         break ;
      }
      if (isnumber(toks[0])) {
         if (curset < 0 || numseq > 1)
            error("! unexpected number sequence") ;
         int n = pz.setdefs[curset].size ;
         expect(toks, n) ;
         uchar *p = r.dat + pz.setdefs[curset].off + numseq * n ;
         for (int i=0; i<n; i++)
            p[i] = getnumber(1-numseq, toks[i]) ;
         numseq++ ;
      } else {
         if (curset >= 0 && numseq == 0)
            error("! empty set def?") ;
         expect(toks, 1) ;
         curset = -1 ;
         for (int i=0; i<pz.setdefs.size(); i++)
            if (toks[0] == pz.setdefs[i].name) {
               curset = i ;
               break ;
            }
         if (curset < 0)
            error("Bad set name?") ;
         if (r.dat[pz.setdefs[curset].off])
            error("! redefined set name?") ;
         numseq = 0 ;
      }
   }
   for (int i=0; i<pz.setdefs.size(); i++) {
      uchar *p = r.dat + pz.setdefs[i].off ;
      int n = pz.setdefs[i].size ;
      if (p[0] == 0) {
         for (int j=0; j<n; j++)
            p[j] = j ; // identity perm
      } else {
         vector<int> cnts ;
         for (int j=0; j<n; j++) {
            int v = --p[j] ;
            if (v >= cnts.size())
               cnts.resize(v+1) ;
            cnts[v]++ ;
         }
         for (int j=0; j<cnts.size(); j++)
            if (cnts[j] == 0)
               error("! values are not contiguous") ;
         if (cnts.size() != n) {
            if (typ != 's')
               error("! expected, but did not see, a perm") ;
            else {
               pz.setdefs[i].uniq = 0 ;
               if (oddperm(p, n))
                  pz.setdefs[i].pparity = 0 ;
            }
         }
      }
      p += n ;
      int s = 0 ;
      for (int j=0; j<n; j++) {
         if (p[j] >= pz.setdefs[i].omod)
            error("! modulo value too large") ;
         s += p[j] ;
      }
      if (s % pz.setdefs[i].omod != 0)
         pz.setdefs[i].oparity = 0 ;
   }
   return r ;
}
puzdef readdef(FILE *f) {
   puzdef pz ;
   int state = 0 ;
   while (1) {
      vector<string> toks = getline(f) ;
      if (toks.size() == 0)
         break ;
      if (toks[0] == "Name") {
         if (state != 0)
            error("! Name in wrong place") ;
         state++ ;
         expect(toks, 2) ;
         pz.name = strdup(toks[1].c_str()) ; ;
      } else if (toks[0] == "Set") {
         if (state == 0) {
            pz.name = "Unnamed" ;
            state++ ;
         }
         if (state != 1)
            error("! Set in wrong place") ;
         expect(toks, 4) ;
         setdef sd ;
         sd.name = strdup(toks[1].c_str()) ;
         sd.size = getnumber(1, toks[2]) ;
         sd.omod = getnumber(1, toks[3]) ;
         sd.pparity = 1 ;
         sd.oparity = 1 ;
         sd.uniq = 1 ;
         sd.off = pz.totsize ;
         pz.setdefs.push_back(sd) ;
         pz.totsize += 2 * sd.size ;
      } else if (toks[0] == "Solved") {
         if (state != 1)
            error("! Solved in wrong place") ;
         state++ ;
         expect(toks, 1) ;
         pz.solved = readposition(pz, 's', f) ;
      } else if (toks[0] == "Move") {
         if (state != 2)
            error("! Move in wrong place") ;
         expect(toks, 2) ;
         pz.moves.push_back(readposition(pz, 'm', f)) ;
      } else {
         error("! unexpected first token on line ", toks[0]) ;
      }
   }
   if (pz.name == 0)
      error("! puzzle must be given a name") ;
   if (pz.setdefs.size() == 0)
      error("! puzzle must have set definitions") ;
   if (pz.solved.dat == 0)
      error("! puzzle must have a solved position") ;
   if (pz.moves.size() == 0)
      error("! puzzle must have moves") ;
   return pz ;
}
int main(int argc, const char **argv) {
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 'v':
         verbose++ ;
         break ;
default:
         error("! did not argument ", argv[0]) ;
      }
   }
   readdef(stdin) ;
}
