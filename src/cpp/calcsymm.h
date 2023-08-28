#ifndef CALCSYMM_H
#include "puzdef.h"
/*
 *   The major improvement needed for this program is to use symmetry to
 *   reduce searches and improve pruning tables.  This code is an early
 *   attempt to automatically determine the symmetry of a given puzzle.
 *   It is not fully ready yet, and should be considered highly
 *   experimental.
 */
void gensymm(const puzdef &pd);
#define CALCSYMM_H
#endif
