# Unresolved: one observed multi-threaded stall, 3x3x3 corners-first

**Status: open, not reproduced, root cause unknown.**

## What happened

Running (default thread count, this machine's full core count):

```sh
echo "B F D' D2 D2 B2 B R' F' L2 F2 F F' D D2 R2 F R' D R' L2 D2 L' D' F B' L2 U2 D' B2 D U2 B F' L B U' U' D' F2 L' R' B L F' R' U U' D F'" \
  | ./build/bin/twsearch_current --jit --jit-style 1 --nowrite -M 4000 \
    -s samples/symm/3x3x3_cornersfirst.tws
```

(the same 50-move scramble against `3x3x3_cornersfirst.tws` -- see
`../3x3x3_cornersfirst.tws`, CORNERS reordered to `setdefs[0]` -- with
`--jit --jit-style 1`, `--fastindex` *not* passed) produced no further
output after `anomalous_stall_20260807_094155.log`'s last line ("Filling
depth 10 val 2 ... in 134.5") for 50+ minutes before being killed. The
file's mtime (09:41:55) shows that line was written ~4 minutes after
launch; the process was still alive, and per `ps` was consuming
substantial cumulative CPU time, when killed roughly 50 minutes later.
That's the completion of the depth-10 prune-table fill; per later clean
runs, the very next step is `pt.checkextend()` then
`makeworkchunks(pd, 18, p, ...)` (single-threaded, heavy on `slowmodm2`,
in solve.cpp) followed by the actual parallel depth-18 search -- the
stall is somewhere in that next phase, not in the fill path itself.

## What's been ruled out

- **Not the ternary/branchy auto-tune**: fixed and stable well before this
  incident (see `autotune_symmguess()` in `src/cpp/rotations.cpp`); also
  runs once, single-threaded, before any worker threads spawn.
- **Not `--fastindex`**: wasn't passed on this run; `build_fastindex()`
  early-returns immediately when `enablefastindex` is 0.
- **Not a spinlock**: `get_global_lock()`/`release_global_lock()`
  (`src/cpp/threads.cpp`) are real `pthread_mutex_t`, not busy-wait.
- **Not an unbounded loop**: `lowsymmbits()`'s extend loop
  (`src/cpp/puzdef.h`) is bounded by `setdefs[0].size`; `makeworkchunks()`
  (`src/cpp/workchunks.cpp`) is bounded by `40 * mythreads`.
- **Not system CPU contention**: high cumulative CPU time during the
  stall (threads were doing *something*, not blocked/idle) argues against
  simple scheduling contention from other processes.
- **Not memory pressure/swapping**: user was watching Activity Monitor's
  CPU/memory pressure chart live during the incident; it never went red.
- **Not reproducible**: 4 immediate repeat attempts of the identical
  command (see `../logs/mt_corners_*.log` if still present, or rerun)
  all completed cleanly in 200-240s, consistent with the expected ~8x
  multi-core speedup over the single-threaded time (1767.7s, measured
  separately and cleanly with `-t 1`).

## What's still true

The algorithmic cost of corners-first vs. edges-first, isolated from
threading entirely, is small and well-measured: ~11-16% slower across
`-T`'s "moves plus symmetry" benchmark, an escalating single-threaded
(`-t 1`) scramble-length sweep (12/14/16 moves), and a full single-threaded
run of the exact scramble above (1560.0s edges vs. 1767.7s corners).
None of that comes close to explaining a 50-minute-vs-3-minute gap by
itself -- something separate and currently unexplained happened during
that one multi-threaded run.

## If this happens again

Don't kill it immediately -- instead, before killing:

1. `ps aux | grep twsearch_current` for per-thread/process CPU%.
2. `sample <pid> 10 -f /tmp/sample_stall.txt` (macOS) to get a stack
   profile *during the actual stall*, not during normal progress (the
   one sample taken during this incident caught the process still
   making progress, not yet stalled -- see `../` for that profile,
   now gone from /tmp).
3. `vm_stat` / Activity Monitor's memory pressure graph, to rule out (or
   confirm) memory pressure directly rather than by inference.
4. If on macOS and `lldb` is available, attaching and getting a full
   backtrace of every thread would be far more conclusive than sampling.

Files here: `anomalous_stall_20260807_094155.log` (the actual incident's
output, truncated at the stall), `one_clean_run_cpu_samples.log` (10s of
%CPU samples from one *successful* multi-threaded run, for comparison --
steady 1350-1460%, no dips).
