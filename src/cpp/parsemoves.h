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
/*
 *   The _generously functions also accept moves outside the move filter,
 *   and rotations unless allowrotations is false.  Positions to be solved
 *   pass false: a rotation there can make a position unsolvable with the
 *   puzzle's moves (the search would never end).
 */
allocsetval findmove_generously(const puzdef &pd, const string &s,
                                bool allowrotations = true);
int findmove(const puzdef &pd, const string &mvstring);
void domove(puzdef &pd, setval p, const string &mvstring);
vector<int> parsemovelist(const puzdef &pd, const string &scr);
vector<int> parsemoveorrotationlist(const puzdef &pd, const string &scr);
vector<allocsetval> parsemovelist_generously(const puzdef &pd,
                                             const string &scr);
void parsedomovelist_generously(const puzdef &pd, const string &scr, setval p,
                                bool allowrotations = true);
int isrotation(const string &s);
int domove_or_rotation_q(const puzdef &pd, setval &sv, setval &tmp,
                         const string &s);
#define PARSEMOVES_H
#endif
