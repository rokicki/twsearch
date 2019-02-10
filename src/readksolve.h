#ifndef READKSOLVE_H
#include <cstdio>
#include "puzdef.h"
vector<string> getline(FILE *f, ull &checksum) ;
void expect(const vector<string> &toks, int cnt) ;
int getnumber(int minval, const string &s) ;
setval readposition(puzdef &pz, char typ, FILE *f, ull &checksum) ;
puzdef readdef(FILE *f) ;
void addmovepowers(puzdef &pd) ;
extern int nocorners, nocenters, noedges, ignoreori ;
#define READKSOLVE_H
#endif
