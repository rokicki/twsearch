#ifndef CMDLINEOPS_H
#include <cstdio>
#include <set>
#include <functional>
#include "puzdef.h"
#include "prunetable.h"
#include "generatingset.h"
/*
 *   The twsearch program also includes a number of utility operations,
 *   such as uniquifying a set of positions.  These routines support
 *   streaming a sequence of positions through various operations.
 */
void solvecmdline(puzdef &pd, const char *scr, generatingset *gs) ;
void processscrambles(istream *f, puzdef &pd, generatingset *gs) ;
void processscrambles(istream *f, puzdef &pd, prunetable &pt, generatingset *gs) ;
void readfirstscramble(istream *f, puzdef &pd, setval sv) ;
extern vector<loosetype> uniqwork ;
extern set<vector<loosetype> > uniqseen ;
void uniqit(const puzdef &pd, setval p, const char *s) ;
void invertit(const puzdef &pd, vector<int> &v, const char *s) ;
void cancelit(const puzdef &pd, vector<int> &v, const char *s) ;
void mergeit(const puzdef &pd, vector<int> &v, const char *s) ;
void symsit(const puzdef &pd, setval p, const char *s) ;
void orderit(const puzdef &pd, setval p, const char *s) ;
void emitmove(const puzdef &pd, setval p, const char *s) ;
void emitposition(const puzdef &pd, setval p, const char *s) ;
void showrandompos(const puzdef &pd) ;
extern int globalinputmovecount ;
void processlines(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
void processlines2(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
void processlines3(const puzdef &pd, function<void(const puzdef &, vector<int> &v, const char *)> f) ;
extern ll proclim ;
extern int compact ;
#define CMDLINEOPS_H
#endif
