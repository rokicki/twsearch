#ifndef FILTERMOVES_H
#include "puzdef.h"
/*
 *   The twsearch program includes the ability to restrict moves from the
 *   command line (without rewriting the puzzle definition file).  This
 *   routine performs said filtering.  Note that such filtering requires
 *   generation of new pruning tables; right now pruning tables cannot be
 *   shared across move subsets.
 */
void filtermovelist(puzdef &pd, const char *movelist);
#define FILTERMOVES_H
#endif
