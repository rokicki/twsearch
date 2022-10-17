#ifndef ANTIPODE_H
#include "puzdef.h"
/*
 *   These routines support stashing away positions as we find them
 *   during God's algorithm searches so we can later emit them.
 *   Instead of scanning whatever result data structure we have,
 *   (which might be problematic depending on how we store things),
 *   we call stash when we find a deep position, and that code can
 *   choose to keep the encoded position, or ignore it if we already
 *   have enough.
 *
 *   We support position encoding both by loose packing and by
 *   dense packing.
 */
extern ll antipodecount ;
void showantipodes(const puzdef &pd, loosetype *beg, loosetype *end) ;
void resetantipodes() ;
void showantipodesloose(const puzdef &pd) ;
void showantipodesdense(const puzdef &pd, int ordered) ;
void stashantipodesloose(loosetype *beg, loosetype *end) ;
void stashantipodedense(ull val) ;
#define ANTIPODE_H
#endif
