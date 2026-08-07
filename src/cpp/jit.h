#ifndef JIT_H
#include "puzdef.h"
/*
 *   Attempt to JIT-compile specialized replacements for the per-rotation
 *   inner loops used by symmetry reduction (see rotations.cpp and, in
 *   particular, slowmodm2/slowmodm2inv).  Two code shapes are available
 *   (see --jit-table below); either way, the result is equivalent to (but
 *   much faster than) pd.rotconjugate(pd.rotinvmap[m], p1, pd.rotgroup[m].pos,
 *   p2) and pd.rotconjugatecmp(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, p2),
 *   reachable through pd.jitconjcall(m, p1, p2)/pd.jitconjcmpcall(m, p1, p2)
 *   (or pd.havejit() to ask whether either is populated) rather than
 *   directly, since callers shouldn't need to care which shape is active.
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
 *   leaves pd.jitconj/jitconjcmp/jitconjtable/jitconjcmptable empty and
 *   every caller falls back to the interpreted implementation.
 */
void jit_build_symmetry(puzdef &pd);
extern int enablejit;
// --jit-table: generate one shared conj()/conjcmp() pair, selected at call
// time by a rotation index argument and reading its per-rotation
// permutation/table data out of flat arrays, instead of --jit's default of
// one fully-specialized (and much larger, all told) function pair per
// rotation.  Trades some speed for code size roughly 1/nrot of the
// default's -- worth trying on puzzles with large rotation groups (e.g.
// megaminx) if instruction-cache pressure from the default's generated
// code turns out to matter on the target machine.  Only meaningful
// alongside --jit; ignored otherwise.
extern int jittable;
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
