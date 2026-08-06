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
 *   Opt-in (see --jit): off by default because it spawns a C compiler
 *   process and dlopen()s the result, which is a meaningfully bigger ask
 *   of the host machine/environment than the rest of twsearch, and not
 *   every environment this runs in can be assumed to have a working,
 *   fast, or even present system compiler.  When disabled, or whenever
 *   it can't succeed, this is always safe to call and never fatal to the
 *   run: if there's no working C compiler, if dlopen isn't available, if
 *   the puzzle uses identical-piece relabeling or has a very large
 *   rotation group, or if the generated code doesn't check out against
 *   the interpreted routines on a batch of random positions, this just
 *   leaves pd.jitconj/jitconjcmp empty and every caller falls back to
 *   the interpreted implementation.
 */
void jit_build_symmetry(puzdef &pd);
extern int enablejit;
// --jit-cc: use exactly this compiler instead of guessing ($CXX, then
// cc/clang/gcc).  An explicit choice is tried alone, on the theory that
// silently substituting something else when it fails would defeat the
// point of specifying it.  Null/empty means "guess" (the default).
//
// With -v2 or higher (see util.h's verbose), print the generated C
// source; on any failure to compile/dlopen/self-check regardless of
// verbosity, print the compiler's own diagnostic output rather than
// failing silently back to the interpreted implementation.
extern const char *jitcc;
#define JIT_H
#endif
