#ifndef ROTATIONS_H
#include "puzdef.h"
void calcrotations(puzdef &pd);
int slowmodm(const puzdef &, const setval, setval);
int slowmodm2(const puzdef &, const setval, setval);
int slowmodm2inv(const puzdef &, const setval, setval, setval);
int slowmodmip(const puzdef &, const setval, setval);
int slowmodmip(const puzdef &, const setval, setval, const vector<moove> &,
               const vector<int> &);
#endif
