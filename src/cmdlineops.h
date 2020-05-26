#ifndef CMDLINEOPS_H
#include <cstdio>
#include <set>
#include <functional>
#include "puzdef.h"
#include "prunetable.h"
/*
 *   The twsearch program also includes a number of utility operations,
 *   such as uniquifying a set of positions.  These routines support
 *   streaming a sequence of positions through various operations.
 */
void solvecmdline(puzdef &pd, const char *scr) ;
void processscrambles(istream *f, puzdef &pd) ;
void processscrambles(istream *f, puzdef &pd, prunetable &pt) ;
void readfirstscramble(istream *f, puzdef &pd, setval sv) ;
extern vector<loosetype> uniqwork ;
extern set<vector<loosetype> > uniqseen ;
void uniqit(const puzdef &pd, setval p, const char *s) ;
void symsit(const puzdef &pd, setval p, const char *s) ;
void orderit(const puzdef &pd, setval p, const char *s) ;
void emitmove(const puzdef &pd, setval p, const char *s) ;
void emitposition(const puzdef &pd, setval p, const char *s) ;
void showrandompos(const puzdef &pd) ;
extern int globalinputmovecount ;
void processlines(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
void processlines2(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
extern ll proclim ;
#define CMDLINEOPS_H
#endif
