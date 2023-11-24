#ifndef TWSEARCH_H
#define TWSEARCH_H
#include "generatingset.h"
#include "prunetable.h"
#include "puzdef.h"
#include <iostream>
using argvtype = const char **;
extern int checkbeforesolve;
extern generatingset *gs;
extern void processargs(int &argc, argvtype &argv, int includecmds);
extern puzdef makepuzdef(istream *f);
extern puzdef makepuzdef(string s);
extern void reseteverything();
void processscrambles(istream *f, puzdef &pd, prunetable &pt,
                      generatingset *gs);
int main_search(const char *def_file, const char *scramble_file);
#endif
