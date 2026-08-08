# Symmetry-table structure investigations

Exploratory follow-up to `--fastindex` (see `permrank.h`/`permrank.cpp`,
`fastindex-results.md`): once the symmetry decision for a puzzle's
`setdefs[0]` permutation is a literal n!-entry table, what does that table
actually *look like* -- how compressible is it, and does it have exploitable
structure beyond raw compression? All data files and scripts referenced here
live alongside this file in `experiments/minrot/`.

## 1. Dumping the table (`--fastindex-dump`)

`src/cpp/permrank.cpp`'s `build_fastindex()` can write `pd.fastbits` (the
raw n!-byte table, one byte per permutation rank -- not `fastbitsside`,
which is the small side-table for tie cases) to disk:

```sh
./build/bin/twsearch --fastindex --fastindex-dump cubesymm1.dat \
  --nowrite -M 10 samples/symm/3x3x3_cornersfirst.tws   # CORNERS, n=8
./build/bin/twsearch --fastindex --fastindex-maxn 0 --fastindex-dump edgesymm1.dat \
  --nowrite -M 10 samples/symm/3x3x3.tws                # EDGES, n=12
```

**A real memory bug found and fixed along the way**: the original
`build_fastindex()` buffered every raw 8-byte `ull` bitmask in a temporary
`vector<ull> rawbits(total)` before compacting it down to the final 1-byte-
per-entry table. For CORNERS (n=8, 40320 entries) that's trivial (322KB),
but for EDGES (n=12, 479,001,600 entries) it meant an 8x-oversized ~3.83GB
temporary on top of the ~479MB final table -- pure waste. Fixed to decide
each rank's compact byte value immediately as `lowsymmbits()` returns it
(the single-bit case needs no lookup at all -- the first `nrot` side-table
slots are fixed as `1LL<<i` up front -- and only genuine ties touch a
`map`), so there's no large buffer at all: build peak memory for EDGES
dropped to **506MB**, matching the "half a gigabyte" expected for a table
that size. Build time: **13.58 seconds**, single-threaded.

## 2. Compressibility

| | CORNERS (n=8, 40,320 bytes) | EDGES (n=12, 479,001,600 bytes) |
|---|---|---|
| raw | 100% | 100% |
| Shannon entropy bound (order-blind) | 64.8% (26,136 B) | 57.4% (274,872,944 B) |
| gzip -9/-6 | 47.2% (19,042 B) | 12.4% (59,309,192 B) |
| bzip2 -9 | 49.0% (19,741 B) | -- |
| zstd -19 | 43.1% (17,364 B) | 4.5% (21,771,848 B) |
| **xz -9** | **38.3% (15,456 B)** | **3.75% (17,943,872 B)** |

