#ifndef CANON_H
#include <vector>
#include "puzdef.h"
using namespace std ;
void makecanonstates(puzdef &pd) ;
extern vector<ull> canonmask ;
extern vector<vector<int> > canonnext ;
extern vector<ull> canonseqcnt ;
extern vector<ull> canontotcnt ;
extern int ccount ;
void makecanonstates2(puzdef &pd) ;
void showcanon(const puzdef &pd, int show) ;
#define CANON_H
#endif
