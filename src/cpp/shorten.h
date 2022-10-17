#ifndef SHORTEN_H
#include <vector>
#include "puzdef.h"
using namespace std ;
/*
 *   Given a sequence, try to shorten it by optimizing subsequences.
 *   Right now this only applies to invertible puzzles.
 */
vector<int> shorten(const puzdef &pd, const vector<int> &orig) ;
#define SHORTEN_H
#endif
