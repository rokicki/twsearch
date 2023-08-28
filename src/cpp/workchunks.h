#ifndef WORKCHUNKS_H
#include "puzdef.h"
/*
 *   Sometimes we want to split a search tree among threads.  This
 *   routine calculates the work chunks and parcels them out as
 *   needed.
 */
extern vector<ull> workchunks;
extern vector<int> workstates;
extern int workat;
void makeworkchunks(const puzdef &pd, int d, int symmreduce);
#define WORKCHUNKS_H
#endif
