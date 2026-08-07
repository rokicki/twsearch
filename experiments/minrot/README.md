# minrot: symmetry-reduction speedup sandbox

Supporting material for the `symm-combined` branch's work on
`slowmodm2`/`slowmodm2inv` (see `src/cpp/rotations.cpp`, `src/cpp/jit.cpp`):
flattened rotation-group data layout, JIT-compiled conjugate/conjcmp
routines (`--jit`, with `--jit-style` choosing the generated code's shape),
and a real-solve A/B benchmark comparing all of it against `main`.

`--jit-style`: `0` table (one shared routine, small, indexed by rotation
number at call time), `1` portable (default; one specialized routine per
rotation, fastest but code size scales with rotation count), `2` neon (like
0 but ARM NEON `vqtbl4q_u8` gather instead of scalar loads; AArch64 only),
`3` sse (like 2 but x86 SSSE3/SSE4.1 `_mm_shuffle_epi8`), `4` avx512 (not
yet implemented). See `src/cpp/jit.h`/`jit.cpp` for the design writeup.

## Real-solve A/B benchmark (`run_megaminx_ab.sh`)

Solves a fixed megaminx scramble twice each on six configurations --
`main` (`twsearch_original`), and `symm-combined` with no JIT and
`--jit-style` 0/1/2/3 (`twsearch_current`) -- strictly sequentially (never
concurrently; timing-sensitive), then prints a table of wall time,
instructions retired, cycles elapsed, and IPC, averaged over the two runs
per configuration.

Setup (once):

```sh
git checkout main
make clean && make -j
cp build/bin/twsearch build/bin/twsearch_original

git checkout symm-combined
make -j
cp build/bin/twsearch build/bin/twsearch_current
```

Then, from the repo root or from here:

```sh
./experiments/minrot/run_megaminx_ab.sh
```

It's meant to be launched once and left alone -- expect on the order of a
couple hours total (12 solves, each several minutes). Results land in
`logs/megaminx_ab_summary.txt` (gitignored -- this is per-session output,
not something to commit) and the table is also reprinted to stdout at the
end.

`--jit-style 3` (sse) only does anything on x86; on other architectures the
generated code simply fails to compile and it falls back to interpreted
(same numbers as `current_nojit`) -- expected, not a bug, and the only way
to give this script the same six-way shape on every machine. `--jit-style
2` (neon) is the converse: only does anything on ARM.

Instructions/cycles come from whatever the platform can supply:
`/usr/bin/time -l` on macOS (built in), `perf stat -e instructions,cycles`
on Linux if `perf` is installed and usable without extra privileges,
otherwise wall time only (still meaningful, just noisier). Edit
`SEQ`/`PUZZLE` in the script to benchmark a different scramble or puzzle;
`aggregate_megaminx_ab.py` re-summarizes an existing `logs/` directory
without rerunning anything.

## Cross-checking --jit-style 3 (sse) without x86 hardware

`--jit-style 3`'s generated code was developed on Apple Silicon, which
can't execute it directly. It was instead validated by cross-compiling
(`clang -target x86_64-apple-macosx11 -mssse3 -msse4.1`) against the
already self-check-proven `--jit-style 0` output for the same puzzle as
ground truth, then actually *running* that comparison under Rosetta --
not just checking that it compiles. To reproduce (macOS on Apple Silicon
with Rosetta installed; `softwareupdate --install-rosetta` if needed):

```sh
# Dump both styles' generated source for the same puzzle:
./build/bin/twsearch --jit --jit-style 0 -v2 --nowrite -M 10 samples/symm/megaminx.tws > s0.txt
./build/bin/twsearch --jit --jit-style 3 -v2 --nowrite -M 10 samples/symm/megaminx.tws > s3.txt
# Extract the "generated JIT source" banner's contents from each into
# style0.c / style3.c, then rename style0.c's public symbols so both can
# link into one program without colliding:
#   AP[ -> REFAP[   CP[ -> REFCP[   MODA -> REFMODA   tblconj -> refconj
# (tblconjcmp becomes refconjcmp for free, since it contains "tblconj" as
# a prefix), and re-add `typedef unsigned char uc;` at the top since the
# rename strips it along with everything else matching s/MODA/REFMODA/.
#
# Then write a small main() that feeds many *structurally valid* random
# positions (see below) through both refconj/refconjcmp and
# tblconj/tblconjcmp and compares byte-for-byte, compile all three
# together for x86_64, and run under Rosetta:
clang -target x86_64-apple-macosx11 -O2 -c style0_renamed.c -o s0.o
clang -target x86_64-apple-macosx11 -O2 -mssse3 -msse4.1 -c style3.c -o s3.o
clang -target x86_64-apple-macosx11 -O2 -c main.c -o m.o
clang -target x86_64-apple-macosx11 -O2 -o validate s0.o s3.o m.o
arch -x86_64 ./validate
```

**Structurally valid positions matter**: don't fill `bp` with uniform
random bytes. Real positions respect each setdef's shape -- permutation
bytes in `[0,n)`, orientation bytes in `[0,omod)` -- which you can read
off the generated source's `ORISPECS`/`ZEROSPECS` tables. Violating that
(e.g. an orientation byte >= omod) drives an out-of-bounds `MODA[]` lookup
that's undefined behavior in *both* implementations, producing spurious
mismatches that look like real bugs but aren't (this cost real debugging
time the first time through -- don't repeat it).

This gets you real execution of the actual generated gather/blend
instructions, just not a real end-to-end twsearch run -- that still needs
to happen via `--jit --jit-style 3` on actual x86 hardware, where the
existing self-check safety net applies the same as any other style (and
will safely fall back to interpreted if something's still wrong).

## bench.cpp / extract.cpp

An older, narrower microbenchmark: candidate "guess which rotation wins"
algorithms and candidate conjugate implementations (interpreted, `--jit`'s
per-rotation-duplication shape, `--jit-style 0`'s shared-routine shape),
cross-tested against real twsearch ground truth. `extract.cpp` links
against real twsearch object files to dump that ground truth once;
`bench.cpp` itself has no twsearch dependency at runtime. See `make bench`
/ `make extractor`. (Predates `--jit-style` 2/3; doesn't cover those.)
