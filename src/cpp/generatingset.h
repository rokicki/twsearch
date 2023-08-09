#ifndef GENERATINGSET_H
#include <vector>
#include "puzdef.h"
using namespace std ;
struct generatingset {
   generatingset(const puzdef &pd) ;
   const puzdef &pd ;
   allocsetval e ;
   vector<vector<allocsetval>> sgs, sgsi, tk ;
   bool resolve(const setval p_) ;
   void knutha(int k1, int k2, const setval &p) ;
   void knuthb(int k1, int k2, const setval &p) ;
} ;
#define GENERATINGSET_H
#endif
