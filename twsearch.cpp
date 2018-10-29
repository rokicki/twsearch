#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <unordered_set>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <algorithm>
#include <string>
#include <math.h>
#include <sys/time.h>
#include <cstdio>
#include <pthread.h>
#include <random>
#include <functional>
#include "city.h"
#undef CHECK
using namespace std ;
typedef long long ll ;
typedef unsigned long long ull ;
typedef unsigned char uchar ;
typedef unsigned int loosetype ;
string inputbasename ;
ull solutionsneeded = 1 ;
const int CACHELINESIZE = 64 ;
const int BITSPERLOOSE = 8*sizeof(loosetype) ;
const int SIGNATURE = 22 ; // start and end of data files
static double start ;
int ignoreori, origroup, checkbeforesolve, noearlysolutions, phase2 ;
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
string curline ;
void error(string msg, string extra="") {
   cerr << msg << extra << endl ;
   if (curline.size() > 0)
      cerr << "At: " << curline << endl ;
   exit(10) ;
}
/*
 *   strdup is going through some issues: POSIX vs C++, so we just
 *   implement it ourselves.
 */
const char *twstrdup(const char *s) {
   char *r = (char *)malloc(strlen(s)+1) ;
   strcpy(r, s) ;
   return r ;
}
double myrand(int n) {
   static mt19937 rng ;
   // the following double is exact
   static double mul = 1.0 / (rng.max() - rng.min() + 1.0) ;
   return (int)((rng()-rng.min()) * mul * n) ;
}
const int MAXTHREADS = 64 ;
int numthreads = 4 ;
pthread_mutex_t mmutex ;
/*
 *   This sets a limit on the scalability of filling, but at the same
 *   time introduces a need for more memory since we need
 *   MAXTHREADS * MEMSHARDS * FILLCHUNKS * sizeof(ull) for shard
 *   buffers.
 */
