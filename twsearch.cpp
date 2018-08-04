#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <math.h>
#include <sys/time.h>
#undef CHECK
using namespace std ;
typedef long long ll ;
typedef unsigned long long ull ;
typedef unsigned char uchar ;
static double start ;
double walltime() {
   struct timeval tv ;
   gettimeofday(&tv, 0) ;
   return tv.tv_sec + 0.000001 * tv.tv_usec ;
}
double duration() {
   double now = walltime() ;
   double r = now - start ;
   start = now ;
   return r ;
}
struct setdef {
   int size, off ;
   const char *name ;
   uchar omod ;
   int pbits, obits ;
   bool uniq, pparity, oparity ;
   double logstates ;
   unsigned long long llperms, llords, llstates ;
   vector<int> cnts ; // only not empty when not unique.
} ;
struct setval {
   setval(uchar *dat_) : dat(dat_) {}
   uchar *dat ;
} ;
typedef setval setvals ;
typedef vector<setdef> setdefs ;
uchar *gmoda[256] ;
struct moove {
   moove() : name(0), pos(0), cost(1) {}
   const char *name ;
   setvals pos ;
   int cost ;
} ;
struct puzdef {
   puzdef() : name(0), setdefs(), solved(0), totsize(0), id(0),
              logstates(0), llstates(0) {}
   const char *name ;
   setdefs setdefs ;
   setvals solved ;
   vector<moove> basemoves, moves ;
   vector<int> basemoveorders ;
   int totsize ;
   setval id ;
   double logstates ;
   unsigned long long llstates ;
   int comparepos(setvals &a, setvals &b) {
      return memcmp(a.dat, b.dat, totsize) ;
   }
   void assignpos(setvals &a, setvals &b) {
      memcpy(a.dat, b.dat, totsize) ;
   }
   void mul(setvals &a, setvals &b, setvals &c) {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      uchar *cp = c.dat ;
      memset(cp, 0, totsize) ;
      for (int i=0; i<setdefs.size(); i++) {
         setdef &sd = setdefs[i] ;
         int n = sd.size ;
         for (int j=0; j<n; j++)
            cp[j] = ap[bp[j]] ;
         ap += n ;
         bp += n ;
         cp += n ;
         if (sd.omod > 1) {
            uchar *moda = gmoda[sd.omod] ;
            for (int j=0; j<n; j++)
               cp[j] = moda[ap[bp[j-n]]+bp[j]] ;
         } else {
            for (int j=0; j<n; j++)
               cp[j] = 0 ;
         }
         ap += n ;
         bp += n ;
         cp += n ;
      }
   }
} ;
struct stacksetval : setval {
   stacksetval(const puzdef &pd) : setval(new uchar[pd.totsize]) {
      memcpy(dat, pd.id.dat, pd.totsize) ;
   }
   stacksetval(const puzdef &pd, const setvals &iv) : setval(new uchar[pd.totsize]) {
      memcpy(dat, iv.dat, pd.totsize) ;
   }
   ~stacksetval() { delete dat ; }
} ;
struct allocsetval : setval {
   allocsetval(const puzdef &pd, const setvals &iv) : setval(new uchar[pd.totsize]) {
      memcpy(dat, iv.dat, pd.totsize) ;
   }
} ;
vector<ll> fact ;
ll maxmem = 7LL * 1024LL * 1024LL * 1024LL ;
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
   static uchar done[256] ;
   for (int i=0; i<n ;i++)
      done[i] = 0 ;
   int r = 0 ;
   for (int i=0; i<n; i++)
      if (!done[i]) {
         int cnt = 1 ;
         done[i] = 1 ;
         for (int j=p[i]; !done[j]; j=p[j]) {
            done[j] = 1 ;
            cnt++ ;
         }
         if (0 == (cnt & 1))
            r++ ;
      }
   return r & 1 ;
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
               error("! expected, but did not see, a proper permutation") ;
            else {
               pz.setdefs[i].uniq = 0 ;
               pz.setdefs[i].cnts = cnts ;
            }
         } else {
            if (oddperm(p, n))
               pz.setdefs[i].pparity = 0 ;
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
      if (typ == 'm') { // fix moves
         static uchar f[256] ;
         for (int j=0; j<n; j++)
            f[j] = p[j] ;
         for (int j=0; j<n; j++)
            p[j] = f[p[j-n]] ;
      }
   }
   return r ;
}
int ceillog2(int v) {
   int r = 0 ;
   while (v > (1 << r))
      r++ ;
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
         sd.pbits = ceillog2(sd.size) ;
         sd.obits = ceillog2(sd.omod) ;
         sd.uniq = 1 ;
         sd.off = pz.totsize ;
         pz.setdefs.push_back(sd) ;
         pz.totsize += 2 * sd.size ;
         if (gmoda[sd.omod] == 0) {
            gmoda[sd.omod] = (uchar *)calloc(4*sd.omod+1, 1) ;
            for (int i=0; i<=4*sd.omod; i++)
               gmoda[sd.omod][i] = i % sd.omod ;
         }
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
         moove m ;
         m.name = strdup(toks[1].c_str()) ;
         m.pos = readposition(pz, 'm', f) ;
         m.cost = 1 ;
         pz.moves.push_back(m) ;
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
   pz.id = setvals((uchar *)calloc(pz.totsize, 1)) ;
   uchar *p = pz.id.dat ;
   for (int i=0; i<pz.setdefs.size(); i++) {
      int n = pz.setdefs[i].size ;
      for (int j=0; j<n; j++)
         p[j] = j ;
      p += n ;
      for (int j=0; j<n; j++)
         p[j] = 0 ;
      p += n ;
   }
   return pz ;
}
void addmovepowers(puzdef &pd) {
   vector<moove> newmoves ;
   pd.basemoves = pd.moves ;
   stacksetval p1(pd), p2(pd) ;
   vector<string> newnames ;
   for (int i=0; i<pd.moves.size(); i++) {
      moove &m = pd.moves[i] ;
      vector<setvals> movepowers ;
      movepowers.push_back(m.pos) ;
      pd.assignpos(p1, m.pos) ;
      pd.assignpos(p2, m.pos) ;
      for (int p=2; p<256; p++) {
         pd.mul(p1, m.pos, p2) ;
         if (pd.comparepos(p2, pd.id) == 0)
            break ;
         movepowers.push_back(allocsetval(pd, p2)) ;
         swap(p1.dat, p2.dat) ;
      }
      int order = movepowers.size() + 1 ;
      pd.basemoveorders.push_back(order) ;
      for (int j=0; j<movepowers.size(); j++) {
         int tw = j + 1 ;
         if (order - tw < tw)
            tw -= order ;
         moove m2 = m ;
         m2.pos = movepowers[j] ;
         m2.cost = abs(tw) ;
         if (tw != 1) {
            string s2 = m.name ;
            if (tw != -1)
               s2 += to_string(abs(tw)) ;
            if (tw < 0)
               s2 += "'" ;
            newnames.push_back(s2) ;
            m2.name = strdup(s2.c_str()) ;
         }
         newmoves.push_back(m2) ;
      }
   }
   if (newnames.size() > 0) {
      pd.moves = newmoves ;
      cout << "Created new moves" ;
      for (int i=0; i<newnames.size(); i++)
         cout << " " << newnames[i] ;
      cout << endl << flush ;
   } else {
      pd.moves = pd.basemoves ;
   }
}
void calculatesizes(puzdef &pd) {
   ull gllstates = 1 ;
   double glogstates = 0 ;
   for (int i=0; i<pd.setdefs.size(); i++) {
      ull llperms = 1 ;
      ull llords = 1 ;
      double logstates = 0 ;
      setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (sd.uniq) {
         int st = 2 ;
         if (sd.pparity)
            st = 3 ;
         for (int i=st; i<=n; i++) {
            llperms *= i ;
            logstates += log2(i) ;
         }
      } else {
         int left = n ;
         for (int j=0; j<sd.cnts.size(); j++) {
            for (int k=0; k<sd.cnts[j]; k++) {
               llperms *= left ;
               logstates += log2(left) ;
               left-- ;
               llperms /= (k+1) ;
               logstates -= log2(k+1) ;
            }
         }
         if (left != 0)
            error("! internal error when calculating sizes") ;
      }
      if (sd.omod != 1) {
         int st = 0 ;
         if (sd.oparity)
            st++ ;
         for (int j=st; j<n; j++) {
            llords *= sd.omod ;
            logstates += log2(sd.omod) ;
         }
      }
      sd.llperms = llperms ;
      sd.llords = llords ;
      sd.llstates = llperms * llords ;
      sd.logstates = logstates ;
      gllstates *= sd.llstates ;
      glogstates += logstates ;
   }
   pd.llstates = gllstates ;
   pd.logstates = glogstates ;
   if (glogstates < 64) {
      cout << "State size is " << gllstates << " log2 " << glogstates << endl ;
   } else {
      double log10v = glogstates / log2(10) ;
      double expo = floor(log10v) ;
      double mant = pow(10., log10v-expo) ;
      cout << "State size is about " << mant << " x 10^" << expo <<
              " log2 " << glogstates << endl ;
   }
}
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
   for (int i=0; i<n; i++) {
      r += m * p[i] ;
      m *= omod ;
   }
   return r ;
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
ull densepack(const puzdef &pd, setvals pos) {
   ull r = 0 ;
   ull m = 1 ;
   uchar *p = pos.dat ;
   for (int i=0; i<pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (!sd.uniq)
         error("! we don't support dense packing of non-unique yet") ;
      if (sd.pparity)
         r += m * permtoindex2(p, n) ;
      else
         r += m * permtoindex(p, n) ;
      m *= sd.llperms ;
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
void denseunpack(const puzdef &pd, ull v, setvals pos) {
   uchar *p = pos.dat ;
   for (int i=0; i<pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      ull nv = v / sd.llperms ;
      if (sd.pparity)
         indextoperm2(p, v - nv * sd.llperms, n) ;
      else
         indextoperm(p, v - nv * sd.llperms, n) ;
      v = nv ;
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
   vector<ull> cnts ;
   cnts.push_back(1) ;
   ull tot = 1 ;
   for (int d = 0; ; d++) {
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
            for (int smi=ffsll(checkv)-1; checkv; smi=ffsll(checkv)-1) {
               checkv -= 1LL << smi ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
#ifdef CHECK
               ull t1 = densepack(pd, p1) ;
               if (t1 != (bigi << 5) + (smi >> 1))
                  cout << "Mispack " << " saw " << (bigi << 5) + (smi >> 1) << " but should have been " << t1 << endl ;
#endif
               for (int i=0; i<pd.moves.size(); i++) {
                  pd.mul(p1, pd.moves[i].pos, p2) ;
                  off = densepack(pd, p2) ;
                  int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
                  if (v == seek) {
                     newseen++ ;
                     mem[bigi] -= (3LL - newv) << smi ;
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
            for (int smi=ffsll(checkv)-1; checkv; smi=ffsll(checkv)-1) {
               checkv -= 1LL << smi ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
#ifdef CHECK
               ull t1 = densepack(pd, p1) ;
               if (t1 != (bigi << 5) + (smi >> 1))
                  cout << "Mispack " << " saw " << (bigi << 5) + (smi >> 1) << " but should have been " << t1 << endl ;
#endif
               for (int i=0; i<pd.moves.size(); i++) {
                  pd.mul(p1, pd.moves[i].pos, p2) ;
                  off = densepack(pd, p2) ;
                  int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
                  if (v == 3) {
                     newseen++ ;
                     mem[off >> 5] -= (3LL - newv) << (2 * (off & 31)) ;
                  }
               }
            }
         }
      }
      cnts.push_back(newseen) ;
      tot += newseen ;
   }
}
int main(int argc, const char **argv) {
   fact.push_back(0) ;
   for (int i=1; i<=20; i++)
      fact.push_back(fact[i-1]*i) ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 'v':
         verbose++ ;
         break ;
case 'M':
         maxmem = 1048576 * atoll(argv[1]) ;
         argc-- ;
         argv++ ;
         break ;
default:
         error("! did not argument ", argv[0]) ;
      }
   }
   puzdef pd = readdef(stdin) ;
   addmovepowers(pd) ;
   calculatesizes(pd) ;
   if (pd.logstates > 50)
      error("! we don't support such big puzzles yet") ;
   if ((pd.llstates >> 2) > maxmem)
      error("! not enough memory") ;
   dotwobitgod(pd) ;
}
