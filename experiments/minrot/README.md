# minrot: symmetry-reduction speedup sandbox

Supporting material for the `symm-combined` branch's work on
`slowmodm2`/`slowmodm2inv` (see `src/cpp/rotations.cpp`, `src/cpp/jit.cpp`):
flattened rotation-group data layout, JIT-compiled conjugate/conjcmp
routines (full per-rotation duplication via `--jit`, or a single
table-indexed shared routine via `--jit --jit-table`), and a real-solve
A/B benchmark comparing all of it against `main`.

## Real-solve A/B benchmark (`run_megaminx_ab.sh`)

Solves a fixed megaminx scramble twice each on four configurations --
`main` (`twsearch_original`), and `symm-combined` with no JIT, `--jit`,
and `--jit --jit-table` (`twsearch_current`) -- strictly sequentially
(never concurrently; timing-sensitive), then prints a table of wall time,
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

It's meant to be launched once and left alone -- expect on the order of an
hour total (8 solves, each several minutes). Results land in
`logs/megaminx_ab_summary.txt` (gitignored -- this is per-session output,
not something to commit) and the table is also reprinted to stdout at the
end.

Instructions/cycles come from whatever the platform can supply:
`/usr/bin/time -l` on macOS (built in), `perf stat -e instructions,cycles`
on Linux if `perf` is installed and usable without extra privileges,
otherwise wall time only (still meaningful, just noisier). Edit
`SEQ`/`PUZZLE` in the script to benchmark a different scramble or puzzle;
`aggregate_megaminx_ab.py` re-summarizes an existing `logs/` directory
without rerunning anything.

## bench.cpp / extract.cpp

An older, narrower microbenchmark: candidate "guess which rotation wins"
algorithms and candidate conjugate implementations (interpreted, `--jit`'s
full-duplication shape, `--jit-table`'s shared-routine shape), cross-tested
against real twsearch ground truth. `extract.cpp` links against real
twsearch object files to dump that ground truth once; `bench.cpp` itself
has no twsearch dependency at runtime. See `make bench` / `make extractor`.
