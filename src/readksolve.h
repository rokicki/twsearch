#ifndef READKSOLVE_H
#include <cstdio>
#include <set>
#include "puzdef.h"
/*
 *   This code manages parsing a tws file (an extension of the ksolve
 *   format).
 */
vector<string> getline(istream *f, ull &checksum) ;
void expect(const vector<string> &toks, int cnt) ;
int getnumber(int minval, const string &s) ;
setval readposition(puzdef &pz, char typ, istream *f, ull &checksum) ;
puzdef readdef(istream *f) ;
void addmovepowers(puzdef &pd) ;
extern int nocorners, nocenters, noedges, ignoreori, distinguishall ;
extern set<string> omitsets ;
#define READKSOLVE_H
#endif
