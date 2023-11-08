#ifndef GOD_H
#include "puzdef.h"
/*
 *   This file contains various implementations of God's algorithm,
 *   depending on the size and complexity of the puzzle.
 */
extern ull symcoordgoal;
void dotwobitgod(puzdef &pd);
void dotwobitgod2(puzdef &pd);
void doarraygod(const puzdef &pd);
void doarraygodsymm(const puzdef &pd);
void doarraygod2(const puzdef &pd);
#define GOD_H
#endif
