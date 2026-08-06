#ifndef JIT_H
#include "puzdef.h"
/*
 *   Attempt to JIT-compile specialized replacements for the per-rotation
 *   inner loops used by symmetry reduction (see rotations.cpp and, in
 *   particular, slowmodm2/slowmodm2inv).  On success this populates
 *   pd.jitconj/pd.jitconjcmp, one function pointer per rotation-group
 *   element, each equivalent to (but much faster than)
 *   pd.rotconjugate(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, p2) and
 *   pd.rotconjugatecmp(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, p2).
 *
 *   This is always safe to call and never fatal to the run: if there's no
 *   working C compiler, if dlopen isn't available, if the puzzle uses
 *   identical-piece relabeling or has a very large rotation group, or if
 *   the generated code doesn't check out against the interpreted routines
 *   on a batch of random positions, this just leaves pd.jitconj/jitconjcmp
 *   empty and every caller falls back to the interpreted implementation.
 */
void jit_build_symmetry(puzdef &pd);
extern int disablejit;
#define JIT_H
#endif
