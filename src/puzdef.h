#ifndef PUZDEF_H
#include <vector>
#include <math.h>
#include "util.h"
using namespace std ;
/*
 *   This is the core code, where we store a puzzle definition and
 *   the values for a puzzle.  The puzzle definition is a sequence
 *   of sets.  Each set has a permutation and orientation component.
 *   Values are stored as a sequence of uchars; first comes the
 *   permutation component and then the orientation component.
 *
 *   Right now the code is simple and general; it is likely we can
 *   gain a fair bit by specializing specific cases.
 */
extern double dllstates ;
extern uchar *gmoda[256] ;
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
typedef vector<setdef> setdefs_t ;
struct setval {
   setval() : dat(0) {}
   setval(uchar *dat_) : dat(dat_) {}
   uchar *dat ;
} ;
struct illegal_t {
   int pos ;
   ull mask ;
} ;
struct moove {
   moove() : name(0), pos(0), cost(1) {}
   const char *name ;
   setval pos ;
   int cost, base, twist, cs ;
} ;
extern int origroup ;
struct puzdef {
   puzdef() : name(0), setdefs(), solved(0), totsize(0), id(0),
              logstates(0), llstates(0), checksum(0), haveillegal(0) {}
   const char *name ;
   setdefs_t setdefs ;
   setval solved ;
   vector<moove> basemoves, moves, parsemoves, rotations, rotgroup ;
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
   int numwrong(const setval a, const setval b, ull mask=-1) const ;
   int permwrong(const setval a, const setval b, ull mask=-1) const ;
   vector<int> cyccnts(const setval a, ull sets=-1) const ;
   static ll order(const vector<int> cc) ;
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
   // does a multiplication and a comparison at the same time.
   // c must be initialized already.
   int mulcmp(const setval a, const setval b, setval c) const {
      const uchar *ap = a.dat ;
      const uchar *bp = b.dat ;
      uchar *cp = c.dat ;
      int r = 0 ;
      for (int i=0; i<(int)setdefs.size(); i++) {
         const setdef &sd = setdefs[i] ;
         int n = sd.size ;
         for (int j=0; j<n; j++) {
            int nv = ap[bp[j]] ;
            if (r > 0)
               cp[j] = nv ;
            else if (nv > cp[j])
               return 1 ;
            else if (nv < cp[j]) {
               r = 1 ;
               cp[j] = nv ;
            }
         }
         ap += n ;
         bp += n ;
         cp += n ;
         if (sd.omod > 1) {
            uchar *moda = gmoda[sd.omod] ;
            for (int j=0; j<n; j++) {
               int nv = moda[ap[bp[j-n]]+bp[j]] ;
               if (r > 0)
                  cp[j] = nv ;
               else if (nv > cp[j])
                  return 1 ;
               else if (nv < cp[j]) {
                  r = 1 ;
                  cp[j] = nv ;
               }
            }
         }
         ap += n ;
         bp += n ;
         cp += n ;
      }
      return -r ;
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
   void addillegal(const char *setname, int pos, int val) ;
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
extern vector<allocsetval> posns ;
extern vector<int> movehist ;
void calculatesizes(puzdef &pd) ;
void domove(const puzdef &pd, setval p, setval pos) ;
void domove(puzdef &pd, setval p, int mv) ;
#define PUZDEF_H
#endif
