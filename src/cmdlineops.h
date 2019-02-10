#ifndef CMDLINEOPS_H
#include <cstdio>
#include <set>
#include "puzdef.h"
void solvecmdline(puzdef &pd, const char *scr) ;
void processscrambles(FILE *f, puzdef &pd) ;
void readfirstscramble(FILE *f, puzdef &pd, setval sv) ;
extern vector<loosetype> uniqwork ;
extern set<vector<loosetype> > uniqseen ;
void uniqit(const puzdef &pd, setval p, const char *s) ;
void orderit(const puzdef &pd, setval p, const char *s) ;
void emitmove(const puzdef &pd, setval p, const char *s) ;
void emitposition(const puzdef &pd, setval p, const char *s) ;
void showrandompos(const puzdef &pd) ;
extern int globalinputmovecount ;
void processlines(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
void processlines2(const puzdef &pd, function<void(const puzdef &, setval, const char *)> f) ;
#define CMDLINEOPS_H
#endif
