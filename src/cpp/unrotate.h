#ifndef UNROTATE_H
#include "puzdef.h"
#include <vector>
using namespace std;
/*
 *   Given a sequence, move all rotations to the end, if possible, by
 *   changing <rotation> <move> to <move'> <rotation>.
 */
vector<int> unrotate(const puzdef &pd, const vector<int> &orig);
#define UNROTATE_H
#endif
