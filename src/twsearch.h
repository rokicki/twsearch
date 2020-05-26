#ifndef TWSEARCH_H
#define TWSEARCH_H
#include <iostream>
#include "puzdef.h"
using argvtype = const char ** ;
extern void processargs(int &argc, argvtype &argv) ;
extern puzdef makepuzdef(istream *f) ;
extern puzdef makepuzdef(string s) ;
#endif
