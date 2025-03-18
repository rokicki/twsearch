#ifndef ROTATIONS_H
#include "puzdef.h"
void calcrotations(puzdef &pd);
int slowmodm(const puzdef &, const setval, setval);
int slowmodm2(const puzdef &, const setval, setval);
const int MODINV_FORWARD = 1 << 24;
const int MODINV_BACKWARD = 2 * MODINV_FORWARD;
const int MODINV_BOTH = MODINV_FORWARD | MODINV_BACKWARD;
const int MODINV_CNTMASK = MODINV_FORWARD - 1;
int slowmodm2inv(const puzdef &, const setval, setval, setval);
int slowmodmip(const puzdef &, const setval, setval);
int slowmodmip(const puzdef &, const setval, setval, const vector<moove> &,
               const vector<int> &);
extern int disablesymmetry;
#define ROTATIONS_H
#endif
