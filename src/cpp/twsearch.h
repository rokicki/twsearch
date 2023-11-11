#ifndef TWSEARCH_H
#define TWSEARCH_H
#include "puzdef.h"
#include "prunetable.h"
#include "generatingset.h"
#include <iostream>
using argvtype = const char **;
extern void processargs(int &argc, argvtype &argv, int includecmds = 0);
extern puzdef makepuzdef(istream *f);
extern puzdef makepuzdef(string s);
extern void reseteverything();
void processscrambles(istream *f, puzdef &pd, prunetable &pt,
                      generatingset *gs);
int main_search(const char *def_file, const char *scramble_file);
#endif
