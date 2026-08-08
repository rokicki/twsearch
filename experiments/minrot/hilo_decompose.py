#!/usr/bin/env python3
"""Reshape cubesymm1.dat (40320 bytes, indexed by permutation rank in the
same lexicographic order std::next_permutation/itertools.permutations use)
into a dense 4096x4096 grid indexed directly by (hi, lo) -- the same raw
base-8 4-digit encoding permrank.cpp uses for its LO[]/HI[] tables: hi from
perm[0..3], lo from perm[4..7], each as ((a*8+b)*8+c)*8+d. Most of the
4096x4096 space is unused (only 1680 of 4096 raw 4-digit patterns are
valid -- i.e. have 4 distinct digits -- per half, and hi/lo must also be
disjoint from each other): 1680*24 = 40320 valid cells, filled with the
real symmetry-table byte; the rest get a sentinel (0xFF, safely outside
the real value range of 0..231).
"""
import itertools
import sys

data = open("cubesymm1.dat", "rb").read()
assert len(data) == 40320

def raw4(a, b, c, d):
    return ((a * 8 + b) * 8 + c) * 8 + d

grid = bytearray(b"\xff" * (4096 * 4096))
valid = 0
for rank, perm in enumerate(itertools.permutations(range(8))):
    hi = raw4(*perm[0:4])
    lo = raw4(*perm[4:8])
    grid[hi * 4096 + lo] = data[rank]
    valid += 1
assert valid == 40320

with open("cubesymm1_hilo.dat", "wb") as f:
    f.write(grid)
print(f"wrote cubesymm1_hilo.dat: {len(grid)} bytes ({valid} valid cells, "
      f"{100*valid/len(grid):.2f}% populated)")
