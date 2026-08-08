#!/usr/bin/env python3
"""Exhaustively evaluate all 35 distinct ways to split the 8 corner
positions into two groups of 4 (C(8,4)/2, since {hi,lo} and {lo,hi} are
the same partition), group the results by identical (HIMASK avg, LOMASK
avg, exact%, AND avg) -- almost certainly corresponding to orbits of the
partition under the puzzle's own rotation group -- and print one example
split per equivalence class.
"""
import itertools
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

    return (
        round(sum(hi_sizes) / len(hi_sizes), 6),
        round(sum(lo_sizes) / len(lo_sizes), 6),
        round(100 * exact / total_single, 6),
        round(and_sum / total_single, 6),
    )


# All 35 canonical partitions: 4-subsets containing position 0 (so we
# never double-count a partition and its complement).
canonical_splits = [
    (0,) + rest for rest in itertools.combinations(range(1, 8), 3)
]
assert len(canonical_splits) == 35

groups = defaultdict(list)
for s in canonical_splits:
    stats = analyze(s)
    groups[stats].append(s)

print(f"{len(groups)} distinct equivalence classes among {len(canonical_splits)} partitions\n")
# Sort by exact% descending for a readable ranking.
for stats, members in sorted(groups.items(), key=lambda kv: -kv[0][2]):
    hi_avg, lo_avg, exact_pct, and_avg = stats
    print(f"hi_avg={hi_avg:<7} lo_avg={lo_avg:<7} exact%={exact_pct:<8} "
          f"and_avg={and_avg:<7} | class size={len(members):>2} "
          f"| example hi={members[0]}")
