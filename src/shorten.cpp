#include <iostream>
#include <algorithm>
#include <unordered_map>
#include "shorten.h"
#include "util.h"
#include "index.h"
#include "solve.h"
vector<loosetype> shenc ;
vector<int> srcsol ;
int solseen ;
int shortencb(setval &, const vector<int> &moves, int d, int) {
   get_global_lock() ;
   srcsol.resize(d) ;
   for (int i=0; i<d; i++)
      srcsol[i] = moves[i] ;
   solseen = 1 ;
   release_global_lock() ;
   return 1 ;
} ;
int shortencbf(int) {
   return 0 ;
}
vector<int> shorten(const puzdef &pd, const vector<int> &orig) {
   if (!pd.invertible())
      error("! can only shorten invertible positions") ;
   shenc.resize(looseiper) ;
   prunetable pt(pd, maxmem) ;
   setsolvecallback(shortencb, shortencbf) ;
   vector<int> seq = orig ;
   stacksetval pos(pd) ;
   unordered_map<vector<loosetype>, pair<int, vector<int>>,
                 hashvector<loosetype>> fini ;
   {
again:
      for (int md=1; md<(int)seq.size(); md++) {
         for (int len=seq.size(); len>md; len--) {
 cout << "Working with depth " << md << " length " << len << endl ;
            maxdepth = md ;
            for (int i=0; i+len<=(int)seq.size(); i++) {
               pd.assignpos(pos, pd.id) ;
               for (int j=i; j<i+len; j++)
                  domove(pd, pos, seq[j]) ;
               loosepack(pd, pos, shenc.data(), 1) ;
               auto it = fini.find(shenc) ;
               if (it == fini.end() || it->second.first < md) {
                  solseen = 0 ;
                  solve(pd, pt, pos, 0) ;
                  if (solseen) {
                     fini[shenc] = {10000, srcsol} ;
                  } else {
                     srcsol.resize(len) ;
                     for (int j=i; j<i+len; j++)
                        srcsol[j-i] = seq[j] ;
                     fini[shenc] = {md, srcsol} ;
                  }
                  it = fini.find(shenc) ;
               }
               vector<int> &sol = it->second.second ;
               if ((int)sol.size() < len) {
 cout << "Improving sequence from " << len << " to " << sol.size() << endl ;
                  for (int j=0; j<(int)sol.size(); j++) {
 cout << "Setting index " << i+sol.size()-1-j << endl ;
                     seq[i+sol.size()-1-j] = pd.invmove(sol[j]) ;
                  }
                  seq.erase(seq.begin()+i+sol.size(), seq.begin()+i+len) ;
 cout << "Current length is " << seq.size() << endl ;
 for (int j=0; j<(int)seq.size(); j++) cout << " " << pd.moves[seq[j]].name ; cout << endl ;
                  goto again ;
               }
            }
         }
      }
   }
   return seq ;
}
