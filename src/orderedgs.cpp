#include "orderedgs.h"
#include <algorithm>
#include <iostream>
bool orderedgs::resolve(const setval p_) {
   stacksetval p(pd), t(pd) ;
   pd.assignpos(p, p_) ;
   for (int i=(int)oset.size()-1; i>=0; i--) {
      int sdi = oset[i] ;
      const setdef &sd = pd.setdefs[sdi] ;
      int n = sd.size ;
      int j = ooff[i] ;
      if (p.dat[sd.off+j] != j || p.dat[sd.off+n+j] != 0) {
         int v = sd.omod * p.dat[sd.off+j] + p.dat[sd.off+n+j] ;
         if (!sgs[i][v].dat)
            return 0 ;
         pd.mul(sgsi[i][v], p, t) ;
         swap(p.dat, t.dat) ;
         if (p.dat[sd.off+j] != j || p.dat[sd.off+n+j] != 0)
            error("! misresolve") ;
      }
   }
   return 1 ;
}
void orderedgs::knutha(int k, const setval &p) {
   tk[k].push_back(allocsetval(pd, p)) ;
   stacksetval p2(pd) ;
   for (int i=0; i<(int)sgs[k].size(); i++)
      if (sgs[k][i].dat) {
         pd.mul(p, sgs[k][i], p2) ;
         knuthb(k, p2) ;
      }
}
void orderedgs::knuthb(int k, const setval &p) {
   int sdi = oset[k] ;
   const setdef &sd = pd.setdefs[sdi] ;
   int n = sd.size ;
   int o = ooff[k] ;
   int j = p.dat[sd.off+o] * sd.omod + p.dat[sd.off+n+o] ;
   stacksetval p2(pd) ;
   if (!sgs[k][j].dat) {
      sgs[k][j] = allocsetval(pd, p) ;
      sgsi[k][j] = allocsetval(pd, p) ;
      pd.inv(sgs[k][j], sgsi[k][j]) ;
      for (int i=0; i<(int)tk[k].size(); i++) {
         pd.mul(tk[k][i], p, p2) ;
         knuthb(k, p2) ;
      }
      return ;
   }
   pd.mul(sgsi[k][j], p, p2) ;
   if (p2.dat[sd.off+o] != o || p2.dat[sd.off+n+o] != 0) {
      error("! misresolve in knuthb") ;
   }
   if (!resolve(p2))
      knutha(k-1, p2) ;
}
vector<int> orderedgs::getsizes() {
   vector<int> r ;
   for (int j=sgs.size()-1; j>=0; j--) {
      int cnt = 0 ;
      for (int k=0; k<(int)sgs[j].size(); k++)
         if (sgs[j][k].dat)
            cnt++ ;
      r.push_back(cnt) ;
   }
   return r ;
}
orderedgs::orderedgs(const puzdef &pd_, const vector<int> &norder) : pd(pd_), e(pd.id) {
   vector<int> order = norder ;
   if ((int)order.size() * 2 != pd.totsize) {
      vector<int> sorted = {-1} ;
      for (auto v : order)
         sorted.push_back(v) ;
      sorted.push_back(pd.totsize>>1) ;
      sort(sorted.begin(), sorted.end()) ;
      for (int i=0; i+1<(int)sorted.size(); i++)
         for (int j=sorted[i]+1; j<sorted[i+1]; j++)
            order.push_back(j) ;
   }
   reverse(order.begin(), order.end()) ;
   oset.resize(order.size()) ;
   ooff.resize(order.size()) ;
   for (int i=0; i<(int)order.size(); i++) {
      int roff = order[i] ;
      int sdi = -1 ;
      for (int j=0; j<(int)pd.setdefs.size(); j++) {
         const setdef &sd = pd.setdefs[j] ;
         if (roff < sd.size) {
            sdi = j ;
            break ;
         }
         roff -= sd.size ;
      }
      if (sdi < 0)
         error("! couldn't match cubie order index to cubie") ;
      const setdef &sd = pd.setdefs[sdi] ;
      oset[i] = sdi ;
      ooff[i] = roff ;
      int sz = sd.size * sd.omod ;
      sgs.push_back(vector<setval>(sz)) ;
      sgsi.push_back(vector<setval>(sz)) ;
      tk.push_back(vector<setval>(0)) ;
      int at = sgs.size() - 1 ;
      sgs[at][roff*sd.omod] = e ;
      sgsi[at][roff*sd.omod] = e ;
   }
   int oldprec = cout.precision() ;
   cout.precision(20) ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      if (resolve(pd.moves[i].pos))
         continue ;
      knutha(order.size()-1, pd.moves[i].pos) ;
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
