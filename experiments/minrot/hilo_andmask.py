#!/usr/bin/env python3
"""Test the AND-of-masks hypothesis precisely: HIMASK[hi] = OR of every
actual answer-bitmask seen across hi's 24 lo-partners (an upper bound on
what hi alone could ever produce); LOMASK[lo] likewise.  Then check, for
every one of the 40320 permutations, whether actual == HIMASK[hi] &
LOMASK[lo] (not just actual subset-of).
"""
import itertools
from collections import defaultdict

data = open("cubesymm1.dat", "rb").read()

def raw4(a, b, c, d):
    return ((a * 8 + b) * 8 + c) * 8 + d

# First pass: only single-winner (byte<24) entries give us a directly-known
# 1-bit mask without needing fastbitsside (not dumped).  Ties (byte>=24)
# are excluded from HIMASK/LOMASK construction for now -- see note at end.
hi_mask = defaultdict(int)
lo_mask = defaultdict(int)
perms = list(itertools.permutations(range(8)))
ties = 0
for rank, perm in enumerate(perms):
    hi = raw4(*perm[0:4])
    lo = raw4(*perm[4:8])
    b = data[rank]
    if b < 24:
        hi_mask[hi] |= (1 << b)
        lo_mask[lo] |= (1 << b)
    else:
        ties += 1
print(f"ties excluded from mask-building: {ties}/40320 ({100*ties/40320:.2f}%)")

hi_sizes = [bin(m).count("1") for m in hi_mask.values()]
lo_sizes = [bin(m).count("1") for m in lo_mask.values()]
print(f"HIMASK popcount: min={min(hi_sizes)} max={max(hi_sizes)} avg={sum(hi_sizes)/len(hi_sizes):.2f} (of 24 possible)")
print(f"LOMASK popcount: min={min(lo_sizes)} max={max(lo_sizes)} avg={sum(lo_sizes)/len(lo_sizes):.2f} (of 24 possible)")

# Now check predictive power on the single-winner cases (where we know the
# true answer is exactly 1 bit).
exact = 0
superset_only = 0
total_single = 0
and_sizes = []
for rank, perm in enumerate(perms):
    hi = raw4(*perm[0:4])
    lo = raw4(*perm[4:8])
    b = data[rank]
    if b >= 24:
        continue
    total_single += 1
    predicted = hi_mask[hi] & lo_mask[lo]
    actual = 1 << b
    and_sizes.append(bin(predicted).count("1"))
    if predicted == actual:
        exact += 1
    elif predicted & actual == actual:
        superset_only += 1
    else:
        print(f"  MISSING BIT at rank {rank}: predicted={predicted:b} actual={actual:b}")

print()
print(f"single-winner cases: {total_single}")
print(f"  HIMASK & LOMASK == actual exactly: {exact} ({100*exact/total_single:.2f}%)")
print(f"  HIMASK & LOMASK is a strict superset of actual: {superset_only} ({100*superset_only/total_single:.2f}%)")
print(f"  AND popcount distribution: min={min(and_sizes)} max={max(and_sizes)} avg={sum(and_sizes)/len(and_sizes):.3f}")
