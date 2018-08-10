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
typedef unsigned int loosetype ;
const int BITSPERLOOSE = 8*sizeof(loosetype) ;
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
uchar *gmoda[256] ;
struct setdef {
   int size, off ;
   const char *name ;
   uchar omod ;
   int pbits, obits, psum ;
   bool uniq, pparity, oparity ;
   double logstates ;
   unsigned long long llperms, llords, llstates ;
   vector<int> cnts ; // only not empty when not unique.
   void mulp(const uchar *ap, const uchar *bp, uchar *cp) const {
      for (int j=0; j<size; j++)
         cp[j] = ap[bp[j]] ;
   }
   // the right side must be a move so we can access the permutation part
   void mulo(const uchar *ap, const uchar *bp, uchar *cp) const {
      if (omod > 1) {
         uchar *moda = gmoda[omod] ;
         for (int j=0; j<size; j++)
            cp[j] = moda[ap[bp[j-size]]+bp[j]] ;
      } else {
         for (int j=0; j<size; j++)
            cp[j] = 0 ;
      }
   }
} ;
struct setval {
   setval(uchar *dat_) : dat(dat_) {}
   uchar *dat ;
} ;
typedef setval setvals ;
typedef vector<setdef> setdefs_t ;
struct moove {
   moove() : name(0), pos(0), cost(1) {}
   const char *name ;
   setvals pos ;
   int cost, base ;
} ;
struct puzdef {
   puzdef() : name(0), setdefs(), solved(0), totsize(0), id(0),
              logstates(0), llstates(0) {}
   const char *name ;
   setdefs_t setdefs ;
   setvals solved ;
   vector<moove> basemoves, moves ;
   vector<int> basemoveorders ;
   int totsize ;
   setval id ;
   double logstates ;
   unsigned long long llstates ;
   int comparepos(const setvals &a, const setvals &b) const {
      return memcmp(a.dat, b.dat, totsize) ;
   }
   void assignpos(setvals &a, const setvals &b) const {
      memcpy(a.dat, b.dat, totsize) ;
   }
   void mul(const setvals &a, const setvals &b, setvals &c) const {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      uchar *cp = c.dat ;
      memset(cp, 0, totsize) ;
      for (int i=0; i<setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
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
   ~allocsetval() {
      // we drop memory here; need fix
   }
} ;
vector<ll> fact ;
ll maxmem = 7LL * 1024LL * 1024LL * 1024LL ;
int verbose ;
int quarter = 0 ;
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
int ceillog2(int v) {
   int r = 0 ;
   while (v > (1 << r))
      r++ ;
   return r ;
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
         if (typ == 's')
            pz.setdefs[i].psum = n * (n - 1) / 2 ;
      } else {
         vector<int> cnts ;
         int sum = 0 ;
         for (int j=0; j<n; j++) {
            int v = --p[j] ;
            sum += v ;
            if (v >= cnts.size())
               cnts.resize(v+1) ;
            cnts[v]++ ;
         }
         if (typ == 's')
            pz.setdefs[i].psum = sum ;
         for (int j=0; j<cnts.size(); j++)
            if (cnts[j] == 0)
               error("! values are not contiguous") ;
         if (cnts.size() != n) {
            if (typ != 's')
               error("! expected, but did not see, a proper permutation") ;
            else {
               pz.setdefs[i].uniq = 0 ;
               pz.setdefs[i].cnts = cnts ;
               pz.setdefs[i].pbits = ceillog2(cnts.size()) ;
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
         sd.pparity = (sd.size == 1 ? 0 : 1) ;
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
         m.base = pz.moves.size() ;
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
      if (quarter && m.cost > 1)
         continue ;
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
double dllstates ;
void calculatesizes(puzdef &pd) {
   ull gllstates = 1 ;
   double glogstates = 0 ;
   dllstates = 1 ;
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
            dllstates *= i ;
         }
      } else {
         int left = n ;
         for (int j=0; j<sd.cnts.size(); j++) {
            for (int k=0; k<sd.cnts[j]; k++) {
               llperms *= left ;
               logstates += log2(left) ;
               dllstates *= left ;
               left-- ;
               llperms /= (k+1) ;
               logstates -= log2(k+1) ;
               dllstates /= k+1 ;
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
            dllstates *= sd.omod ;
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
ull densepack(const puzdef &pd, setvals pos) {
   ull r = 0 ;
   ull m = 1 ;
   uchar *p = pos.dat ;
   for (int i=0; i<pd.setdefs.size(); i++) {
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
void denseunpack(const puzdef &pd, ull v, setvals pos) {
   uchar *p = pos.dat ;
   for (int i=0; i<pd.setdefs.size(); i++) {
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
vector<ull> cnts ;
/*
 *   God's algorithm using two bits per state.
 */
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
            for (int smi=ffsll(checkv); checkv; smi=ffsll(checkv)) {
               checkv -= 1LL << (smi-1) ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
               for (int i=0; i<pd.moves.size(); i++) {
                  if (quarter && pd.moves[i].cost > 1)
                     continue ;
                  pd.mul(p1, pd.moves[i].pos, p2) ;
                  off = densepack(pd, p2) ;
                  int v = 3 & (mem[off >> 5] >> (2 * (off & 31))) ;
                  if (v == seek) {
                     newseen++ ;
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
            for (int smi=ffsll(checkv); checkv; smi=ffsll(checkv)) {
               checkv -= 1LL << (smi-1) ;
               denseunpack(pd, (bigi << 5) + (smi >> 1), p1) ;
               for (int i=0; i<pd.moves.size(); i++) {
                  if (quarter && pd.moves[i].cost > 1)
                     continue ;
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
/*
 *   God's algorithm using two bits per state, but we also try to decompose
 *   the state so we can use symcoords at the lowest level, for speed.
 */
ull symcoordgoal = 20000 ;
int numsym = 0 ;
ull symcoordsize = 0 ;
vector<pair<ull, int> > parts ;
int nmoves ;
vector<int> movemap ;
ull densepack_ordered(const puzdef &pd, setvals pos) {
   ull r = 0 ;
   for (int ii=0; ii<parts.size(); ii++) {
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
ull newseen ;
uint *symc ;
ull *mem ;
void innerloop(int at, int back, int seek, int newv,
               ull sofar, vector<ull> &muld) {
   sofar *= symcoordsize ;
   for (int i=0; i<nmoves; i++)
      muld[i] *= symcoordsize ;
   uint *symtab = symc ;
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
// cout << "From " << off << " to " << off2 << endl ;
                  newseen++ ;
               }
            }
         }
      }
   }
}
void recur(puzdef &pd, int at, int back, int seek, int newv, ull sofar, vector<ull> &muld) {
   if (at + numsym == parts.size()) {
      innerloop(at, back, seek, newv, sofar, muld) ;
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
   for (int i=0; i<pd.moves.size(); i++)
      if (!quarter || pd.moves[i].cost == 1)
         movemap.push_back(i) ;
   nmoves = movemap.size() ;
   for (int i=0; i<pd.setdefs.size(); i++) {
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
   while (numsym < parts.size()) {
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
   for (int i=0; i<parts.size(); i++) {
      if (i)
         cout << " " ;
      cout << parts[i].first ;
      if (i + 1 == numsym)
         cout << "]" ;
   }
   cout << endl << flush ;
   reverse(parts.begin(), parts.end()) ;
   // consider adding support for shorts here for cache friendliness.
   symc = (uint *)calloc(symcoordsize * nmoves, sizeof(uint)) ;
   if (symc == 0)
      error("! not enough memory") ;
   cout << "Making symcoord lookup table size " << symcoordsize <<
           " x " << nmoves << flush ;
   uint *ss = symc ;
   for (ull i=0; i<symcoordsize; i++, ss += nmoves) {
      stacksetval p1(pd) ;
      stacksetval p2(pd) ;
      uchar *wmem = p1.dat ;
      uchar *wmem2 = p2.dat ;
      ull u = i ;
      ull mul = 1 ;
      for (int j=parts.size()-1; j>=parts.size()-numsym; j--) {
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
}
int looseper ;
void calclooseper(puzdef &pd) {
   int bits = 0 ;
   for (int i=0; i<pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      bits += sd.pbits * (n-1) ;
      if (sd.oparity)
         bits += sd.obits * (n-1) ;
      else
         bits += sd.obits * n ;
   }
   looseper = (bits + BITSPERLOOSE - 1) / BITSPERLOOSE ;
}
void loosepack(const puzdef &pd, setvals pos, loosetype *w) {
   uchar *p = pos.dat ;
   ull accum = 0 ;
   int storedbits = 0 ;
   for (int i=0; i<pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int n = sd.size ;
      if (n > 1) {
         int bitsper = sd.pbits ;
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
      if (sd.omod != 1) {
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
   while (storedbits > 0) {
      *w++ = accum ;
      accum >>= BITSPERLOOSE ;
      storedbits -= BITSPERLOOSE ;
   }
}
void looseunpack(const puzdef &pd, setvals pos, loosetype *r) {
   uchar *p = pos.dat ;
   ull accum = 0 ;
   int storedbits = 0 ;
   for (int i=0; i<pd.setdefs.size(); i++) {
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
      if (sd.omod != 1) {
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
   if (verbose || temp)
      cout << "Created " << numel << " elements in " << duration() << endl << flush ;
   qsort(beg, numel, looseper*sizeof(loosetype), compare) ;
   if (verbose)
      cout << "Sorted " << flush ;
   loosetype *s_0 = beg ;
   loosetype *w = beg ;
   loosetype *r_2 = s_2 ;
   loosetype *r_1 = s_1 ;
   while (beg < end) {
      if (beg + looseper >= end || compare(beg, beg+looseper)) {
         while (r_2 + looseper < s_1 && compare(beg, r_2) > 0)
            r_2 += looseper ;
         if (compare(beg, r_2)) {
            while (r_1 + looseper < s_0 && compare(beg, r_1) > 0)
               r_1 += looseper ;
            if (compare(beg, r_1)) {
               memcpy(w, beg, looseper*sizeof(loosetype)) ;
               w += looseper ;
            }
         }
      }
      beg += looseper ;
   }
   if (verbose || temp)
      cout << "to " << (w - s_0) / looseper << " in " << duration() << endl << flush ;
   return w ;
}
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting.
 */
void doarraygod(puzdef &pd) {
   ull memneeded = maxmem ;
   loosetype *mem = (loosetype *)malloc(memneeded) ;
   if (mem == 0)
      error("! not enough memory") ;
   stacksetval p1(pd), p2(pd), p3(pd) ;
   pd.assignpos(p1, pd.solved) ;
   calclooseper(pd) ;
   cout << "Requiring " << looseper*sizeof(loosetype) << " bytes per entry." << endl ;
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
         for (int i=0; i<pd.moves.size(); i++) {
            if (quarter && pd.moves[i].cost > 1)
               continue ;
            pd.mul(p1, pd.moves[i].pos, p2) ;
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
   }
}
/*
 *   Do canonicalization calculations by finding commutating moves.
 */
vector<ull> canonmask ;
vector<vector<int> > canonnext ;
void makecanonstates(const puzdef &pd) {
   int nbase = pd.basemoves.size() ;
   if (nbase > 63)
      error("! too many base moves for canonicalization calculation") ;
   vector<ull> commutes(nbase) ;
   stacksetval p1(pd), p2(pd) ;
   for (int i=0; i<nbase; i++)
      for (int j=0; j<i; j++) {
         pd.mul(pd.basemoves[i].pos, pd.basemoves[j].pos, p1) ;
         pd.mul(pd.basemoves[j].pos, pd.basemoves[i].pos, p2) ;
         if (pd.comparepos(p1, p2) == 0) {
            commutes[i] |= 1LL << j ;
            commutes[j] |= 1LL << i ;
         }
      }
   map<ull, int> statemap ;
   vector<ull> statebits ;
   statemap[0] = 0 ;
   statebits.push_back(0) ;
   int qg = 0 ;
   int statecount = 1 ;
   while (qg < statebits.size()) {
      vector<int> nextstate(nbase) ;
      for (int i=0; i<nbase; i++)
         nextstate[i] = -1 ;
      ull stateb = statebits[qg] ;
      canonmask.push_back(0) ;
      int fromst = qg++ ;
      for (int m=0; m<nbase; m++) {
         if (((stateb >> m) & 1) == 0 &&
             (stateb & commutes[m] & ((1 << m) - 1)) == 0) {
            ull nstb = (stateb & commutes[m]) | (1LL << m) ;
            if (statemap.find(nstb) == statemap.end()) {
               statemap[nstb] = statecount++ ;
               statebits.push_back(nstb) ;
            }
            int nextst = statemap[nstb] ;
            nextstate[m] = nextst ;
         } else {
            canonmask[fromst] |= 1LL << m ;
         }
      }
      canonnext.push_back(nextstate) ;
   }
   cout << "Found " << statecount << " canonical move states." << endl ;
}
void showcanon(const puzdef &pd) {
   cout.precision(16) ;
   int nstates = canonmask.size() ;
   vector<vector<double> > counts ;
   vector<double> zeros(nstates) ;
   counts.push_back(zeros) ;
   int lookahead = 1 ;
   int nbase = pd.basemoves.size() ;
   if (quarter)
      for (int i=0; i<nbase; i++)
         lookahead = max(lookahead, pd.basemoveorders[i] >> 1) ;
   counts[0][0] = 1 ;
   double gsum = 0 ;
   for (int d=0; d<=100; d++) {
      while (counts.size() <= d+lookahead)
         counts.push_back(zeros) ;
      double sum = 0 ;
      for (int i=0; i<nstates; i++)
         sum += counts[d][i] ;
      gsum += sum ;
      cout << "D " << d << " this " << sum << " total " << gsum << endl << flush ;
      if (gsum > dllstates)
         break ;
      for (int st=0; st<nstates; st++) {
         ull mask = canonmask[st] ;
         for (int m=0; m<nbase; m++) {
            if ((mask >> m) & 1)
               continue ;
            if (quarter) {
               for (int j=1; j+j<=pd.basemoveorders[m]; j++)
                  if (j+j==pd.basemoveorders[m])
                     counts[d+j][canonnext[st][m]] += 1 * counts[d][st] ;
                  else
                     counts[d+j][canonnext[st][m]] += 2 * counts[d][st] ;
            } else
               counts[d+1][canonnext[st][m]] +=
                                  (pd.basemoveorders[m]-1) * counts[d][st] ;
         }
      }
   }
}
//   Do a recursive search to find good algorithms that only affect three
//   or fewer cubicles.
vector<allocsetval> posns ;
vector<int> movehist ;
ll bigcnt = 0 ;
void recurfindalgo(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      bigcnt++ ;
      int wr = 0 ;
      const uchar *p1 = posns[sp].dat ;
      const uchar *so = pd.solved.dat ;
      for (int i=0; i<pd.setdefs.size(); i++) {
         int n = pd.setdefs[i].size ;
         for (int j=0; j<n; j++) {
            if (p1[j] != so[j] || p1[j+n] != so[j+n]) {
               wr++ ;
               if (wr > 3)
                  return ;
            }
         }
         p1 += 2 * n ;
         so += 2 * n ;
      }
      cout << wr ;
      for (int i=0; i<sp; i++)
         cout << " " << pd.moves[movehist[i]].name ;
      cout << endl << flush ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.base) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      recurfindalgo(pd, togo-1, sp+1, ns[mv.base]) ;
   }
}
void findalgos(const puzdef &pd) {
   for (int d=1; ; d++) {
      while (posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.solved)) ;
         movehist.push_back(-1) ;
      }
      bigcnt = 0 ;
      recurfindalgo(pd, d, 0, 0) ;
      cout << "At " << d << " big count is " << bigcnt << " in " << duration() << endl ;
   }
}
int dogod, docanon, doalgo ;
int main(int argc, const char **argv) {
   duration() ;
   cout << "This is twsearch 0.1 (C) 2018 Tomas Rokicki." << endl ;
   cout << "-" ;
   for (int i=0; i<argc; i++)
      cout << " " << argv[i] ;
   cout << endl << flush ;
   fact.push_back(0) ;
   for (int i=1; i<=20; i++)
      fact.push_back(fact[i-1]*i) ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 'q':
         quarter++ ;
         break ;
case 'v':
         verbose++ ;
         break ;
case 'M':
         maxmem = 1048576 * atoll(argv[1]) ;
         argc-- ;
         argv++ ;
         break ;
case 'y':
         symcoordgoal = atoll(argv[1]) ;
         if (symcoordgoal <= 0)
            symcoordgoal = 1 ;
         argc-- ;
         argv++ ;
         break ;
case 'g':
         dogod++ ;
         break ;
case 'C':
         docanon++ ;
         break ;
case 'A':
         doalgo++ ;
         break ;
default:
         error("! did not argument ", argv[0]) ;
      }
   }
   FILE *f = stdin ;
   if (argc > 1) {
      f = fopen(argv[1], "r") ;
      if (f == 0)
         error("! could not open file ", argv[1]) ;
   }
   puzdef pd = readdef(f) ;
   addmovepowers(pd) ;
   calculatesizes(pd) ;
   makecanonstates(pd) ;
   if (dogod) {
      if (pd.logstates <= 50 && (pd.llstates >> 2) <= maxmem) {
         dotwobitgod2(pd) ;
      } else {
         doarraygod(pd) ;
      }
   }
   if (docanon)
      showcanon(pd) ;
   if (doalgo)
      findalgos(pd) ;
}
