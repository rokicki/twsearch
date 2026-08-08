#!/usr/bin/env python3
"""Does the hi/lo AND-mask technique's predictive power depend on *which*
4 of the 8 corner positions get grouped as "hi" (vs the natural first-
half/second-half split)?  Try 8 random 4-of-8 position splits plus the
natural one, and compare HIMASK/LOMASK/AND stats across all of them.
"""
import itertools
import random
from collections import defaultdict

data = open("cubesymm1.dat", "rb").read()
assert len(data) == 40320
perms = list(itertools.permutations(range(8)))


def raw4(vals):
    r = 0
    for v in vals:
        r = r * 8 + v
    return r


def analyze(hi_positions):
    hi_positions = tuple(sorted(hi_positions))
    lo_positions = tuple(p for p in range(8) if p not in hi_positions)
    hi_mask = defaultdict(int)
    lo_mask = defaultdict(int)
    for rank, perm in enumerate(perms):
        b = data[rank]
        if b >= 24:
            continue
        hi = raw4(perm[p] for p in hi_positions)
        lo = raw4(perm[p] for p in lo_positions)
        hi_mask[hi] |= (1 << b)
        lo_mask[lo] |= (1 << b)

    hi_sizes = [bin(m).count("1") for m in hi_mask.values()]
    lo_sizes = [bin(m).count("1") for m in lo_mask.values()]

    exact = 0
    total_single = 0
    and_sum = 0
    for rank, perm in enumerate(perms):
        b = data[rank]
        if b >= 24:
            continue
        total_single += 1
        hi = raw4(perm[p] for p in hi_positions)
        lo = raw4(perm[p] for p in lo_positions)
        predicted = hi_mask[hi] & lo_mask[lo]
        actual = 1 << b
        and_sum += bin(predicted).count("1")
        if predicted == actual:
            exact += 1
        assert predicted & actual == actual, "AND must always be a superset"

    return {
        "hi_positions": hi_positions,
        "hi_avg": sum(hi_sizes) / len(hi_sizes),
        "lo_avg": sum(lo_sizes) / len(lo_sizes),
        "exact_pct": 100 * exact / total_single,
        "and_avg": and_sum / total_single,
    }


random.seed(20260808)
splits = [(0, 1, 2, 3)]  # natural split first, as baseline
seen = {splits[0]}
while len(splits) < 9:
    s = tuple(sorted(random.sample(range(8), 4)))
    if s not in seen and tuple(p for p in range(8) if p not in s) not in seen:
        splits.append(s)
        seen.add(s)

print(f"{'hi positions':<16} {'HIMASK avg':>11} {'LOMASK avg':>11} "
      f"{'AND exact%':>11} {'AND avg':>8}")
for i, s in enumerate(splits):
    r = analyze(s)
    label = "natural" if i == 0 else str(s)
    print(f"{label:<16} {r['hi_avg']:>11.2f} {r['lo_avg']:>11.2f} "
          f"{r['exact_pct']:>10.2f}% {r['and_avg']:>8.3f}")
