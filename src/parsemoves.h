#ifndef PARSEMOVES_H
#include "puzdef.h"
#include "prunetable.h"
#include "generatingset.h"
/*
 *   Frequently we need to parse a move string.  These moves may come
 *   from the ksolve file directly, or may be constructed moves based
 *   on repetitions; in addition, sometimes when using move filtering
 *   we may still want to be able to describe a position using moves
 *   not in the filter string.  These routines help us manage this.
 */
setval findmove_generously(const puzdef &pd, const char *mvstring) ;
setval findmove_generously(const puzdef &pd, string s) ;
int findmove(const puzdef &pd, const char *mvstring) ;
int findmove(const puzdef &pd, string mvstring) ;
void domove(puzdef &pd, setval p, string mvstring) ;
void solveit(const puzdef &pd, prunetable &pt, string scramblename, setval &p, 
             generatingset *gs=0) ;
vector<int> parsemovelist(const puzdef &pd, const char *scr) ;
vector<setval> parsemovelist_generously(const puzdef &pd, const char *scr) ;
int isrotation(const char *s) ;
#define PARSEMOVES_H
#endif
