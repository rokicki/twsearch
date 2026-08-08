#!/usr/bin/env python3
"""Test the hypothesis: for a fixed hi (top 4 corner values), only some
subset of the 24 rotations can ever be the winner, no matter what lo (the
remaining 4 values) is -- and likewise for a fixed lo.  If so, the winner
is essentially "best of hi's allowed candidates" vs. "best of lo's
allowed candidates", which would explain the table's structure/
compressibility directly.
"""
import itertools
from collections import defaultdict

data = open("cubesymm1.dat", "rb").read()

def raw4(a, b, c, d):
    return ((a * 8 + b) * 8 + c) * 8 + d

hi_winners = defaultdict(set)   # hi -> set of single-winner rotation indices seen
lo_winners = defaultdict(set)   # lo -> set of single-winner rotation indices seen
hi_all = defaultdict(set)       # hi -> set of ALL byte values seen (incl. tie codes)
lo_all = defaultdict(set)

for rank, perm in enumerate(itertools.permutations(range(8))):
    hi = raw4(*perm[0:4])
    lo = raw4(*perm[4:8])
    b = data[rank]
    hi_all[hi].add(b)
    lo_all[lo].add(b)
    if b < 24:  # single-bit winner case; byte IS the rotation index directly
        hi_winners[hi].add(b)
        lo_winners[lo].add(b)

def summarize(name, winners):
    sizes = [len(s) for s in winners.values()]
    print(f"{name}: {len(winners)} distinct keys, "
          f"winner-set size min={min(sizes)} max={max(sizes)} "
          f"avg={sum(sizes)/len(sizes):.2f}")
    # how many keys have a winner-set that's a small subset (<24)?
    small = sum(1 for s in sizes if s < 24)
    print(f"  keys with winner-set < 24 (i.e. genuinely restricted): {small}/{len(winners)}")

summarize("hi -> single-winner rotations", hi_winners)
summarize("lo -> single-winner rotations", lo_winners)

# Cross-check: is the union of (hi's allowed set) and (lo's allowed set)
# always big enough to cover the actual winner?  (sanity, should be 100%)
miss = 0
total = 0
for rank, perm in enumerate(itertools.permutations(range(8))):
    hi = raw4(*perm[0:4])
    lo = raw4(*perm[4:8])
    b = data[rank]
    if b < 24:
        total += 1
        if b not in hi_winners[hi] or b not in lo_winners[lo]:
            miss += 1
print(f"sanity: {miss}/{total} winners not in both their hi's and lo's own set (should be 0)")

# The real test: does hi ALONE (independent of lo) restrict candidates?
# Pick a few example hi's and print their winner sets.
print()
print("sample hi -> winner-set (first 5 distinct hi values seen):")
for i, (hi, s) in enumerate(hi_winners.items()):
    if i >= 5:
        break
    print(f"  hi={hi} (perm prefix implied): winners={sorted(s)}")
