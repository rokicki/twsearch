#ifndef ANTIPODE_H
#include "puzdef.h"
extern ll antipodecount ;
void showantipodes(const puzdef &pd, loosetype *beg, loosetype *end) ;
void resetantipodes() ;
void showantipodesloose(const puzdef &pd) ;
void showantipodesdense(const puzdef &pd, int ordered) ;
void stashantipodesloose(loosetype *beg, loosetype *end) ;
void stashantipodedense(ull val) ;
#define ANTIPODE_H
#endif