`xz` wins in both cases. Both compress well past the "no correlation"
Shannon-entropy floor -- edges dramatically more so (57.4% -> 3.75%, >15x
below the floor, vs. corners' 64.8% -> 38.3%, ~1.7x below) -- meaning
there's real sequential/local structure in the byte order itself (nearby
permutation ranks' answers are correlated), not just a skewed value
distribution.

**A clean, exact (not approximate) structural fact in both tables**: each
of the 24 rotations appears as the *sole* winner exactly the same number
of times -- 1,546 times each for CORNERS (24 x 1,546 = 37,104, matching
the popcount histogram's "1 bit: 37,104 (92.02%)" line exactly) and
19,950,467 times each for EDGES (24 x 19,950,467 = 478,811,208, matching
"1 bit: 478,811,208 (99.96%)"). A real group-theoretic uniformity, not a
coincidence of the data.

## 3. Hi/lo decomposition of the table itself

Same split-index idea `permrank.cpp` uses for *ranking* (see its header
comment) applied to the *table*: split the n elements into two halves,
each read as its own raw base-n digit value, and look at the table as a
function of (hi, lo) instead of the combined rank.

`hilo_decompose.py` builds a dense `n_choices^(n/2) x n_choices^(n/2)`
grid (`cubesymm1_hilo.dat`, mostly-unused cells filled with a 0xFF
sentinel; only 40,320 of its 16,777,216 cells are real) -- mainly useful
as a stepping stone to the next idea, not compelling on its own.

## 4. The AND-of-masks hypothesis

**Hypothesis** (user's): the top-half values alone "disallow" some
rotations as candidates, independent of the bottom half, and vice versa;
treat both as bitmasks and AND them together.

**Why this should work mechanistically**: `lowsymmbits()`'s mandatory
round compares, for each rotation m, a value read from exactly one fixed
physical position of `setdefs[0]` (`rotgroup[m].pos.dat[0]`) -- fixed per
rotation, independent of the runtime permutation. So each rotation's
comparison value depends on *either* the hi half *or* the lo half, never
both. `HIMASK[hi]` = OR of every single-winner bitmask seen across all
valid lo-partners of a given hi (an upper bound on what hi alone could
ever produce); `LOMASK[lo]` likewise.

**Results** (`hilo_andmask.py` / `hilo_andmask_edges.py`, natural first-
half/second-half split in both cases):

| | CORNERS (n=8, split 4+4) | EDGES (n=12, split 6+6) |
|---|---|---|
| HIMASK/LOMASK avg popcount | 6.15 / 24 | 8.20 / 24, 8.14 / 24 |
| `HIMASK & LOMASK == actual` exactly | **36.76%** | **20.95%** |
| AND popcount average | 1.862 | 2.157 |

Always a superset of the true answer, never wrong, never missing the
true winner -- just not always down to exactly one bit. Interestingly,
EDGES (the far stronger discriminator overall -- 99.96% vs. 92% single-
bit) is proportionally *worse* for this specific technique than CORNERS;
the AND-mask's effectiveness turns out to hinge on the particular wiring
of which physical position each rotation reads, not on how strong a
discriminator the set is overall (see next section).

EDGES's 479M-permutation double pass ran in **178 seconds total** under
`pypy3` (~7.5M ranks/sec) -- worth remembering for any future work on
this scale; plain CPython would very likely have taken 20-40+ minutes for
the same computation.

## 5. Does the choice of *which* 4 positions matters?

Yes, and it reveals the puzzle's own rotation group directly.
`hilo_all_splits.py` exhaustively evaluates all 35 distinct ways to split
CORNERS' 8 positions into two groups of 4 (`C(8,4)/2`, since a partition
and its complement are the same split) and groups them by identical
(HIMASK avg, LOMASK avg, exact%, AND avg):

| exact% | AND avg | class size | example split |
|---|---|---|---|
| **47.15%** | **1.666** | **1** | **(0,3,4,7)** |
| 39.38% | 1.798 | 4 | (0,1,2,6) |
| 38.78% | 1.830 | 12 | (0,1,2,5) |
| 36.80% | 1.845 | 6 | (0,1,2,4) |
| 36.76% | 1.862 | 3 | (0,1,2,3) -- the "natural" split |
| 35.79% | 1.898 | 3 | (0,1,4,5) |
| 35.26% | 1.879 | 6 | (0,1,2,7) |

Exactly **7 equivalence classes**, sizes summing to 35, every size
dividing 24 (`1, 3, 3, 4, 6, 6, 12`) -- exactly what orbit-stabilizer
theory predicts if these classes are the orbits of the puzzle's own
24-element rotation group acting on "ways to split 8 corners into two
groups of 4". Not a coincidence: it *is* those orbits.

The best split is the lone singleton orbit -- the one partition fixed
(as a whole) by all 24 rotations. Geometrically, a cube's 8 vertices have
exactly one way to 2-color them into two tetrahedra (alternating corners,
no two adjacent corners sharing a color) such that every rotation either
preserves both colors or swaps them wholesale. That this maximally
symmetric partition is also empirically the best-performing split
(47.15% exact vs. the natural split's 36.76%) is a satisfying, non-
coincidental result: it's the one split compatible with the full rotation
group acting on it as a single object, rather than being permuted among
equivalent-but-distinct alternatives.

## Practical implication (not yet built)

Instead of the full n!-byte table, storing just `HIMASK`/`LOMASK` (keyed
by the raw hi/lo halves, `n^(n/2)`-ish entries each -- 1,680 for CORNERS)
would be vastly smaller, gets the exact answer outright more than a third
of the time (using the tetrahedral split), and narrows to ~1-2 candidates
the rest of the time -- falling back to the same real tie-breaking
comparison (`rotconjugatecmp`) the existing code already does for ties.
Not implemented; this document is the investigation, not the feature.

## Scripts (this directory)

- `hilo_decompose.py` -- dense hi/lo grid reshape of `cubesymm1.dat`.
- `hilo_disallow.py` -- first (superseded) pass at the disallow idea,
  single-winner sets only, no AND.
- `hilo_andmask.py` -- the AND-of-masks test on CORNERS.
- `hilo_andmask_edges.py` -- same, scaled to EDGES (run under `pypy3`).
- `hilo_random_splits.py` -- 8 random position-splits vs. the natural one.
- `hilo_all_splits.py` -- exhaustive 35-split equivalence-class analysis.

Data files (`cubesymm1.dat`, `cubesymm1_hilo.dat`, `edgesymm1.dat`,
`edgesymm1.dat.xz`) are regenerable via the commands in section 1 plus the
scripts above; not committed (bulky, and `edgesymm1.dat` alone is 479MB).
