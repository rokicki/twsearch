#ifndef TEST_H
#include "generatingset.h"
#include "puzdef.h"
/*
 *   A couple of test routines.
 */
extern int scramblemoves;
void timingtest(puzdef &pd);
void solvetest(puzdef &pd, generatingset *gs = 0);
#define TEST_H
#endif
