#ifndef PARSEMOVES_H
#include "generatingset.h"
#include "prunetable.h"
#include "puzdef.h"
/*
 *   Frequently we need to parse a move string.  These moves may come
 *   from the ksolve file directly, or may be constructed moves based
 *   on repetitions; in addition, sometimes when using move filtering
 *   we may still want to be able to describe a position using moves
 *   not in the filter string.  These routines help us manage this.
 */
allocsetval findmove_generously(const puzdef &pd, const string &s);
int findmove(const puzdef &pd, const string &mvstring);
void domove(puzdef &pd, setval p, const string &mvstring);
vector<int> parsemovelist(const puzdef &pd, const string &scr);
vector<int> parsemoveorrotationlist(const puzdef &pd, const string &scr);
vector<allocsetval> parsemovelist_generously(const puzdef &pd,
                                             const string &scr);
int isrotation(const string &s);
#define PARSEMOVES_H
#endif
