#ifndef CANON_H
#include "puzdef.h"
#include <vector>
using namespace std;
/*
 *   This set of routines calculates the canonical sequences for a given
 *   puzzle.  The normal, default version does this by calculating what
 *   pairs of moves commute, and deriving a state graph from this.  The
 *   second version uses a hashtable of identical states (if the group
 *   is a graph) which lets it do more aggressive pruning at the cost of
 *   additional memory usage.  With the show canonical states option
 *   you can see how effective each strategy is for a given puzzle, and
 *   from that decide which to use.
 */
void makecanonstates(puzdef &pd);
extern vector<ull> canonmask;
extern vector<vector<int>> canonnext;
extern vector<int> cancelmoves(const puzdef &pd, vector<int> mvseq);
extern vector<int> canonicalize(const puzdef &pd, vector<int> mvseq);
extern int ccount, canonlim;
void makecanonstates2(puzdef &pd);
void showcanon(const puzdef &pd, int show);
#define CANON_H
#endif
