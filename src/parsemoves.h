#ifndef PARSEMOVES_H
#include "puzdef.h"
#include "prunetable.h"
#include "generatingset.h"
setval findmove_generously(const puzdef &pd, const char *mvstring) ;
setval findmove_generously(const puzdef &pd, string s) ;
int findmove(const puzdef &pd, const char *mvstring) ;
int findmove(const puzdef &pd, string mvstring) ;
void domove(puzdef &pd, setval p, string mvstring) ;
void solveit(const puzdef &pd, prunetable &pt, string scramblename, setval &p, 
             generatingset *gs=0) ;
vector<int> parsemovelist(const puzdef &pd, const char *scr) ;
vector<setval> parsemovelist_generously(const puzdef &pd, const char *scr) ;
#define PARSEMOVES_H
#endif
