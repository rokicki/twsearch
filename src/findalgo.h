#ifndef FINDALGO_H
#include "puzdef.h"
/*
 *   The twsearch program supports finding useful algorithms for a given
 *   puzzle.  These are usually short, or easy to remember, sequences that
 *   affect only a small subset of the pieces.  These routines do the
 *   search for such algorithms.  The implementation is currently just
 *   brute force; more intelligence can be added later through the
 *   consideration of support.
 */
void findalgos(const puzdef &pd, int which) ;
extern int algostrict ;
#define FINDALGO_H
#endif
