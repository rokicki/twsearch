#ifndef PERMRANK_H
#include "puzdef.h"
/*
 *   For a puzzle whose primary discriminator set (setdefs[0], the one
 *   slowmodm2/slowmodm2inv narrow candidates through first) has a small
 *   enough number of elements n, precompute an n!-entry table mapping
 *   every one of its possible permutations directly to the exact bitmask
 *   lowsymmbits() would compute for it (see rotations.cpp) -- replacing
 *   that function's runtime scan, at query time, with two table lookups
 *   and an add instead of an O(nrot) (or worse) pass over the rotation
 *   group.
 *
 *   Ranking (mapping a permutation to its index in [0,n!)) is done by
 *   splitting the n elements into a "lo" half of size n/2 and a "hi"
 *   half of the rest, each independently direct-indexed by treating its
 *   raw bytes as a base-n number, and adding: lo[]'s size is n^(n/2),
 *   hi[]'s is n^(n-n/2), both tiny next to the n!-entry result table
 *   they're just there to index into quickly.  This works because a
 *   permutation's standard (Lehmer code) rank is a sum of independent
 *   per-position terms, and the terms for the first half depend only on
 *   which values occupy it and their order -- never on how the second
 *   half is arranged -- while the second half's terms reduce to the
 *   ordinary rank of its own values relative to each other, independent
 *   of what the first half was.  See build() for the derivation.
 *
 *   Useful for n up to maybe 10-11 in practice; both build time and
 *   memory are O(n!), so n=12 (479M entries, ~3.8GB just for the result
 *   table) is a real, expected limit of the approach, not a bug -- the
 *   lo/hi split tables themselves stay small at any n that matters here
 *   (e.g. n=12 splits into two 12^6 ~= 3M-entry tables).
 *
 *   lowsymmbits() only ever reads setdefs[0]'s permutation bytes, never
 *   its orientation bytes, so its output is a pure function of those n
 *   bytes; build_fastindex() exploits that by calling the existing,
 *   already-correct lowsymmbits() once per permutation (enumerated via
 *   std::next_permutation, so no unranking needed at build time either)
 *   and caching what it returns -- no new correctness logic, just
 *   precomputing an existing exact function over its whole domain.
 *   Verified against the interpreted lowsymmbits() on real reachable
 *   positions before being trusted (see build_fastindex()); on any
 *   mismatch this just leaves pd.fastbits empty and every caller falls
 *   back to lowsymmbits() as before, same safety-net shape as jit.cpp.
 *
 *   Opt-in (see --fastindex): off by default while this is still new
 *   and being A/B'd against the rest of this branch's work.
 */
void build_fastindex(puzdef &pd);
extern int enablefastindex;
// Cap on n! (see the size warning above), default 10! (~29MB); 0 means
// "no cap, trust the caller."  --fastindex-maxn.
extern long long fastindexmaxn;
// --fastindex-dump path: after a successful build, write pd.fastbits
// (the raw n!-byte table, one byte per permutation rank -- not
// fastbitsside, which is tiny) to this file.  For poking at how
// compressible the table actually is; null means don't dump.
extern const char *fastindexdumppath;
// Rank of setdefs[0]'s current permutation under whatever n/split
// build_fastindex() last set up; only meaningful (and only ever called)
// when pd.fastbits is non-empty.  A free function rather than a puzdef
// method so puzdef.h doesn't need to know about PermRank at all.
int fastrank(const unsigned char *perm);
#define PERMRANK_H
#endif
