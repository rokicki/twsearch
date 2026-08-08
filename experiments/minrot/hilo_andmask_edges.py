#!/usr/bin/env python3
"""Same AND-of-masks test as hilo_andmask.py, scaled to EDGES (n=12,
split 6+6, base-12 digits).  479M permutations -- run under pypy3, it's
much faster than cpython for this kind of tight pure-Python loop.
"""
import itertools
import time
from collections import defaultdict

N = 12
SPLIT = 6
TOTAL = 479001600

print("loading edgesymm1.dat...", flush=True)
data = open("edgesymm1.dat", "rb").read()
assert len(data) == TOTAL

def raw6(p):
    r = 0
    for x in p:
        r = r * N + x
    return r

hi_mask = defaultdict(int)
lo_mask = defaultdict(int)
ties = 0
t0 = time.time()
PROGRESS_EVERY = 20_000_000
for rank, perm in enumerate(itertools.permutations(range(N))):
    hi = raw6(perm[0:SPLIT])
    lo = raw6(perm[SPLIT:N])
    b = data[rank]
    if b < 24:
        hi_mask[hi] |= (1 << b)
        lo_mask[lo] |= (1 << b)
    else:
        ties += 1
    if rank and rank % PROGRESS_EVERY == 0:
        el = time.time() - t0
        rate = rank / el
        eta = (TOTAL - rank) / rate
        print(f"  rank={rank:,} ({100*rank/TOTAL:.1f}%) elapsed={el:.0f}s "
              f"rate={rate/1e6:.2f}M/s eta={eta:.0f}s", flush=True)

print(f"pass 1 done in {time.time()-t0:.0f}s")
print(f"ties excluded from mask-building: {ties}/{TOTAL} ({100*ties/TOTAL:.4f}%)")

hi_sizes = [bin(m).count("1") for m in hi_mask.values()]
lo_sizes = [bin(m).count("1") for m in lo_mask.values()]
print(f"distinct hi keys: {len(hi_mask)}, distinct lo keys: {len(lo_mask)}")
print(f"HIMASK popcount: min={min(hi_sizes)} max={max(hi_sizes)} avg={sum(hi_sizes)/len(hi_sizes):.2f} (of 24 possible)")
print(f"LOMASK popcount: min={min(lo_sizes)} max={max(lo_sizes)} avg={sum(lo_sizes)/len(lo_sizes):.2f} (of 24 possible)")

# Second pass: predictive power on single-winner cases.
exact = 0
superset_only = 0
total_single = 0
and_sizes_sum = 0
t1 = time.time()
for rank, perm in enumerate(itertools.permutations(range(N))):
    hi = raw6(perm[0:SPLIT])
    lo = raw6(perm[SPLIT:N])
    b = data[rank]
    if b >= 24:
        continue
    total_single += 1
    predicted = hi_mask[hi] & lo_mask[lo]
    actual = 1 << b
    and_sizes_sum += bin(predicted).count("1")
    if predicted == actual:
        exact += 1
    elif predicted & actual == actual:
        superset_only += 1
    else:
        print(f"  MISSING BIT at rank {rank}: predicted={predicted:b} actual={actual:b}")
    if rank and rank % PROGRESS_EVERY == 0:
        el = time.time() - t1
        print(f"  pass2 rank={rank:,} ({100*rank/TOTAL:.1f}%) elapsed={el:.0f}s", flush=True)

print()
print(f"pass 2 done in {time.time()-t1:.0f}s")
print(f"single-winner cases: {total_single}")
print(f"  HIMASK & LOMASK == actual exactly: {exact} ({100*exact/total_single:.2f}%)")
print(f"  HIMASK & LOMASK is a strict superset of actual: {superset_only} ({100*superset_only/total_single:.2f}%)")
print(f"  AND popcount avg: {and_sizes_sum/total_single:.3f}")
