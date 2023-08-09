#ifndef ORDEREDGS_H
#include <vector>
#include "puzdef.h"
using namespace std ;
struct orderedgs {
   orderedgs(const puzdef &pd, const vector<int> &order) ;
   const puzdef &pd ;
   allocsetval e ;
   vector<vector<allocsetval>> sgs, sgsi, tk ;
   vector<int> oset, ooff ;
   bool resolve(const setval p_) ;
   void knutha(int k, const setval &p) ;
   void knuthb(int k, const setval &p) ;
   vector<int> getsizes() ;
   int inputlength ;
} ;
void runorderedgs(const puzdef &pd) ;
#define ORDEREDGS_H
#endif
