#ifndef TEST_H
#include "puzdef.h"
#include "generatingset.h"
/*
 *   A couple of test routines.
 */
extern int scramblemoves ;
void timingtest(puzdef &pd) ;
void solvetest(puzdef &pd, generatingset *gs = 0) ;
#define TEST_H
#endif