const int MEMSHARDS = 64 ;
struct memshard {
   pthread_mutex_t mutex ;
   char pad[256] ;
} memshards[MEMSHARDS] ;
void init_mutex() {
  pthread_mutex_init(&mmutex, NULL) ;
}
void get_global_lock() {
   pthread_mutex_lock(&mmutex) ;
}
void release_global_lock() {
   pthread_mutex_unlock(&mmutex) ;
}
pthread_t p_thread[MAXTHREADS] ;
#define THREAD_RETURN_TYPE void *
#define THREAD_DECLARATOR
void spawn_thread(int i, THREAD_RETURN_TYPE(THREAD_DECLARATOR *p)(void *),
                                                                     void *o) {
   pthread_create(&(p_thread[i]), NULL, p, o) ;
}
void join_thread(int i) {
   pthread_join(p_thread[i], 0) ;
}
ll gcd(ll a, ll b) {
   if (a > b)
      swap(a, b) ;
   if (a == 0)
      return b ;
   return gcd(b % a, a) ;
}
ll lcm(ll a, ll b) {
   return a / gcd(a,b) * b ;
}
uchar *gmoda[256] ;
struct setdef {
   int size, off ;
   const char *name ;
   uchar omod ;
   int pbits, obits, pibits, psum ;
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
   setval() : dat(0) {}
   setval(uchar *dat_) : dat(dat_) {}
   uchar *dat ;
} ;
typedef vector<setdef> setdefs_t ;
struct moove {
   moove() : name(0), pos(0), cost(1) {}
   const char *name ;
   setval pos ;
   int cost, base, twist, cs ;
} ;
struct illegal_t {
   int pos ;
   ull mask ;
} ;
struct puzdef {
   puzdef() : name(0), setdefs(), solved(0), totsize(0), id(0),
              logstates(0), llstates(0), checksum(0), haveillegal(0) {}
   const char *name ;
   setdefs_t setdefs ;
   setval solved ;
   vector<moove> basemoves, moves, parsemoves ;
   vector<int> basemoveorders ;
   int totsize ;
   int ncs ;
   setval id ;
   double logstates ;
   unsigned long long llstates ;
   ull checksum ;
   ull optionssum ;
   vector<illegal_t> illegal ;
   char haveillegal ;
   int comparepos(const setval a, const setval b) const {
      return memcmp(a.dat, b.dat, totsize) ;
   }
   int canpackdense() const {
      for (int i=0; i<(int)setdefs.size(); i++)
         if (!setdefs[i].uniq)
            return 0 ;
      return 1 ;
   }
   void assignpos(setval a, const setval b) const {
      memcpy(a.dat, b.dat, totsize) ;
   }
   void addoptionssum(const char *p) {
      while (*p)
         optionssum = 37 * optionssum + *p++ ;
   }
   int numwrong(const setval a, const setval b, ull mask=-1) const {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      int r = 0 ;
      for (int i=0; i<(int)setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
         int n = sd.size ;
         if ((mask >> i) & 1) {
            if (origroup == 0) {
               for (int j=0; j<n; j++)
                  if (ap[j] != bp[j] || ap[j+n] != bp[j+n])
                     r++ ;
            } else {
               for (int j=0; j<n; j += origroup)
                  for (int k=0; k<origroup; k++)
                     if (ap[j+k] != bp[j+k]) {
                        r++ ;
                        break ;
                     }
            }
         }
         ap += 2*n ;
         bp += 2*n ;
      }
      return r ;
   }
   int permwrong(const setval a, const setval b, ull mask=-1) const {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      int r = 0 ;
      for (int i=0; i<(int)setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
         int n = sd.size ;
         if ((mask >> i) & 1) {
            if (origroup == 0) {
               for (int j=0; j<n; j++)
                  if (ap[j] != bp[j])
                     r++ ;
            } else {
               for (int j=0; j<n; j += origroup) {
                  int sa = 0, sb = 0 ;
                  for (int k=0; k<origroup; k++) {
                     sa += ap[j+k] ;
                     sb += bp[j+k] ;
                  }
                  if (sa != sb)
                     r++ ;
               }
            }
         }
         ap += 2*n ;
         bp += 2*n ;
      }
      return r ;
   }
   vector<int> cyccnts(const setval a, ull sets=-1) const {
      const uchar *ap = a.dat ;
      vector<int> r ;
      for (int i=0; i<(int)setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
         int n = sd.size ;
         if ((sets >> i) & 1) {
            ull done = 0 ;
            for (int j=0; j<n; j++) {
               if (0 == ((done >> j) & 1)) {
                  int cnt = 0 ;
                  int ori = 0 ;
                  for (int k=j; 0==((done >> k) & 1); k = ap[k]) {
                     cnt++ ;
                     ori += ap[k+n] ;
                     done |= 1LL << k ;
                  }
                  ori %= sd.omod ;
                  if (ori != 0)
                     cnt *= sd.omod / gcd(ori, sd.omod) ;
                  if ((int)r.size() <= cnt)
                     r.resize(cnt+1) ;
                  r[cnt]++ ;
               }
            }
         }
         ap += 2*n ;
      }
      return r ;
   }
   static ll order(const vector<int> cc) {
      ll r = 1 ;
      for (int i=2; i<(int)cc.size(); i++)
         if (cc[i])
            r = lcm(r, i) ;
      return r ;
   }
   void mul(const setval a, const setval b, setval c) const {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      uchar *cp = c.dat ;
      memset(cp, 0, totsize) ;
      for (int i=0; i<(int)setdefs.size(); i++) {
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
   int legalstate(const setval a) const {
      if (!haveillegal)
         return 1 ;
      for (auto i : illegal) {
         if ((i.mask >> a.dat[i.pos]) & 1)
            return 0 ;
      }
      return 1 ;
   }
   void addillegal(const char *setname, int pos, int val) {
      if (val > 64)
         error("! cannot use illegal on sets with more than 64 elements") ;
      if (val <= 0)
         error("! value in illegal must be strictly positive.") ;
      haveillegal = 1 ;
      int rpos = -1 ;
      for (int i=0; i<(int)setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
         if (strcmp(sd.name, setname) == 0) {
            if (pos <= 0 || pos > sd.size)
               error("! position out of bounds of set") ;
            rpos = sd.off + pos - 1 ;
            break ;
         }
      }
      if (rpos < 0)
         error("! did not find set in Illegal command") ;
      for (auto &i : illegal)
         if (i.pos == rpos) {
            i.mask |= 1LL << val ;
            return ;
         }
      illegal.push_back(illegal_t({rpos, 1ULL << (val-1)})) ;
   }
   void pow(const setval a, setval b, ll cnt) const ;
   void inv(const setval a, setval b) const ;
} ;
struct stacksetval : setval {
   stacksetval(const puzdef &pd) : setval(new uchar[pd.totsize]) {
      memcpy(dat, pd.id.dat, pd.totsize) ;
   }
   stacksetval(const puzdef &pd, const setval iv) : setval(new uchar[pd.totsize]) {
      memcpy(dat, iv.dat, pd.totsize) ;
   }
   ~stacksetval() { delete [] dat ; }
} ;
struct allocsetval : setval {
   allocsetval(const puzdef &pd, const setval iv) : setval(new uchar[pd.totsize]) {
      memcpy(dat, iv.dat, pd.totsize) ;
   }
   ~allocsetval() {
      // we drop memory here; need fix
   }
} ;
void puzdef::pow(const setval a, setval b, ll cnt) const {
   if (cnt == 0) {
      assignpos(b, id) ;
      return ;
   }
   if (cnt == 1) {
      assignpos(b, a) ;
      return ;
   }
   stacksetval s(*this, a), r(*this), t(*this) ;
   while (cnt > 0) {
      if (cnt & 1) {
         mul(r, s, t) ;
         assignpos(r, t) ;
      }
      cnt >>= 1 ;
      mul(s, s, t) ;
      assignpos(s, t) ;
   }
   assignpos(b, r) ;
}
void puzdef::inv(const setval a, setval b) const {
   const uchar *ap = a.dat ;
   uchar *bp = b.dat ;
   for (int i=0; i<(int)setdefs.size(); i++) {
      const setdef &sd = setdefs[i] ;
      int n = sd.size ;
      if (sd.omod == 1) {
         for (int j=0; j<n; j++) {
            bp[ap[j]] = j ;
            bp[j+n] = 0 ;
         }
      } else {
         uchar *moda = gmoda[sd.omod] ;
         for (int j=0; j<n; j++) {
            bp[ap[j]] = j ;
            bp[ap[j]+n] = moda[sd.omod-ap[j+n]] ;
         }
      }
      ap += 2 * n ;
      bp += 2 * n ;
   }
}
vector<ll> fact ;
struct generatingset {
   generatingset(const puzdef &pd) ;
   const puzdef &pd ;
   setval e ;
   vector<vector<setval>> sgs, sgsi, tk ;
   bool resolve(const setval p_) {
      stacksetval p(pd), t(pd) ;
      pd.assignpos(p, p_) ;
      for (int i=(int)pd.setdefs.size()-1; i>=0; i--) {
         const setdef &sd = pd.setdefs[i] ;
         int n = sd.size ;
         int s = 0 ;
         for (int j=0; j<n; j++)
            s += p.dat[sd.off+j] ;
         if (s * 2 != n * (n - 1))
            error("! identical pieces during generating set resolve?") ;
         int off = (sd.off >> 1) ;
         for (int j=n-1; j>=0; j--) {
            if (p.dat[sd.off+j] != j || p.dat[sd.off+n+j] != 0) {
               int v = sd.omod * p.dat[sd.off+j] + p.dat[sd.off+n+j] ;
               if (!sgs[off+j][v].dat)
                  return 0 ;
               pd.mul(sgsi[off+j][v], p, t) ;
               swap(p.dat, t.dat) ;
               if (p.dat[sd.off+j] != j || p.dat[sd.off+n+j] != 0)
                  error("! misresolve") ;
            }
         }
      }
      return 1 ;
   }
   void knutha(int k1, int k2, const setval &p) {
      int k = k2 + (pd.setdefs[k1].off >> 1) ;
      tk[k].push_back(allocsetval(pd, p)) ;
      stacksetval p2(pd) ;
      for (int i=0; i<(int)sgs[k].size(); i++)
         if (sgs[k][i].dat) {
            pd.mul(p, sgs[k][i], p2) ;
            knuthb(k1, k2, p2) ;
         }
   }
   void knuthb(int k1, int k2, const setval &p) {
      const setdef &sd = pd.setdefs[k1] ;
      int k = k2 + (sd.off >> 1) ;
      int n = sd.size ;
      int j = p.dat[sd.off+k2] * sd.omod + p.dat[sd.off+n+k2] ;
      stacksetval p2(pd) ;
      if (!sgs[k][j].dat) {
         sgs[k][j] = allocsetval(pd, p) ;
         sgsi[k][j] = allocsetval(pd, p) ;
         pd.inv(sgs[k][j], sgsi[k][j]) ;
         for (int i=0; i<(int)tk[k].size(); i++) {
            pd.mul(tk[k][i], p, p2) ;
            knuthb(k1, k2, p2) ;
         }
         return ;
      }
      pd.mul(sgsi[k][j], p, p2) ;
      if (p2.dat[sd.off+k2] != k2 || p2.dat[sd.off+n+k2] != 0) {
         error("! misresolve in knuthb") ;
      }
      if (!resolve(p2)) {
         if (k2 > 0)
            k2-- ;
         else {
            k1-- ;
            if (k1 < 0)
               error("! fell off end in knuthb") ;
            k2 = pd.setdefs[k1].size - 1 ;
         }
         knutha(k1, k2, p2) ;
      }
   }
} ;
generatingset::generatingset(const puzdef &pd_) : pd(pd_), e(pd.id) {
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
      const setdef &sd = pd.setdefs[i] ;
      int sz = sd.size * sd.omod ;
      for (int j=0; j<sd.size; j++) {
         sgs.push_back(vector<setval>(sz)) ;
         sgsi.push_back(vector<setval>(sz)) ;
         tk.push_back(vector<setval>(0)) ;
         int at = sgs.size() - 1 ;
         sgs[at][j*sd.omod] = e ;
         sgsi[at][j*sd.omod] = e ;
      }
   }
   int oldprec = cout.precision() ;
   cout.precision(20) ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      if (resolve(pd.moves[i].pos))
         continue ;
      knutha(pd.setdefs.size()-1, pd.setdefs[pd.setdefs.size()-1].size-1,
             pd.moves[i].pos) ;
      long double totsize = 1 ;
      for (int j=0; j<(int)sgs.size(); j++) {
         int cnt = 0 ;
         for (int k=0; k<(int)sgs[j].size(); k++)
            if (sgs[j][k].dat)
               cnt++ ;
         totsize *= cnt ;
      }
      cout << "Adding move " << pd.moves[i].name << " extends size to " << totsize << endl ;
   }
   cout.precision(oldprec) ;
}
generatingset *gs ;
ll maxmem = 8LL * 1024LL * 1024LL * 1024LL ;
int verbose = 1 ;
int quarter = 0 ;
int nocorners, noedges, nocenters ;
int warn(string msg, string extra="") {
   cerr << msg << extra << endl ;
   return 0 ;
}
vector<string> getline(FILE *f, ull &checksum) {
   string s ;
   int c ;
   while (1) {
      s.clear() ;
      while (1) {
         c = getc(f) ;
         if (c != EOF)
            checksum = 31 * checksum + c ;
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
      if (verbose > 2)
         cout << ">> " << s << endl ;
      if (s[0] == '#')
         continue ;
      string tok ;
      for (int i=0; i<(int)s.size(); i++) {
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
   if (cnt != (int)toks.size())
      error("! wrong number of tokens on line") ;
}
// must be a number under 256.
int getnumber(int minval, const string &s) {
   int r = 0 ;
   for (int i=0; i<(int)s.size(); i++) {
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
int omitset(string s) {
   if (s.size() < 2)
      return 0 ;
   if (nocorners && tolower(s[0]) == 'c' && tolower(s[1]) == 'o')
      return 1 ;
   if (nocenters && tolower(s[0]) == 'c' && tolower(s[1]) == 'e')
      return 1 ;
   if (noedges && tolower(s[0]) == 'e' && tolower(s[1]) == 'd')
      return 1 ;
   return 0 ;
}
setval readposition(puzdef &pz, char typ, FILE *f, ull &checksum) {
   setval r((uchar *)calloc(pz.totsize, 1)) ;
   int curset = -1 ;
   int numseq = 0 ;
   int ignore = 0 ;
   while (1) {
      vector<string> toks = getline(f, checksum) ;
      if (toks.size() == 0)
         error("! premature end while reading position") ;
      if (toks[0] == "End") {
         if (curset >= 0 && numseq == 0 && ignore == 0)
            error("! empty set def?") ;
         expect(toks, 1) ;
         ignore = 0 ;
         break ;
      }
      if (isnumber(toks[0])) {
         if (ignore)
            continue ;
         if (curset < 0 || numseq > 1)
            error("! unexpected number sequence") ;
         int n = pz.setdefs[curset].size ;
         expect(toks, n) ;
         uchar *p = r.dat + pz.setdefs[curset].off + numseq * n ;
         for (int i=0; i<n; i++)
            p[i] = getnumber(1-numseq, toks[i]) ;
         if (numseq == 1 && ignoreori)
            for (int i=0; i<n; i++)
               p[i] = 0 ;
         numseq++ ;
      } else {
         if (curset >= 0 && numseq == 0)
            error("! empty set def?") ;
         expect(toks, 1) ;
         ignore = 0 ;
         if (omitset(toks[0])) {
            ignore = 1 ;
            continue ;
         }
         curset = -1 ;
         for (int i=0; i<(int)pz.setdefs.size(); i++)
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
   for (int i=0; i<(int)pz.setdefs.size(); i++) {
      uchar *p = r.dat + pz.setdefs[i].off ;
      int n = pz.setdefs[i].size ;
      if (p[0] == 0) {
         if (typ == 'S') {
            for (int j=0; j<n; j++)
               p[j] = pz.solved.dat[pz.setdefs[i].off+j] ;
         } else {
            for (int j=0; j<n; j++)
               p[j] = j ; // identity perm
            if (typ == 's')
               pz.setdefs[i].psum = n * (n - 1) / 2 ;
         }
      } else {
         vector<int> cnts ;
         int sum = 0 ;
         for (int j=0; j<n; j++) {
            int v = --p[j] ;
            sum += v ;
            if (v >= (int)cnts.size())
               cnts.resize(v+1) ;
            cnts[v]++ ;
         }
         if (typ == 's')
            pz.setdefs[i].psum = sum ;
         for (int j=0; j<(int)cnts.size(); j++)
            if (cnts[j] == 0)
               error("! values are not contiguous") ;
         if ((int)cnts.size() != n) {
            if (typ == 'S') {
               if (!(cnts == pz.setdefs[i].cnts))
                  error("! scramble position permutation doesn't match solved") ;
            } else if (typ != 's')
               error("! expected, but did not see, a proper permutation") ;
            else {
               pz.setdefs[i].uniq = 0 ;
               pz.setdefs[i].cnts = cnts ;
               pz.setdefs[i].pbits = ceillog2(cnts.size()) ;
            }
         } else {
            if (typ != 'S' && oddperm(p, n))
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
   ull checksum = 0 ;
   pz.optionssum = 0;
   while (1) {
      vector<string> toks = getline(f, checksum) ;
      if (toks.size() == 0)
         break ;
      if (toks[0] == "Name") {
         if (state != 0)
            error("! Name in wrong place") ;
         state++ ;
         expect(toks, 2) ;
         pz.name = twstrdup(toks[1].c_str()) ; ;
      } else if (toks[0] == "Set") {
         if (state == 0) {
            pz.name = "Unnamed" ;
            state++ ;
         }
         if (state != 1)
            error("! Set in wrong place") ;
         expect(toks, 4) ;
         if (omitset(toks[1]))
            continue ;
         setdef sd ;
         sd.name = twstrdup(toks[1].c_str()) ;
         sd.size = getnumber(1, toks[2]) ;
         sd.omod = getnumber(1, toks[3]) ;
         if (ignoreori)
            sd.omod = 1 ;
         sd.pparity = (sd.size == 1 ? 0 : 1) ;
         sd.oparity = 1 ;
         sd.pbits = ceillog2(sd.size) ;
         sd.pibits = sd.pbits ;
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
      } else if (toks[0] == "Illegal") {
         if (state < 2)
            error("! Illegal must be after solved") ;
         // set name, position, value, value, value, value ...
         for (int i=3; i<(int)toks.size(); i++)
            pz.addillegal(toks[1].c_str(),
                          getnumber(1, toks[2]), getnumber(1, toks[i])) ;
      } else if (toks[0] == "Solved") {
         if (state != 1)
            error("! Solved in wrong place") ;
         state++ ;
         expect(toks, 1) ;
         pz.solved = readposition(pz, 's', f, checksum) ;
      } else if (toks[0] == "Move") {
         if (state != 2)
            error("! Move in wrong place") ;
         expect(toks, 2) ;
         moove m ;
         m.name = twstrdup(toks[1].c_str()) ;
         m.pos = readposition(pz, 'm', f, checksum) ;
         m.cost = 1 ;
         m.twist = 1 ;
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
   pz.id = setval((uchar *)calloc(pz.totsize, 1)) ;
   uchar *p = pz.id.dat ;
   for (int i=0; i<(int)pz.setdefs.size(); i++) {
      int n = pz.setdefs[i].size ;
      for (int j=0; j<n; j++)
         p[j] = j ;
      p += n ;
      for (int j=0; j<n; j++)
         p[j] = 0 ;
      p += n ;
   }
   pz.checksum = checksum ;
   return pz ;
}
void addmovepowers(puzdef &pd) {
   vector<moove> newmoves ;
   pd.basemoves = pd.moves ;
   stacksetval p1(pd), p2(pd) ;
   vector<string> newnames ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      moove &m = pd.moves[i] ;
      if (quarter && m.cost > 1)
         continue ;
      vector<setval> movepowers ;
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
      for (int j=0; j<(int)movepowers.size(); j++) {
         int tw = j + 1 ;
         if (order - tw < tw)
            tw -= order ;
         moove m2 = m ;
         m2.pos = movepowers[j] ;
         m2.cost = abs(tw) ;
         m2.twist = (tw + order) % order ;
         if (tw != 1) {
            string s2 = m.name ;
            if (tw != -1)
               s2 += to_string(abs(tw)) ;
            if (tw < 0)
               s2 += "'" ;
            newnames.push_back(s2) ;
            m2.name = twstrdup(s2.c_str()) ;
         }
         newmoves.push_back(m2) ;
      }
   }
   if (newnames.size() > 0) {
      pd.moves = newmoves ;
      if (verbose) {
         cout << "Created new moves" ;
         for (int i=0; i<(int)newnames.size(); i++)
            cout << " " << newnames[i] ;
         cout << endl << flush ;
      }
   } else {
      pd.moves = pd.basemoves ;
   }
}
double dllstates ;
void calculatesizes(puzdef &pd) {
   ull gllstates = 1 ;
   double glogstates = 0 ;
   dllstates = 1 ;
   for (int i=0; i<(int)pd.setdefs.size(); i++) {
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
         for (int j=0; j<(int)sd.cnts.size(); j++) {
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
ull densepack(const puzdef &pd, setval pos) {
   ull r = 0 ;
   ull m = 1 ;
   uchar *p = pos.dat ;
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
vector<ull> cnts ;
/*
 *   Antipod recovery
 */
int looseper, looseiper ;
ll antipodecount = 20 ;
ll antipodeshave ;
int resetonnewantipode = 0 ;
loosetype *antipodesloose ;
ull *antipodesdense ;
void emitposition(const puzdef &pd, setval p, const char *s) ;
void looseunpack(const puzdef &pd, setval pos, loosetype *r) ;
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos) ;
void showantipodes(const puzdef &pd, loosetype *beg, loosetype *end) {
   ll t = (end - beg) / looseper ;
   if (t > antipodecount)
      t = antipodecount ;
   antipodeshave = t ;
   cout << "Showing " << antipodeshave << " antipodes." << endl ;
   stacksetval pos(pd) ;
   for (int i=0; i<antipodeshave; i++) {
      looseunpack(pd, pos, beg+i*looseper) ;
      emitposition(pd, pos, nullptr) ;
   }
}
void resetantipodes() {
   resetonnewantipode = 1 ;
}
void showantipodesloose(const puzdef &pd) {
   showantipodes(pd, antipodesloose, antipodesloose+looseper*antipodecount) ;
}
void showantipodesdense(const puzdef &pd, int ordered) {
   if (antipodesdense == 0)
      error("! no antipodes?") ;
   cout << "Showing " << antipodeshave << " antipodes." << endl ;
   stacksetval pos(pd) ;
   for (int i=0; i<antipodeshave; i++) {
      if (ordered)
         denseunpack_ordered(pd, antipodesdense[i], pos) ;
      else
         denseunpack(pd, antipodesdense[i], pos) ;
      emitposition(pd, pos, nullptr) ;
   }
}
void stashantipodesloose(loosetype *beg, loosetype *end) {
   if (end == beg)
      return ;
   if (antipodesloose == 0)
      antipodesloose =
               (loosetype *)calloc(antipodecount, looseper*sizeof(loosetype)) ;
   ll t = (end - beg) / looseper ;
   if (t > antipodecount)
      t = antipodecount ;
   antipodeshave = t ;
   memcpy(antipodesloose, beg, antipodeshave * looseper * sizeof(loosetype)) ;
}
void stashantipodedense(ull val) {
   if (resetonnewantipode) {
      antipodeshave = 0 ;
      resetonnewantipode = 0 ;
   }
   if (antipodeshave < antipodecount) {
      if (antipodesdense == 0)
         antipodesdense = (ull *)calloc(antipodecount, sizeof(ull)) ;
      antipodesdense[antipodeshave++] = val ;
   }
}
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
vector<pair<ull, int> > parts ;
int nmoves ;
vector<int> movemap ;
ull densepack_ordered(const puzdef &pd, setval pos) {
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
   looseper = (bits + BITSPERLOOSE - 1) / BITSPERLOOSE ;
   looseiper = (ibits + BITSPERLOOSE - 1) / BITSPERLOOSE ;
   cout << "Requiring " << looseper*sizeof(loosetype) << " bytes per entry; " << (looseiper*sizeof(loosetype)) << " from identity." << endl ;
}
void loosepack(const puzdef &pd, setval pos, loosetype *w, int fromid=0) {
   uchar *p = pos.dat ;
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
void looseunpack(const puzdef &pd, setval pos, loosetype *r) {
   uchar *p = pos.dat ;
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
 *   Do canonicalization calculations by finding commutating moves.
 *   Now we embed quarter-turn logic in here, which significantly
 *   complicates this, but simplifies all the search routines.
 */
vector<ull> canonmask ;
vector<vector<int> > canonnext ;
vector<ull> canonseqcnt ;
vector<ull> canontotcnt ;
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
   for (int i=0; i<(int)pd.moves.size(); i++)
      for (int j=0; j<i; j++) {
         pd.mul(pd.moves[i].pos, pd.moves[j].pos, p1) ;
         pd.mul(pd.moves[j].pos, pd.moves[i].pos, p2) ;
         if (pd.comparepos(p1, p2) != 0) {
            commutes[pd.moves[i].cs] &= ~(1LL << pd.moves[j].cs) ;
            commutes[pd.moves[j].cs] &= ~(1LL << pd.moves[i].cs) ;
         }
      }
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
         if ((stateb & commutes[m] & ((1LL << m) - 1)) != 0) {
            canonmask[fromst] |= 1LL << m ;
            continue ;
         }
         if (!quarter && (((stateb >> m) & 1) != 0)) {
            canonmask[fromst] |= 1LL << m ;
            continue ;
         }
         ull nstb = (stateb & commutes[m]) | (1LL << m) ;
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
template<typename T>
struct hashvector {
   size_t operator()(const T&v) const {
      return CityHash64((const char *)&v[0], sizeof(T)*v.size()) ;
   }
} ;
template<typename T>
void freeContainer(T& c) {
   T empty;
   swap(c, empty);
}
map<ull,int> statemap ;
int movebits, ccount ;
vector<loosetype> ccenc ;
unordered_set<vector<loosetype>, hashvector<vector<loosetype>>> ccseen ;
vector<allocsetval> posns ;
vector<int> movehist ;
vector<int> ccnextstate ;
int ccstalloc = 0 ;
int ccnbase = 0 ;
int recurcanonstates2(const puzdef &pd, int togo, ull moveset, int sp) {
   if (togo == 0) {
      loosepack(pd, posns[sp], &ccenc[0], 1) ;
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
void showcanon(const puzdef &pd, int show) {
   cout.precision(16) ;
   int nstates = canonmask.size() ;
   vector<vector<double> > counts ;
   vector<double> zeros(nstates) ;
   counts.push_back(zeros) ;
   counts[0][0] = 1 ;
   double gsum = 0 ;
   double osum = 1 ;
   for (int d=0; d<=100; d++) {
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
      if (sum == 0 || gsum > 1e18)
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
}
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting, but this time using a recursive
 *   enumeration process rather than using a frontier.
 */
ll bigcnt = 0 ;
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
map<ll, int> bestsofar ;
const int HIWR = 4 ;
ll extendkey(ll k, int nwr, int npwr) {
   return k * 10 + nwr * 2 + (npwr == 0 ? 0 : 1) ;
}
void recurfindalgo(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      bigcnt++ ;
      int wr = pd.numwrong(posns[sp], pd.solved) ;
      if (wr > HIWR || wr == 0)
         return ;
      ll key = 0 ;
      for (int i=0; i<(int)pd.setdefs.size(); i++) {
         key = extendkey(key,
                          pd.numwrong(posns[sp], pd.solved, 1LL << i),
                          pd.permwrong(posns[sp], pd.solved, 1LL << i)) ;
      }
      int mvs = sp ;
      if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
         return ;
      bestsofar[key] = mvs ;
      cout << key << " " << mvs << " (" ;
      for (int i=0; i<sp; i++) {
         if (i)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << ")" << endl << flush ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      recurfindalgo(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void findalgos(const puzdef &pd) {
   for (int d=1; ; d++) {
      while ((int)posns.size() <= d + 1) {
         posns.push_back(allocsetval(pd, pd.solved)) ;
         movehist.push_back(-1) ;
      }
      bigcnt = 0 ;
      recurfindalgo(pd, d, 0, 0) ;
      cout << "At " << d << " big count is " << bigcnt << " in " << duration() << endl ;
   }
}
void recurfindalgo2(const puzdef &pd, int togo, int sp, int st) {
   if (togo == 0) {
      vector<int> cc = pd.cyccnts(posns[sp]) ;
      ll o = puzdef::order(cc) ;
      for (int pp=2; pp<=3; pp++) {
         if (o % pp == 0) {
            pd.pow(posns[sp], posns[sp+1], o/pp) ;
            int wr = pd.numwrong(posns[sp+1], pd.id) ;
            if (wr > HIWR || wr == 0)
               continue ;
            ll key = 0 ;
            for (int i=0; i<(int)pd.setdefs.size(); i++) {
               key = extendkey(key, pd.numwrong(posns[sp+1], pd.id, 1LL << i),
                                pd.permwrong(posns[sp+1], pd.id, 1LL << i)) ;
            }
            int mvs = o / pp * sp ;
            if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
               continue ;
            bestsofar[key] = mvs ;
            cout << pp << " " << key << " " << mvs << " (" ;
            for (int i=0; i<sp; i++) {
               if (i)
                  cout << " " ;
               cout << pd.moves[movehist[i]].name ;
            }
            cout << ")" << (mvs / sp) << " (" ;
            const char *spacer = "" ;
            for (int i=1; i<(int)cc.size(); i++) {
               if (cc[i]) {
                  cout << spacer ;
                  spacer = " " ;
                  cout << i << ":" << cc[i] ;
               }
            }
            cout << ") " ;
            cout << o << endl << flush ;
         }
      }
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      recurfindalgo2(pd, togo-1, sp+1, ns[mv.cs]) ;
   }
}
void findalgos2(const puzdef &pd) {
   for (int d=1; ; d++) {
      while ((int)posns.size() <= d + 3) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      recurfindalgo2(pd, d, 0, 0) ;
   }
}
void recurfindalgo3b(const puzdef &pd, int togo, int sp, int st, int fp) {
   if (togo == 0) {
      pd.inv(posns[sp], posns[sp+1]) ;
      pd.mul(posns[fp], posns[sp], posns[sp+2]) ;
      pd.mul(posns[sp+2], posns[fp+1], posns[sp+3]) ;
      pd.mul(posns[sp+3], posns[sp+1], posns[sp+2]) ;
      int wr = pd.numwrong(posns[sp+2], pd.id) ;
      if (wr > HIWR || wr == 0)
         return ;
      ll key = 0 ;
      for (int i=0; i<(int)pd.setdefs.size(); i++) {
         key = extendkey(key, pd.numwrong(posns[sp+2], pd.id, 1LL << i),
                          pd.permwrong(posns[sp+2], pd.id, 1LL << i)) ;
      }
      int mvs = 2 * (fp + (sp - (fp + 2))) ;
      if (bestsofar.find(key) != bestsofar.end() && bestsofar[key] < mvs)
         return ;
      bestsofar[key] = mvs ;
      cout << key << " " << mvs << " [" ;
      for (int i=0; i<fp; i++) {
         if (i)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << "," ;
      for (int i=fp+2; i<sp; i++) {
         if (i != fp+2)
            cout << " " ;
         cout << pd.moves[movehist[i]].name ;
      }
      cout << "]" << endl << flush ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      recurfindalgo3b(pd, togo-1, sp+1, ns[mv.cs], fp) ;
   }
}
void recurfindalgo3a(const puzdef &pd, int togo, int sp, int st, int b) {
   if (togo == 0) {
      pd.inv(posns[sp], posns[sp+1]) ;
      pd.assignpos(posns[sp+2], pd.id) ;
      recurfindalgo3b(pd, b, sp+2, 0, sp) ;
      return ;
   }
   ull mask = canonmask[st] ;
   const vector<int> &ns = canonnext[st] ;
   for (int m=0; m<(int)pd.moves.size(); m++) {
      const moove &mv = pd.moves[m] ;
      if ((mask >> mv.cs) & 1)
         continue ;
      movehist[sp] = m ;
      pd.mul(posns[sp], mv.pos, posns[sp+1]) ;
      recurfindalgo3a(pd, togo-1, sp+1, ns[mv.cs], b) ;
   }
}
void findalgos3(const puzdef &pd) {
   for (int d=2; ; d++) {
      while ((int)posns.size() <= d + 7) {
         posns.push_back(allocsetval(pd, pd.id)) ;
         movehist.push_back(-1) ;
      }
      for (int a=1; a+a<=d; a++)
         recurfindalgo3a(pd, d-a, 0, 0, a) ;
   }
}
// we take advantage of the fact that the totsize is always divisible by 2.
ull fasthash(int n, const setval sv) {
   ull r = CityHash64((const char *)sv.dat, n) ;
   // this little hack ensures that at least one of bits 1..7
   // (numbered from zero) is set.
   r ^= ((r | (1LL << 43)) & ((r & 0xfe) - 2)) >> 42 ;
   return r ;
}
vector<ull> workchunks ;
vector<int> workstates ;
int workat ;
void makeworkchunks(const puzdef &pd, int d) {
   workchunks.clear() ;
   workstates.clear() ;
   workchunks.push_back(1) ;
   workstates.push_back(0) ;
   int nmoves = pd.moves.size() ;
   int chunkmoves = 0 ;
   if (numthreads > 1 && d >= 3) {
      ull mul = 1 ;
      while (chunkmoves + 3 < d && (int)workchunks.size() < 40 * numthreads) {
         vector<ull> wc2 ;
         vector<int> ws2 ;
         for (int i=0; i<(int)workchunks.size(); i++) {
            ull pmv = workchunks[i] ;
            int st = workstates[i] ;
            ull mask = canonmask[st] ;
            const vector<int> &ns = canonnext[st] ;
            for (int mv=0; mv<nmoves; mv++)
               if (0 == ((mask >> pd.moves[mv].cs) & 1)) {
                  wc2.push_back(pmv + (nmoves + mv - 1) * mul) ;
                  ws2.push_back(ns[pd.moves[mv].cs]) ;
               }
         }
         swap(wc2, workchunks) ;
         swap(ws2, workstates) ;
         chunkmoves++ ;
         mul *= nmoves ;
      }
   }
}
struct prunetable ;
struct workerparam {
   workerparam(const puzdef &pd_, prunetable &pt_, int tid_) :
      pd(pd_), pt(pt_), tid(tid_) {}
   const puzdef &pd ;
   prunetable &pt ;
   int tid ;
} ;
vector<workerparam> workerparams ;
void setupparams(const puzdef &pd, prunetable &pt, int numthreads) {
   while ((int)workerparams.size() < numthreads) {
      int i = workerparams.size() ;
      workerparams.push_back(workerparam(pd, pt, i)) ;
   }
}
int setupthreads(const puzdef &pd, prunetable &pt) {
   int wthreads = min(numthreads, (int)workchunks.size()) ;
   workat = 0 ;
   setupparams(pd, pt, wthreads) ;
   return wthreads ;
}
const int BLOCKSIZE = 8192 ; // in long longs
const int FILLCHUNKS = 256 ; // long longs
struct fillbuf {
   int nchunks ;
   ull chunks[FILLCHUNKS] ;
} ;
struct fillworker {
   vector<allocsetval> posns ;
   int d ;
   fillbuf fillbufs[MEMSHARDS] ;
   char pad[256] ;
   void init(const puzdef &pd, int d_) {
      while (posns.size() <= 100 || (int)posns.size() <= d_+1)
         posns.push_back(allocsetval(pd, pd.solved)) ;
      pd.assignpos(posns[0], pd.solved) ;
      d = d_ ;
      for (int i=0; i<MEMSHARDS; i++)
         fillbufs[i].nchunks = 0 ;
   }
   ull fillstart(const puzdef &pd, prunetable &pt, int w) ;
   ull fillflush(const prunetable &pt, int shard) ;
   void dowork(const puzdef &pd, prunetable &pt) ;
   ull filltable(const puzdef &pd, prunetable &pt, int togo, int sp, int st) ;
} fillworkers[MAXTHREADS] ;
void *fillthreadworker(void *o) {
   workerparam *wp = (workerparam *)o ;
   fillworkers[wp->tid].dowork(wp->pd, wp->pt) ;
   return 0 ;
}
struct ioworkitem {
   char state ;
   ull *mem ;
   ull longcnt ;
   uchar *buf ;
   prunetable *pt ;
   unsigned int bytecnt ;
} ;
void *unpackworker(void *o) ;
void *packworker(void *o) ;
struct ioqueue {
   void init(struct prunetable *pt_, FILE *f_ = 0) {
      pt = pt_ ;
      f = f_ ;
      for (int i=0; i<numthreads; i++)
         ioworkitems[i].state = 0 ;
      nextthread = 0 ;
   }
   void waitthread(int i) {
      join_thread(i) ;
      if (ioworkitems[i].state == 2) {
         unsigned int bytecnt = ioworkitems[i].bytecnt ;
         unsigned int longcnt = ioworkitems[i].longcnt ;
         putc(bytecnt & 255, f) ;
         putc((bytecnt >> 8) & 255, f) ;
         putc((bytecnt >> 16) & 255, f) ;
         putc((bytecnt >> 24) & 255, f) ;
         putc(longcnt & 255, f) ;
         putc((longcnt >> 8) & 255, f) ;
         putc((longcnt >> 16) & 255, f) ;
         putc((longcnt >> 24) & 255, f) ;
         if (fwrite(ioworkitems[i].buf, 1, bytecnt, f) != bytecnt)
            error("! I/O error writing block") ;
         free(ioworkitems[i].buf) ;
      }
   }
   void queuepackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) {
      if (ioworkitems[nextthread].state != 0) {
         waitthread(nextthread) ;
         ioworkitems[nextthread].state = 0 ;
      }
      ioworkitems[nextthread].mem = mem ;
      ioworkitems[nextthread].longcnt = longcnt ;
      ioworkitems[nextthread].buf = buf ;
      ioworkitems[nextthread].bytecnt = bytecnt ;
      ioworkitems[nextthread].pt = pt ;
      ioworkitems[nextthread].state = 2 ;
      spawn_thread(nextthread, packworker, &ioworkitems[nextthread]) ;
      nextthread++ ;
      if (nextthread >= numthreads)
         nextthread = 0 ;
   }
   void queueunpackwork(ull *mem, ull longcnt,
                        uchar *buf, unsigned int bytecnt) {
      if (ioworkitems[nextthread].state != 0) {
         waitthread(nextthread) ;
         ioworkitems[nextthread].state = 0 ;
      }
      ioworkitems[nextthread].mem = mem ;
      ioworkitems[nextthread].longcnt = longcnt ;
      ioworkitems[nextthread].buf = buf ;
      ioworkitems[nextthread].bytecnt = bytecnt ;
      ioworkitems[nextthread].pt = pt ;
      ioworkitems[nextthread].state = 1 ;
      spawn_thread(nextthread, unpackworker, &ioworkitems[nextthread]) ;
      nextthread++ ;
      if (nextthread >= numthreads)
         nextthread = 0 ;
   }
   void finishall() {
      for (int i=0; i<numthreads; i++) {
         if (ioworkitems[nextthread].state != 0)
            waitthread(nextthread) ;
         nextthread = (nextthread + 1) % numthreads ;
      }
   }
   int nextthread ;
   struct prunetable *pt ;
   ioworkitem ioworkitems[MAXTHREADS] ;
   FILE *f ;
} ioqueue ;
struct prunetable {
   prunetable(const puzdef &pd, ull maxmem) {
      totsize = pd.totsize ;
      ull bytesize = 16 ;
      while (2 * bytesize <= maxmem &&
             (pd.logstates > 55 || 8 * bytesize < pd.llstates))
         bytesize *= 2 ;
      size = bytesize * 4 ;
      shardshift = 0 ;
      while ((size >> shardshift) > MEMSHARDS)
         shardshift++ ;
      hmask = size - 1 ;
      totpop = 0 ;
      int base = 1 ;
      while (base + 2 < (int)canontotcnt.size() && canontotcnt[base+2] < size)
         base++ ;
      // hack memalign
      mem = (ull *)malloc(CACHELINESIZE + (bytesize >> 3) * sizeof(ull)) ;
      while (((ull)mem) & (CACHELINESIZE - 1))
         mem++ ;
      lookupcnt = 0 ;
      fillcnt = 0 ;
      hibase = base ;
      justread = 0 ;
      if (!readpt(pd)) {
         memset(mem, -1, bytesize) ;
         baseval = min(hibase, 2) ;
         for (int d=0; d<=baseval+1; d++) {
            int val = 0 ;
            if (d >= baseval)
               val = d - baseval + 1 ;
            wval = val ;
            wbval = min(d, 15) ;
            filltable(pd, d) ;
         }
      }
      writept(pd) ;
   }
   void filltable(const puzdef &pd, int d) {
      popped = 0 ;
      cout << "Filling table at depth " << d << " with val " << wval << flush ;
      makeworkchunks(pd, d) ;
      int wthreads = setupthreads(pd, *this) ;
      for (int t=0; t<wthreads; t++)
         fillworkers[t].init(pd, d) ;
      for (int i=0; i<wthreads; i++)
         spawn_thread(i, fillthreadworker, &(workerparams[i])) ;
      for (int i=0; i<wthreads; i++)
         join_thread(i) ;
      fillcnt += canonseqcnt[d] ;
      cout << " saw " << popped << " (" << canonseqcnt[d] << ") in "
           << duration() << endl << flush ;
      totpop += popped ;
      justread = 0 ;
   }
   void checkextend(const puzdef &pd) {
      if (lookupcnt < 3 * fillcnt || baseval > 100 || totpop * 2 > size ||
          baseval > hibase ||
          (pd.logstates <= 50 && canonseqcnt[baseval+2] > pd.llstates))
         return ;
      ull longcnt = (size + 31) >> 5 ;
      cout << "Demoting memory values " << flush ;
      for (ull i=0; i<longcnt; i += 8) {
         // decrement 1's and 2's; leave 3's alone
         // watch out for first element; the 0 in the first one is not a mistake
         ull v = mem[i] ;
         mem[i] = v - ((v ^ (v >> 1)) & 0x5555555555555550LL) ;
         v = mem[i+1] ;
         mem[i+1] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+2] ;
         mem[i+2] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+3] ;
         mem[i+3] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+4] ;
         mem[i+4] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+5] ;
         mem[i+5] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+6] ;
         mem[i+6] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
         v = mem[i+7] ;
         mem[i+7] = v - ((v ^ (v >> 1)) & 0x5555555555555555LL) ;
      }
      cout << "in " << duration() << endl << flush ;
      baseval++ ;
      wval = 2 ;
      wbval = baseval+1 ;
      filltable(pd, baseval+1) ;
      writept(pd) ;
   }
   int lookuph(ull h) const {
      h &= hmask ;
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 0)
         return mem[(h >> 5) & ~7] & 15 ;
      else
         return v + baseval - 1 ;
   }
   void prefetch(ull h) const {
      __builtin_prefetch(mem+((h & hmask) >> 5)) ;
   }
   int lookup(const setval sv) const {
      ull h = fasthash(totsize, sv) & hmask ;
      int v = 3 & (mem[h >> 5] >> ((h & 31) * 2)) ;
      if (v == 0)
         return mem[(h >> 5) & ~7] & 15 ;
      else
         return v + baseval - 1 ;
   }
   void addlookups(ull lookups) {
      lookupcnt += lookups ;
   }
   // if someone set options that affect the hash, we add a suffix to the
   // data file name to reflect this.
   void addsumdat(const puzdef &pd, string &filename) const {
      filename.push_back('-') ;
      filename.push_back('o') ;
      ull t = pd.optionssum ;
      while (t) {
         int v = t % 36 ;
         t /= 36 ;
         if (v < 10)
            filename.push_back('0'+v) ;
         else
            filename.push_back('a'+(v-10)) ;
      }
   }
   string makefilename(const puzdef &pd) const {
      string filename = "tws2-" + inputbasename + "-" ;
      if (quarter)
         filename += "q-" ;
      ull bytes = size >> 2 ;
      char suffix = 0 ;
      if (bytes >= 1024) {
         suffix = 'K' ;
         bytes >>= 10 ;
      }
      if (bytes >= 1024) {
         suffix = 'M' ;
         bytes >>= 10 ;
      }
      if (bytes >= 1024) {
         suffix = 'G' ;
         bytes >>= 10 ;
      }
      if (bytes >= 1024) {
         suffix = 'T' ;
         bytes >>= 10 ;
      }
      filename += to_string(bytes) ;
      if (suffix)
         filename += suffix ;
      if (pd.optionssum)
         addsumdat(pd, filename) ;
      filename += ".dat" ;
      return filename ;
   }
   ull calcblocksize(ull *mem, ull longcnt) {
      ull bits = 0 ;
      for (ull i=0; i<longcnt; i++) {
         ull v = mem[i] ;
         for (int j=0; j<8; j++) {
            bits += codewidths[v & 255] ;
            v >>= 8 ;
         }
      }
      return ((bits + 7) >> 3) ;
   }
   void packblock(ull *mem, ull longcnt, uchar *buf, ull bytecnt) {
      ull accum = 0 ;
      int havebits = 0 ;
      ull bytectr = 0 ;
      for (ull i=0; i<longcnt; i++) {
         ull v = mem[i] ;
         for (int j=0; j<8; j++) {
            int cp = v & 255 ;
            int cpw = codewidths[cp] ;
            if (cpw == 0)
               error("! internal error in Huffman encoding") ;
            while (havebits + cpw > 64) {
               buf[bytectr++] = accum >> (havebits - 8) ;
               if (bytectr > bytecnt)
                  error("! packing issue") ;
               havebits -= 8 ;
            }
            accum = (accum << cpw) + codevals[cp] ;
            havebits += cpw ;
            v >>= 8 ;
         }
      }
      int extra = (8 - havebits) & 7 ;
      havebits += extra ;
      accum <<= extra ;
      while (havebits > 0) {
         buf[bytectr++] = accum >> (havebits - 8) ;
         if (bytectr > bytecnt)
            error("! packing issue 2") ;
         havebits -= 8 ;
      }
      if (bytectr != bytecnt)
         error("! packing issue 3") ;
   }
   void unpackblock(ull *mem, ull longcnt, uchar *block, int bytecnt) {
      int bytectr = 0 ;
      int havebits = 0 ;
      ull accum = 0 ;
      for (ull i=0; i<longcnt; i++) {
         ull v = 0 ;
         for (int j=0; j<8; j++) {
            int bitsneeded = 8 ;
            int k = 0 ;
            while (1) {
               if (havebits < bitsneeded) {
                  int c = 0 ;
                  if (bytectr < bytecnt)
                     c = block[bytectr++] ;
                  accum = (accum << 8) + c ;
                  havebits += 8 ;
               }
               int cp = tabs[k][accum >> (havebits - bitsneeded)] ;
               if (cp >= 0) {
                  v += ((ull)cp) << (8 * j) ;
                  havebits -= codewidths[cp] ;
                  if (havebits > 14)
                     error("! oops; should not have this many bits left") ;
                  accum &= ((1LL << havebits) - 1) ;
                  break ;
               }
               bitsneeded += 8 ;
               k++ ;
               if (k >= 7)
                  error("! failure while decoding") ;
            }
         }
         mem[i] = v ;
      }
      if (bytecnt != bytectr)
         error("! error when unpacking bytes") ;
   }
   void writeblock(ull *mem, ull longcnt) {
      ull bytecnt = calcblocksize(mem, longcnt) ;
      uchar *buf = (uchar *)malloc(bytecnt) ;
      ioqueue.queuepackwork(mem, longcnt, buf, bytecnt) ;
   }
   void readblock(ull *mem, ull explongcnt, FILE *f) {
      unsigned int bytecnt, longcnt ;
      bytecnt = getc(f) ;
      bytecnt += getc(f) << 8 ;
      bytecnt += getc(f) << 16 ;
      bytecnt += getc(f) << 24 ;
      longcnt = getc(f) ;
      longcnt += getc(f) << 8 ;
      longcnt += getc(f) << 16 ;
      longcnt += getc(f) << 24 ;
      if (longcnt != explongcnt || bytecnt <= 0 || bytecnt > 32 * BLOCKSIZE)
         error("! I/O error while reading block") ;
      uchar *buf = (uchar *)malloc(bytecnt) ;
      if (fread(buf, 1, bytecnt, f) != bytecnt)
         error("! I/O error while reading block") ;
      ioqueue.queueunpackwork(mem, longcnt, buf, bytecnt) ;
   }
   void writept(const puzdef &pd) {
      // only write the table if at least 1 in 100 elements has a value
      if (justread || fillcnt * 100 < size)
         return ;
      ll longcnt = (size + 31) >> 5 ;
      if (longcnt % BLOCKSIZE != 0)
         return ; // too small
      // this *could* be calculated more efficiently, but the runtime is
      // dominated by scanning the array so we use simple code.
      // We use optimal huffman coding; for tables that fit on real
      // machines, this should probably never exceed a code length of
      // 56-bits, so we don't use the more complicated length-limited
      // coding.  We use 56-bits so we can use a 64-bit accumulator and
      // still shift things out in byte-sized chunks.
      ll bytecnts[256] ;
      for (int i=0; i<256; i++)
         bytecnts[i] = 0 ;
      cout << "Scanning memory for compression information " << flush ;
      for (ll i=0; i<longcnt; i++) {
         ull v = mem[i] ;
         for (int j=0; j<8; j++) {
            bytecnts[v & 255]++ ;
            v >>= 8 ;
         }
      }
      cout << "in " << duration() << endl << flush ;
      set<pair<ll, int> > codes ;
      vector<pair<int, int> > tree ; // binary tree
      vector<int> depths ; // max depths
      for (int i=0; i<256; i++)
         if (bytecnts[i])
            codes.insert(make_pair(bytecnts[i], i)) ;
      int nextcode = 256 ;
      int maxwidth = 0 ;
      ull bitcost = 0 ;
      while (codes.size() > 1) { // take out least two and insert sum
         auto a = *(codes.begin()) ;
         codes.erase(a) ;
         auto b = *(codes.begin()) ;
         codes.erase(b) ;
         tree.push_back(make_pair(a.second, b.second)) ;
         int dep = 1 ;
         if (a.second >= 256)
            dep = 1 + depths[a.second-256] ;
         if (b.second >= 256)
            dep = max(dep, 1 + depths[b.second-256]) ;
         maxwidth = max(maxwidth, dep) ;
         if (maxwidth > 56)
            error("! exceeded maxwidth in Huffman encoding; fix the code") ;
         depths.push_back(dep) ;
         codes.insert(make_pair(a.first+b.first, nextcode)) ;
         bitcost += a.first + b.first ;
         nextcode++ ;
      }
      cout << "Encoding; max width is " << maxwidth << " bitcost "
         << bitcost << " compression " << ((64.0 * longcnt) / bitcost)
         << " in " << duration() << endl ;
      codewidths[nextcode-1] = 0 ;
      codevals[nextcode-1] = 0 ;
      for (int i=0; i<256; i++) {
         codewidths[i] = 0 ;
         codevals[i] = 0 ;
      }
      int widthcounts[64] ;
      for (int i=0; i<64; i++)
         widthcounts[i] = 0 ;
      codewidths[nextcode-1] = 0 ;
      for (int i=nextcode-1; i>=256; i--) {
         int a = tree[i-256].first ;
         int b = tree[i-256].second ;
         codewidths[a] = codewidths[i] + 1 ;
         codewidths[b] = codewidths[i] + 1 ;
      }
      for (int i=0; i<256; i++)
         widthcounts[codewidths[i]]++ ;
      ull widthbases[64] ;
      ull at = 0 ;
      for (int i=63; i>0; i--) {
         if (widthcounts[i]) {
            widthbases[i] = at >> (maxwidth - i) ;
            at += ((ull)widthcounts[i]) << (maxwidth - i) ;
         }
      }
      if (at != (1ULL << maxwidth))
         error("! Bad calculation in codes") ;
      for (int i=0; i<256; i++)
         if (codewidths[i]) {
            codevals[i] = widthbases[codewidths[i]] ;
            widthbases[codewidths[i]]++ ;
         }
      string filename = makefilename(pd) ;
      cout << "Writing " << filename << " " << flush ;
      FILE *w = fopen(filename.c_str(), "wb") ;
      if (w == 0)
         error("! can't open filename") ;
      if (putc(SIGNATURE, w) < 0)
         error("! I/O error") ;
      fwrite(&pd.checksum, sizeof(pd.checksum), 1, w) ;
      fwrite(&size, sizeof(size), 1, w) ;
      fwrite(&hmask, sizeof(hmask), 1, w) ;
      fwrite(&popped, sizeof(popped), 1, w) ;
      fwrite(&totpop, sizeof(totpop), 1, w) ;
      fwrite(&fillcnt, sizeof(fillcnt), 1, w) ;
      fwrite(&totsize, sizeof(totsize), 1, w) ;
      fwrite(&baseval, sizeof(baseval), 1, w) ;
      fwrite(&hibase, sizeof(hibase), 1, w) ;
      fwrite(codewidths, sizeof(codewidths[0]), 256, w) ;
      if (longcnt % BLOCKSIZE != 0)
         error("Size must be a multiple of block size") ;
      ioqueue.init(this, w) ;
      for (ll i=0; i<longcnt; i += BLOCKSIZE)
         writeblock(mem+i, BLOCKSIZE) ;
      ioqueue.finishall() ;
      if (putc(SIGNATURE, w) < 0)
         error("! I/O error") ;
      fclose(w) ;
      cout << "written in " << duration() << endl << flush ;
   }
   int readpt(const puzdef &pd) {
      for (int i=0; i<256; i++) {
         codewidths[i] = 0 ;
         codevals[i] = 0 ;
      }
      string filename = makefilename(pd) ;
      FILE *r = fopen(filename.c_str(), "rb") ;
      if (r == 0)
         return 0 ;
      cout << "Reading " << filename << " " << flush ;
      if (getc(r) != SIGNATURE)
         return warn("! first byte not signature") ;
      ull checksum = 0 ;
      if (fread(&checksum, sizeof(checksum), 1, r) < 1)
         error("! I/O error reading pruning table") ;
      if (checksum != pd.checksum) {
         cout <<
 "Puzzle definition appears to have changed; recreating pruning table" << endl ;
         fclose(r) ;
         return 0 ;
      }
      ull temp = 0 ;
      if (fread(&temp, sizeof(temp), 1, r) != 1)
         error("! I/O error in reading pruning table") ;
      if (temp != size) {
         cout <<
 "Pruning table size is different; recreating pruning table" << endl ;
         fclose(r) ;
         return 0 ;
      }
      if (fread(&hmask, sizeof(hmask), 1, r) < 1 ||
          fread(&popped, sizeof(popped), 1, r) < 1 ||
          fread(&totpop, sizeof(totpop), 1, r) < 1 ||
          fread(&fillcnt, sizeof(fillcnt), 1, r) < 1 ||
          fread(&totsize, sizeof(totsize), 1, r) < 1 ||
          fread(&baseval, sizeof(baseval), 1, r) < 1 ||
          fread(&hibase, sizeof(hibase), 1, r) < 1)
         error("! I/O error reading pruning table") ;
      if (fread(codewidths, sizeof(codewidths[0]), 256, r) != 256) {
         warn("I/O error in reading pruning table") ;
         fclose(r) ;
         return 0 ;
      }
      int widthcounts[64] ;
      for (int i=0; i<64; i++)
         widthcounts[i] = 0 ;
      int maxwidth = 1 ;
      for (int i=0; i<256; i++) {
         if (codewidths[i] >= 56)
            error("! bad code widths in pruning table file") ;
         maxwidth = max(maxwidth, (int)codewidths[i]) ;
         widthcounts[codewidths[i]]++ ;
      }
      ull widthbases[64] ;
      ull at = 0 ;
      for (int i=63; i>0; i--) {
         if (widthcounts[i]) {
            widthbases[i] = at >> (maxwidth - i) ;
            at += ((ull)widthcounts[i]) << (maxwidth - i) ;
         }
      }
      if (at != (1ULL << maxwidth))
         error("! Bad codewidth sum in codes") ;
      for (int i=0; i<256; i++)
         if (codewidths[i]) {
            codevals[i] = widthbases[codewidths[i]] ;
            widthbases[codewidths[i]]++ ;
         }
      at = 0 ; // restore the widthbases
      int theight[7] ;
      for (int i=63; i>0; i--) {
         if (widthcounts[i]) {
            widthbases[i] = at >> (maxwidth - i) ;
            at += ((ull)widthcounts[i]) << (maxwidth - i) ;
         }
         if ((i & 7) == 1) {
            int t = maxwidth - i - 7 ;
            if (t < 0) {
               theight[i>>3] = (at << -t) ;
            } else {
               theight[i>>3] = (at + (1LL << t) - 1) >> t ;
            }
         }
      }
      for (int i=0; i<7; i++)
         if (theight[i]) {
            tabs[i] = (short *)malloc(theight[i] * sizeof(short)) ;
            memset(tabs[i], -1, theight[i] * sizeof(short)) ;
         }
      at = 0 ;
      int twidth = (maxwidth + 7) & -8 ;
      for (int i=63; i>0; i--) {
         if (widthcounts[i]) {
            for (int cp=0; cp<256; cp++)
               if (codewidths[cp] == i) {
                  int k = (i - 1) >> 3 ;
                  int incsh = twidth-8*k-8 ;
                  ull inc = 1LL << incsh ;
                  ull nextat = at + (1LL << (twidth - i)) ;
                  while (at < nextat) {
                     tabs[k][at>>incsh] = cp ;
                     at += inc ;
                  }
                  at = nextat ;
               }
         }
      }
      ll longcnt = (size + 31) >> 5 ;
      if (longcnt % BLOCKSIZE != 0)
         error("! when reading, expected multiple of longcnt") ;
      ioqueue.init(this) ;
      for (ll i=0; i<longcnt; i += BLOCKSIZE)
         readblock(mem+i, BLOCKSIZE, r) ;
      ioqueue.finishall() ;
      int tv = getc(r) ;
      if (tv != SIGNATURE)
         error("! I/O error reading final signature") ;
      fclose(r) ;
      cout << "read in " << duration() << endl << flush ;
      justread = 1 ;
      return 1 ;
   }
   ull size, hmask, popped, totpop ;
   ull lookupcnt ;
   ull fillcnt ;
   ull *mem ;
   int totsize ;
   int shardshift ;
   int baseval, hibase ; // 0 is less; 1 is this; 2 is this+1; 3 is >=this+2
   int wval, wbval ;
   uchar codewidths[512] ;
   ull codevals[512] ;
   short *tabs[7] ;
   char justread ;
} ;
void *unpackworker(void *o) {
   ioworkitem *wi = (ioworkitem *)o ;
   wi->pt->unpackblock(wi->mem, wi->longcnt, wi->buf, wi->bytecnt) ;
   free(wi->buf) ;
   return 0 ;
}
void *packworker(void *o) {
   ioworkitem *wi = (ioworkitem *)o ;
   wi->pt->packblock(wi->mem, wi->longcnt, wi->buf, wi->bytecnt) ;
   return 0 ;
}
ull fillworker::fillstart(const puzdef &pd, prunetable &pt, int w) {
   ull initmoves = workchunks[w] ;
   int nmoves = pd.moves.size() ;
   int sp = 0 ;
   int st = 0 ;
   int togo = d ;
   while (initmoves > 1) {
      int mv = initmoves % nmoves ;
      pd.mul(posns[sp], pd.moves[mv].pos, posns[sp+1]) ;
      if (!pd.legalstate(posns[sp+1]))
         return 0 ;
      st = canonnext[st][pd.moves[mv].cs] ;
      sp++ ;
      togo-- ;
      initmoves /= nmoves ;
   }
   ull r = filltable(pd, pt, togo, sp, st) ;
   for (int i=0; i<MEMSHARDS; i++)
      r += fillflush(pt, i) ;
   return r ;
}
ull fillworker::fillflush(const prunetable &pt, int shard) {
   ull r = 0 ;
   fillbuf &fb = fillbufs[shard] ;
   if (fb.nchunks > 0) {
      pthread_mutex_lock(&(memshards[shard].mutex)) ;
      for (int i=0; i<fb.nchunks; i++) {
         ull h = fb.chunks[i] ;
         if (((pt.mem[h>>5] >> (2*(h&31))) & 3) == 3) {
            pt.mem[h>>5] -= (3LL - pt.wval) << (2*(h&31)) ;
            if ((pt.mem[(h>>5)&-8] & 15) == 15)
               pt.mem[(h>>5)&-8] -= 15 - pt.wbval ;
            r++ ;
         }
      }
      pthread_mutex_unlock(&(memshards[shard].mutex)) ;
      fb.nchunks = 0 ;
   }
   return r ;
}
void fillworker::dowork(const puzdef &pd, prunetable &pt) {
   while (1) {
      int w = -1 ;
      get_global_lock() ;
      if (workat < (int)workchunks.size())
         w = workat++ ;
      release_global_lock() ;
      if (w < 0)
         return ;
      ull cnt = fillstart(pd, pt, w) ;
      get_global_lock() ;
      pt.popped += cnt ;
      release_global_lock() ;
   }
}
ull fillworker::filltable(const puzdef &pd, prunetable &pt, int togo,
                          int sp, int st) {
   ull r = 0 ;
   if (togo == 0) {
      ull h = fasthash(pd.totsize, posns[sp]) & pt.hmask ;
      int shard = (h >> pt.shardshift) ;
      fillbuf &fb = fillbufs[shard] ;
      fb.chunks[fb.nchunks++] = h ;
      if (fb.nchunks >= FILLCHUNKS)
         r += fillflush(pt, shard) ;
      return r ;
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
      r += filltable(pd, pt, togo-1, sp+1, ns[mv.cs]) ;
   }
   return r ;
}
ull solutionsfound = 0 ;
struct solveworker {
   vector<allocsetval> posns ;
   vector<int> movehist ;
   long long lookups ;
   int d ;
   char padding[256] ; // kill false sharing
   void init(const puzdef &pd, int d_, const setval &p) {
      // make the position table big to minimize false sharing.
      while (posns.size() <= 100 || (int)posns.size() <= d_+1) {
         posns.push_back(allocsetval(pd, pd.solved)) ;
         movehist.push_back(-1) ;
      }
      pd.assignpos(posns[0], p) ;
      lookups = 0 ;
      d = d_ ;
   }
   int solverecur(const puzdef &pd, prunetable &pt, int togo, int sp, int st) {
      lookups++ ;
      int v = pt.lookup(posns[sp]) ;
      if (v > togo + 1)
         return -1 ;
      if (v > togo)
         return 0 ;
      if (v == 0 && togo > 0 && pd.comparepos(posns[sp], pd.solved) == 0 &&
          noearlysolutions)
         return 0 ;
      if (togo == 0) {
         if (pd.comparepos(posns[sp], pd.solved) == 0) {
            int r = 1 ;
            get_global_lock() ;
            solutionsfound++ ;
            for (int i=0; i<d; i++)
               cout << " " << pd.moves[movehist[i]].name ;
            cout << endl << flush ;
            if (solutionsfound < solutionsneeded)
               r = 0 ;
            release_global_lock() ;
            return r ;
         } else
            return 0 ;
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
         movehist[sp] = m ;
         v = solverecur(pd, pt, togo-1, sp+1, ns[mv.cs]) ;
         if (v == 1)
            return 1 ;
         if (!quarter && v == -1) {
            // skip similar rotations
            while (m+1 < (int)pd.moves.size() && pd.moves[m].base == pd.moves[m+1].base)
               m++ ;
         }
      }
      return 0 ;
   }
   int solvestart(const puzdef &pd, prunetable &pt, int w) {
      ull initmoves = workchunks[w] ;
      int nmoves = pd.moves.size() ;
      int sp = 0 ;
      int st = 0 ;
      int togo = d ;
      while (initmoves > 1) {
         int mv = initmoves % nmoves ;
         pd.mul(posns[sp], pd.moves[mv].pos, posns[sp+1]) ;
         if (!pd.legalstate(posns[sp+1]))
            return -1 ;
         movehist[sp] = mv ;
         st = canonnext[st][pd.moves[mv].cs] ;
         sp++ ;
         togo-- ;
         initmoves /= nmoves ;
      }
      return solverecur(pd, pt, togo, sp, st) ;
   }
   void dowork(const puzdef &pd, prunetable &pt) {
      while (1) {
         int w = -1 ;
         int finished = 0 ;
         get_global_lock() ;
         finished = (solutionsfound >= solutionsneeded) ;
         if (workat < (int)workchunks.size())
            w = workat++ ;
         release_global_lock() ;
         if (finished || w < 0)
            return ;
         if (solvestart(pd, pt, w) == 1)
            return ;
      }
   }
} solveworkers[MAXTHREADS] ;
void *threadworker(void *o) {
   workerparam *wp = (workerparam *)o ;
   solveworkers[wp->tid].dowork(wp->pd, wp->pt) ;
   return 0 ;
}
int maxdepth = 1000000000 ;
int solve(const puzdef &pd, prunetable &pt, const setval p) {
   solutionsfound = solutionsneeded ;
   if (gs && !gs->resolve(p)) {
      if (!phase2)
         cout << "Ignoring unsolvable position." << endl ;
      return -1 ;
   }
   double starttime = walltime() ;
   ull totlookups = 0 ;
   int initd = pt.lookup(p) ;
   for (int d=initd; d <= maxdepth; d++) {
      if (d - initd > 3)
         makeworkchunks(pd, d) ;
      else
         makeworkchunks(pd, 0) ;
      int wthreads = setupthreads(pd, pt) ;
      for (int t=0; t<wthreads; t++)
         solveworkers[t].init(pd, d, p) ;
      solutionsfound = 0 ;
      for (int i=0; i<wthreads; i++)
         spawn_thread(i, threadworker, &(workerparams[i])) ;
      for (int i=0; i<wthreads; i++)
         join_thread(i) ;
      for (int i=0; i<wthreads; i++) {
         totlookups += solveworkers[i].lookups ;
         pt.addlookups(solveworkers[i].lookups) ;
      }
      if (solutionsfound >= solutionsneeded) {
         duration() ;
         double actualtime = start - starttime ;
         cout << "Found " << solutionsfound << " solution" <<
                 (solutionsfound != 1 ? "s" : "") << " at maximum depth " <<
                 d << " lookups " << totlookups << " in " << actualtime <<
                 " rate " << (totlookups/actualtime) << endl << flush ;
         return d ;
      }
      double dur = duration() ;
      if (verbose && dur > 1)
         cout << "Depth " << d << " finished in " << dur << endl << flush ;
      pt.checkextend(pd) ; // fill table up a bit more if needed
   }
   if (!phase2)
      cout << "No solution found in " << maxdepth << endl << flush ;
   return -1 ;
}
void timingtest(puzdef &pd) {
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   cout << "Timing moves." << endl << flush ;
   duration() ;
   int cnt = 100000000 ;
   for (int i=0; i<cnt; i += 2) {
      int rmv = myrand(pd.moves.size()) ;
      pd.mul(p1, pd.moves[rmv].pos, p2) ;
      rmv = myrand(pd.moves.size()) ;
      pd.mul(p2, pd.moves[rmv].pos, p1) ;
   }
   double tim = duration() ;
   cout << "Did " << cnt << " in " << tim << " rate " << cnt/tim/1e6 << endl << flush ;
   cout << "Timing moves plus hash." << endl << flush ;
   duration() ;
   cnt = 100000000 ;
   ull sum = 0 ;
   for (int i=0; i<cnt; i += 2) {
      int rmv = myrand(pd.moves.size()) ;
      pd.mul(p1, pd.moves[rmv].pos, p2) ;
      sum += fasthash(pd.totsize, p2) ;
      rmv = myrand(pd.moves.size()) ;
      pd.mul(p2, pd.moves[rmv].pos, p1) ;
      sum += fasthash(pd.totsize, p1) ;
   }
   tim = duration() ;
   cout << "Did " << cnt << " in " << tim << " rate " << cnt/tim/1e6 << " sum " << sum << endl << flush ;
   prunetable pt(pd, maxmem) ;
   cout << "Timing moves plus lookup." << endl << flush ;
   duration() ;
   cnt = 100000000 ;
   sum = 0 ;
   for (int i=0; i<cnt; i += 2) {
      int rmv = myrand(pd.moves.size()) ;
      pd.mul(p1, pd.moves[rmv].pos, p2) ;
      sum += pt.lookup(p2) ;
      rmv = myrand(pd.moves.size()) ;
      pd.mul(p2, pd.moves[rmv].pos, p1) ;
      sum += pt.lookup(p1) ;
   }
   tim = duration() ;
   cout << "Did " << cnt << " in " << tim << " rate " << cnt/tim/1e6 << " sum " << sum << endl << flush ;
   const int MAXLOOK = 128 ;
   ull tgo[MAXLOOK] ;
   for (int look=2; look<=MAXLOOK; look *= 2) {
      int mask = look - 1 ;
      for (int i=0; i<look; i++)
         tgo[i] = 0 ;
      cout << "Timing moves plus lookup piped " << look << endl << flush ;
      duration() ;
      cnt = 100000000 ;
      sum = 0 ;
      for (int i=0; i<cnt; i += 2) {
         int rmv = myrand(pd.moves.size()) ;
         pd.mul(p1, pd.moves[rmv].pos, p2) ;
         sum += pt.lookuph(tgo[i&mask]) ;
         tgo[i&mask] = fasthash(pd.totsize, p2) ;
         pt.prefetch(tgo[i&mask]) ;
         rmv = myrand(pd.moves.size()) ;
         pd.mul(p2, pd.moves[rmv].pos, p1) ;
         sum += pt.lookuph(tgo[1+(i&mask)]) ;
         tgo[1+(i&mask)] = fasthash(pd.totsize, p1) ;
         pt.prefetch(tgo[1+(i&mask)]) ;
      }
      tim = duration() ;
      cout << "Did " << cnt << " in " << tim << " rate " << cnt/tim/1e6 << " sum " << sum << endl << flush ;
   }
}
void solvetest(puzdef &pd) {
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.solved) ;
   prunetable pt(pd, maxmem) ;
   while (1) {
      solve(pd, pt, p1) ;
      while (1) {
         int rmv = myrand(pd.moves.size()) ;
         pd.mul(p1, pd.moves[rmv].pos, p2) ;
         if (pd.legalstate(p2)) {
            pd.assignpos(p1, p2) ;
            break ;
         }
      }
   }
}
void domove(const puzdef &pd, setval p, setval pos) {
   stacksetval pt(pd) ;
   pd.mul(p, pos, pt) ;
   pd.assignpos(p, pt) ;
}
void domove(puzdef &pd, setval p, int mv) {
   domove(pd, p, pd.moves[mv].pos) ;
}
setval findmove_generously(const puzdef &pd, const char *mvstring) {
   for (int i=0; i<(int)pd.moves.size(); i++)
      if (strcmp(mvstring, pd.moves[i].name) == 0)
         return pd.moves[i].pos ;
   for (int i=0; i<(int)pd.parsemoves.size(); i++)
      if (strcmp(mvstring, pd.parsemoves[i].name) == 0)
         return pd.parsemoves[i].pos ;
   error("! bad move name ", mvstring) ;
   return setval(0) ;
}
setval findmove_generously(const puzdef &pd, string s) {
   return findmove_generously(pd, s.c_str()) ;
}
int findmove(const puzdef &pd, const char *mvstring) {
   for (int i=0; i<(int)pd.moves.size(); i++)
      if (strcmp(mvstring, pd.moves[i].name) == 0)
         return i ;
   error("! bad move name ", mvstring) ;
   return -1 ;
}
int findmove(const puzdef &pd, string mvstring) {
   return findmove(pd, mvstring.c_str()) ;
}
void domove(puzdef &pd, setval p, string mvstring) {
   domove(pd, p, findmove(pd, mvstring)) ;
}
void solveit(const puzdef &pd, prunetable &pt, string scramblename, setval &p) {
   if (scramblename.size())
      cout << "Solving " << scramblename << endl << flush ;
   else
      cout << "Solving" << endl << flush ;
   solve(pd, pt, p) ;
}
vector<int> parsemovelist(const puzdef &pd, const char *scr) {
   vector<int> movelist ;
   string move ;
   for (const char *p=scr; *p; p++) {
      if (*p <= ' ' || *p == ',') {
         if (move.size()) {
            movelist.push_back(findmove(pd, move)) ;
            move.clear() ;
         }
      } else
         move.push_back(*p) ;
   }
   if (move.size())
      movelist.push_back(findmove(pd, move)) ;
   return movelist ;
}
vector<setval> parsemovelist_generously(const puzdef &pd, const char *scr) {
   vector<setval> movelist ;
   string move ;
   for (const char *p=scr; *p; p++) {
      if (*p <= ' ' || *p == ',') {
         if (move.size()) {
            movelist.push_back(findmove_generously(pd, move)) ;
            move.clear() ;
         }
      } else
         move.push_back(*p) ;
   }
   if (move.size())
      movelist.push_back(findmove_generously(pd, move)) ;
   return movelist ;
}
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
void processscrambles(FILE *f, puzdef &pd) {
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
void readfirstscramble(FILE *f, puzdef &pd, setval sv) {
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
/*
 *   Rewrite the movelist in the puzzle definition to restrict moves.
 *   This is a bit tricky.  The moves in the move list can be base
 *   moves (like U) or derived moves (like U2 or U').  In all cases
 *   we include only appropriate multiples.
 */
int goodmove(const moove &mv, int inc, int order) {
   if (inc == 0)
      return 0 ;
   if (order % inc != 0)
      error("! filtered move has to be simplest possible") ;
   // there's a faster number theory way to do this, but why.
   return (mv.twist % inc == 0) ;
}
void filtermovelist(puzdef &pd, const char *movelist) {
   vector<int> moves = parsemovelist(pd, movelist) ;
   vector<int> lowinc(pd.basemoves.size()) ;
   for (int i=0; i<(int)moves.size(); i++) {
      moove &mv = pd.moves[moves[i]] ;
      if (lowinc[mv.base])
         error("Move list restriction should only list a base move once.") ;
      lowinc[mv.base] = mv.twist ;
   }
   vector<moove> newbase ;
   map<int, int> moveremap ;
   vector<int> newbasemoveorders ;
   for (int i=0; i<(int)pd.basemoves.size(); i++)
      if (goodmove(pd.basemoves[i], lowinc[i], pd.basemoveorders[i])) {
         int newbasenum = newbase.size() ;
         moove newmv = pd.basemoves[i] ;
         newmv.base = newbasenum ;
         moveremap[i] = newbasenum ;
         newbase.push_back(newmv) ;
         newbasemoveorders.push_back(pd.basemoveorders[i] / lowinc[i]) ;
      } else {
      }
   vector<moove> newmvs ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      int obase = pd.moves[i].base ;
      if (goodmove(pd.moves[i], lowinc[obase], pd.basemoveorders[obase])) {
         moove newmv = pd.moves[i] ;
         int otwist = newmv.twist ;
         newmv.twist /= lowinc[pd.moves[i].base] ;
         if (otwist == lowinc[obase] && lowinc[obase] > 1) {
            int newbasenum = newbase.size() ;
            moveremap[obase] = newbasenum ;
            newmv.base = newbasenum ;
            newmv.cost /= lowinc[obase] ;
            newbase.push_back(newmv) ;
            newbasemoveorders.push_back(pd.basemoveorders[obase] / lowinc[obase]) ;
         }
         newmv.base = moveremap[obase] ;
         newmvs.push_back(newmv) ;
      }
   }
   // allow parsing to pick up old move positions
   pd.parsemoves = pd.moves ;
   pd.basemoveorders = newbasemoveorders ;
   pd.basemoves = newbase ;
   pd.moves = newmvs ;
   pd.addoptionssum(movelist) ;
}
vector<loosetype> uniqwork ;
set<vector<loosetype> > uniqseen ;
void uniqit(const puzdef &pd, setval p, const char *s) {
   uniqwork.resize(looseper) ;
   loosepack(pd, p, &uniqwork[0]) ;
   if (uniqseen.find(uniqwork) == uniqseen.end()) {
      uniqseen.insert(uniqwork) ;
      cout << s << endl << flush ;
   }
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
      cout << "   " ;
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
int bestsolve = 1000000 ;
int optmaxdepth = 0 ;
void dophase2(const puzdef &pd, setval scr, setval p1sol, prunetable &pt,
              const char *p1str) {
   stacksetval p2(pd) ;
   if (optmaxdepth == 0)
      optmaxdepth = maxdepth ;
   pd.mul(scr, p1sol, p2) ;
   maxdepth = min(optmaxdepth - globalinputmovecount,
                  bestsolve - globalinputmovecount - 1) ;
   int r = solve(pd, pt, p2) ;
   if (r >= 0) {
      cout << "Phase one was " << p1str << endl ;
      bestsolve = r + globalinputmovecount ;
      cout << "Found a solution totaling " << bestsolve << " moves." << endl ;
   }
}
int dogod, docanon, doalgo, dosolvetest, dotimingtest, douniq,
    dosolvelines, doorder, doshowmoves, doshowpositions, genrand,
    checksolvable, doss ;
const char *scramblealgo = 0 ;
const char *legalmovelist = 0 ;
void calcsym(const puzdef &pd, int iat, int nmoves, vector<char> &used,
             vector<int> &mapped, vector<int> &basemovemapped,
             const setval &ip1, const setval &ip2, vector<vector<int>> &cc,
             int nmul) {
// cout << "In calcsym at " << at << endl ;
   if (iat == nmoves) {
      cout << "SYM" ;
      for (int i=0; i<nmoves; i++)
         cout << " " << pd.moves[i].name << "-" << pd.moves[mapped[i]].name  ;
      cout << endl ;
      return ;
   }
   int at = iat * nmul % nmoves ;
   stacksetval p1(pd), p2(pd), p3(pd), p4(pd) ;
   for (int i=0; i<nmoves; i++) {
      if (used[i])
         continue ;
      if (basemovemapped[pd.moves[at].base] >= 0 &&
          basemovemapped[pd.moves[at].base] != pd.moves[i].base)
         continue ;
      if (cc[at] != cc[i])
         continue ;
      pd.assignpos(p1, ip1) ;
      pd.assignpos(p2, ip2) ;
      pd.mul(p1, pd.moves[at].pos, p3) ;
      pd.mul(p2, pd.moves[i].pos, p4) ;
      if (pd.cyccnts(p3) != pd.cyccnts(p4))
         continue ;
      if (iat > 0) {
         int r = (int)(iat*drand48()) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
         r = (int)(iat*drand48()) ;
         r = r * nmul % nmoves ;
         pd.mul(p1, pd.moves[r].pos, p3) ;
         pd.mul(p2, pd.moves[mapped[r]].pos, p4) ;
         if (pd.cyccnts(p3) != pd.cyccnts(p4))
            continue ;
         r = (int)(iat*drand48()) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
         r = (int)(iat*drand48()) ;
         r = r * nmul % nmoves ;
         pd.mul(p1, pd.moves[r].pos, p3) ;
         pd.mul(p2, pd.moves[mapped[r]].pos, p4) ;
         if (pd.cyccnts(p3) != pd.cyccnts(p4))
            continue ;
         r = (int)(iat*drand48()) ;
         r = r * nmul % nmoves ;
         pd.mul(p3, pd.moves[r].pos, p1) ;
         pd.mul(p4, pd.moves[mapped[r]].pos, p2) ;
         if (pd.cyccnts(p1) != pd.cyccnts(p2))
            continue ;
      }
      int obmm = basemovemapped[pd.moves[at].base] ;
      basemovemapped[pd.moves[at].base] = pd.moves[i].base ;
      mapped[at] = i ;
      used[i] = 1 ;
      calcsym(pd, iat+1, nmoves, used, mapped, basemovemapped, p1, p2, cc, nmul) ;
      used[i] = 0 ;
      basemovemapped[pd.moves[at].base] = obmm ;
   }
}
int isprime(int p) {
   if (p < 2)
      return 0 ;
   if (p < 4)
      return 1 ;
   if ((p & 1) == 0)
      return 0 ;
   for (int j=3; ; j+=2) {
      if (p % j == 0)
         return 0 ;
      if (j * j > p)
         return 1 ;
   }
}
void gensymm(const puzdef &pd) {
   int nmoves = pd.moves.size() ;
   int nmul = (int)(0.618 * nmoves) ;
   if (nmul == 0)
      nmul = 1 ;
   while (gcd(nmul, nmoves) != 1)
      nmul++ ;
   if (nmul >= nmoves)
      nmul = 1 ;
   vector<char> used(nmoves) ;
   vector<int> mapped(nmoves) ;
   vector<int> basemovemapped(pd.basemoves.size()) ;
   for (int i=0; i<(int)basemovemapped.size(); i++)
      basemovemapped[i] = -1 ;
   stacksetval p1(pd), p2(pd) ;
   pd.assignpos(p1, pd.id) ;
   pd.assignpos(p2, pd.id) ;
   vector<vector<int>> cc ;
   for (int i=0; i<nmoves; i++)
      cc.push_back(pd.cyccnts(pd.moves[i].pos)) ;
   calcsym(pd, 0, nmoves, used, mapped, basemovemapped, p1, p2, cc, nmul) ;
}
int main(int argc, const char **argv) {
   int seed = 0 ;
   int forcearray = 0 ;
   duration() ;
   init_mutex() ;
   for (int i=0; i<MEMSHARDS; i++)
      pthread_mutex_init(&(memshards[i].mutex), NULL) ;
   cout << "# This is twsearch 0.1 (C) 2018 Tomas Rokicki." << endl ;
   cout << "#" ;
   for (int i=0; i<argc; i++)
      cout << " " << argv[i] ;
   cout << endl << flush ;
   fact.push_back(0) ;
   for (int i=1; i<=20; i++)
      fact.push_back(fact[i-1]*i) ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      if (argv[0][1] == '-') {
         if (strcmp(argv[0], "--moves") == 0) {
            legalmovelist = argv[1] ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--showmoves") == 0) {
            doshowmoves++ ;
         } else if (strcmp(argv[0], "--showpositions") == 0) {
            doshowpositions++ ;
         } else if (strcmp(argv[0], "--newcanon") == 0) {
            ccount = atol(argv[1]) ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--nocorners") == 0) {
            nocorners++ ;
         } else if (strcmp(argv[0], "--nocenters") == 0) {
            nocenters++ ;
         } else if (strcmp(argv[0], "--noorientation") == 0) {
            ignoreori = 1 ;
         } else if (strcmp(argv[0], "--noearlysolutions") == 0) {
            noearlysolutions = 1 ;
         } else if (strcmp(argv[0], "--checkbeforesolve") == 0) {
            checkbeforesolve = 1 ;
         } else if (strcmp(argv[0], "--orientationgroup") == 0) {
            origroup = atol(argv[1]) ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--noedges") == 0) {
            noedges++ ;
         } else if (strcmp(argv[0], "--scramblealg") == 0) {
            scramblealgo = argv[1] ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--schreiersims") == 0) {
            doss = 1 ;
         } else {
            error("! Argument not understood ", argv[0]) ;
         }
      } else {
         switch (argv[0][1]) {
case 'q':
            quarter++ ;
            break ;
case 'v':
            verbose++ ;
            if (argv[0][2] != 0)
               verbose = argv[0][2] - '0' ;
            break ;
case 'm':
case 'd':
            maxdepth = atol(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'r':
            genrand = 1 ;
            break ;
case 'R':
            seed = atol(argv[1]) ;
            argc-- ;
            argv++ ;
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
case 'c':
            solutionsneeded = atoll(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'g':
            dogod++ ;
            break ;
case 'o':
            doorder++ ;
            break ;
case 'u':
            douniq++ ;
            break ;
case 's':
            dosolvelines++ ;
            break ;
case 'C':
            docanon++ ;
            break ;
case 'F':
            forcearray++ ;
            break ;
case 'a':
            antipodecount = atoll(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'A':
            if (argv[0][2] == 0 || argv[0][2] == '1')
               doalgo = 1 ;
            else if (argv[0][2] == '2')
               doalgo = 2 ;
            else if (argv[0][2] == '3')
               doalgo = 3 ;
            else
               error("! bad -A value") ;
            break ;
case 'T':
            dotimingtest++ ;
            break ;
case 'S':
            dosolvetest++ ;
            break ;
case 't':
            numthreads = atol(argv[1]) ;
            if (numthreads > MAXTHREADS)
               error("Numthreads cannot be more than ", to_string(MAXTHREADS)) ;
            argc-- ;
            argv++ ;
            break ;
case '2':
            phase2 = 1 ;
            break ;
default:
            error("! did not argument ", argv[0]) ;
         }
      }
   }
   if (seed)
      srand48(seed) ;
   else
      srand48(time(0)) ;
   if (argc <= 1)
      error("! please provide a twsearch file name on the command line") ;
   FILE *f = fopen(argv[1], "r") ;
   if (f == 0)
      error("! could not open file ", argv[1]) ;
   int sawdot = 0 ;
   for (int i=0; argv[1][i]; i++) {
      if (argv[1][i] == '.')
         sawdot = 1 ;
      else if (argv[1][i] == '/' || argv[1][i] == '\\') {
         sawdot = 0 ;
         inputbasename.clear() ;
      } else if (!sawdot)
         inputbasename.push_back(argv[1][i]) ;
   }
   puzdef pd = readdef(f) ;
   addmovepowers(pd) ;
   if (legalmovelist)
      filtermovelist(pd, legalmovelist) ;
   if (nocorners)
      pd.addoptionssum("nocorners") ;
   if (nocenters)
      pd.addoptionssum("nocenters") ;
   if (noedges)
      pd.addoptionssum("noedges") ;
   if (doss || checkbeforesolve)
      gs = new generatingset(pd) ;
   if (genrand) {
      showrandompos(pd) ;
      return 0 ;
   }
   calculatesizes(pd) ;
   calclooseper(pd) ;
   if (ccount == 0)
      makecanonstates(pd) ;
   else
      makecanonstates2(pd) ;
   cout << "Calculated canonical states in " << duration() << endl << flush ;
   showcanon(pd, docanon) ;
//   gensymm(pd) ;
   if (dogod) {
      int statesfit2 = pd.logstates <= 50 && ((ll)(pd.llstates >> 2)) <= maxmem ;
      int statesfitsa = forcearray ||
          (pd.logstates <= 50 &&
             ((ll)(pd.llstates * sizeof(loosetype) * looseper) <= maxmem)) ;
      if (statesfit2 && pd.canpackdense()) {
         cout << "Using twobit arrays." << endl ;
         dotwobitgod2(pd) ;
      } else if (statesfitsa) {
         cout << "Using sorting bfs and arrays." << endl ;
         doarraygod(pd) ;
      } else {
         cout << "Using canonical sequences and arrays." << endl ;
         doarraygod2(pd) ;
      }
   }
   if (doalgo == 1)
      findalgos(pd) ;
   if (doalgo == 2)
      findalgos2(pd) ;
   if (doalgo == 3)
      findalgos3(pd) ;
   if (dosolvetest)
      solvetest(pd) ;
   if (dotimingtest)
      timingtest(pd) ;
   if (scramblealgo)
      solvecmdline(pd, scramblealgo) ;
   if (douniq)
      processlines(pd, uniqit) ;
   if (doorder)
      processlines2(pd, orderit) ;
   if (doshowmoves)
      processlines2(pd, emitmove) ;
   if (doshowpositions)
      processlines(pd, emitposition) ;
   if (dosolvelines) {
      prunetable pt(pd, maxmem) ;
      string emptys ;
      processlines(pd, [&](const puzdef &pd, setval p, const char *) {
                          solveit(pd, pt, emptys, p) ;
                       }) ;
   }
   if (phase2) {
      if (argc <= 2)
         error("! need a scramble file for phase 2") ;
      f = fopen(argv[2], "r") ;
      if (f == 0)
         error("! could not open scramble file ", argv[2]) ;
      stacksetval scr(pd) ;
      readfirstscramble(f, pd, scr) ;
      prunetable pt(pd, maxmem) ;
      processlines2(pd, [&](const puzdef &pd, setval p1sol, const char *p1str) {
                               dophase2(pd, scr, p1sol, pt, p1str); }) ;
      fclose(f) ;
   } else if (argc > 2) {
      f = fopen(argv[2], "r") ;
      if (f == 0)
         error("! could not open scramble file ", argv[2]) ;
      processscrambles(f, pd) ;
   }
}
