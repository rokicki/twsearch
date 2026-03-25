# Multiphase Solver: Design and Implementation

This document describes the changes made on the `concurrent-solves` branch to
add an N-phase concurrent solver to twsearch, modelled on Kociemba's two-phase
algorithm but generalised to an arbitrary number of phases with independent move
sets and subgroup targets.

---

## Motivation

The classic way to solve large puzzles like the 3x3x3 optimally (Kociemba's
algorithm) works by splitting the search into two phases.  Phase 1 reduces any
scramble into a position reachable by a restricted set of moves (the "G1"
subgroup — quarter-turn-free in one axis).  Phase 2 then solves that position
using only those restricted moves, which is a much smaller search space.  The
phases run in a pipeline: whenever phase 1 finds a solution it hands the
resulting position to phase 2, and the two work concurrently.  The total length
is minimised across all phase 1 / phase 2 splits.

twsearch already supported single-phase IDA* solving and subgroup-target
solving.  This branch adds the plumbing to chain an arbitrary number of such
phases together.

---

## Prerequisite refactoring: encapsulating solve state

Before concurrent solves could work, all of the mutable state that the IDA*
solver touched during a search had to be moved out of globals.  The original
code scattered this state across a collection of file-scope globals in
`solve.cpp` and `solve.h`:

```
ll solutionsfound, solutionsneeded;
int noearlysolutions, onlyimprovements, alloptimal, phase2, optmindepth;
int maxdepth, didprepass, requesteduthreading, workinguthreading;
string lastsolution;
int globalinputmovecount;
static vector<ull> workchunks;
int workat;
vector<workerparam> workerparams;
solveworker solveworkers[MAXTHREADS];
int (*callback)(...);
int (*flushback)(int);
```

Two new structs were introduced to replace these globals.

### `solveoptions`

A plain data struct holding everything that *configures* a solve — the
parameters that are set before the search starts and read but never written
during it.  Fields include `solutionsneeded`, `maxdepth`, `alloptimal`,
`noearlysolutions`, `onlyimprovements`, `phase2`, `optmindepth`,
`requesteduthreading`, `didprepass`, `randomstart`, `globalinputmovecount`, a
`thread_base`/`thread_count` pair (described below), a `no_checkextend` flag,
and `std::function` callback/flushback hooks.

A single global instance `g_opts` is still written by command-line parsing so
that the existing call sites are unchanged.  The `solve()` overload that
existing code calls snapshots `g_opts` into the context at entry.

### `solvecontext`

A struct holding everything that *changes* during a solve — the mutable shared
state that worker threads read and write.  Fields include `solutionsfound`,
`lastsolution`, `workchunks`, `workat`, `workinguthreading`, `thread_base`,
`thread_count`, the array of `solveworker` objects, `workerparams`, and the
`randomized` move-order table.  Every `microthread` and `solveworker` holds a
pointer back to the `solvecontext` that owns it, rather than reaching into
globals.

Because `solvecontext` is stack-allocated inside `solve()`, two concurrent
calls to `solve()` each have their own independent context with no shared
mutable state.

### New `solve()` overload

A second overload was added:

```cpp
int solve(const puzdef &pd, prunetable &pt, const setval p,
          const solveoptions &opts, generatingset *gs = 0);
```

This takes an explicit `solveoptions` instead of snapshotting `g_opts`, making
it safe to call from concurrent threads with independent option sets.  The
multiphase solver uses this overload exclusively.

### Canon state moved into `puzdef`

The canonical-move state arrays `canonmask` and `canonnext` were previously
globals in `canon.cpp`.  They have been moved into `puzdef` so that each
phase's independently-built puzzle definition carries its own canonical state,
preventing any cross-phase interference.

### Callback and flushback changed to `std::function`

The original `callback` and `flushback` were raw C function pointers.  They
have been changed to `std::function<...>` so that the multiphase phase workers
can supply closures that capture per-phase context (the work item, the
phase index, the shared `best_total`, etc.) without needing global storage.

---

## Prerequisite refactoring: per-prunetable fill workers

The original code had a global array `fillworker fillworkers[MAXTHREADS]` that
all prunetable instances shared.  This would cause corruption if two prunetables
tried to fill concurrently.  The array was moved inside `prunetable` as a
`unique_ptr<fillworker[]>`, so each prunetable owns its fill workers.

---

## The multiphase solver

The new code lives in `src/cpp/multiphase.cpp` and `src/cpp/multiphase.h`.

### Interface

```cpp
struct phasespec {
    const char *movelist;       // comma-separated moves allowed in this phase
    const char *subgroupmoves;  // moves defining the target subgroup, or null
    ull maxmem;                 // memory budget for the pruning table
};

int multiphase_solve(const string &twsfile,
                     const vector<phasespec> &specs,
                     const setval &start, int totsize);
```

N `phasespec` entries produce N phases.  The invocation from the command line
(`--multiphase moveset`, repeatable) builds N+1 phases: an implicit phase 0
using all moves that targets the subgroup of the first moveset, then N−1
intermediate phases, and a final phase that targets the fully solved position.

### Building phases

Each phase gets its own `puzdef` built by re-reading the puzzle file, applying
the phase's move filter via `filtermovelist`, and (if the phase has a subgroup
target) running `runsubgroup` to relabel piece values into orbit indices.  The
pruning table is then constructed for that `puzdef`.

Phases are built sequentially before any solving threads start, because pruning
table construction is not thread-safe across independent instances.

### Pipeline structure

Each phase runs in a dedicated `std::thread` that owns:
- Its `puzdef` and `prunetable`.
- A condition-variable queue of `phasework` items.  Each item carries: a copy
  of the current puzzle position (in original piece-value space), the
  accumulated move count from all preceding phases, and the accumulated
  move-name string for output.
- A pointer to the next phase in the chain (null for the last phase).
- A pointer to a shared atomic integer `best_total`, initialised to INT_MAX.

Phase 0's queue is seeded with the scrambled starting position; its
`signal_input_done()` is called immediately after.  Each subsequent phase's
`signal_input_done()` is called when the preceding phase's worker thread exits.

### Per-phase solving

For each work item, the phase worker:

1. Checks whether there is any room left for improvement given `best_total` and
   the number of remaining phases; skips the item if not.
2. Converts the item's original-space position into the orbit-space that this
   phase's `pd.solved` uses (by applying `pd.mul(pd.solved, work.state,
   orbit_start)`).
3. Calls `solve()` with the phase's own `solveoptions`, using a `callback` and
   a `flushback` hook to control iteration.

The `flushback` hook stops the IDA* loop as soon as going one level deeper
cannot improve `best_total`.

The `callback` hook is called at every depth-limit leaf where the position
matches the phase's target (`pd.equivpos(pos, pd.solved)`).  For intermediate
phases it reconstructs the original-space position (by replaying `movehist`
against the item's original-space state), builds a new `phasework` item, and
enqueues it to the next phase — but only if `total + remaining_phases <
best_total`.  For the last phase it updates `best_total` under the global lock
and prints the complete solution.

### Orbit-space vs. original-space

After `runsubgroup`, a phase's `pd.solved.dat` contains orbit indices (0, 0, 0,
0, 1, 1, 1, 1 for a two-orbit puzzle) rather than the original piece values (0,
1, 2, …).  Positions flowing through the pipeline, however, are kept in
original piece-value space so they can be correctly hashed and looked up in each
phase's pruning table.  The conversion for the IDA* search is done once per
work item via `pd.mul(pd.solved, original_pos, orbit_pos)`, which maps each
piece's original value through `pd.solved` to get its orbit index.

### Thread allocation

The global thread pool (size `numthreads`) is divided evenly across phases.
Phase i gets threads `i*k .. (i+1)*k − 1` where k = `numthreads / n`.  The
last phase absorbs any remainder.  These slices are recorded in
`solveoptions::thread_base` and `solveoptions::thread_count` so that
`solve()` offsets its `spawn_thread(thread_base + i, …)` calls appropriately.

---

## Fix: concurrent fill-table thread-slot collision

`prunetable::filltable` fills the pruning table by spawning worker threads with
`spawn_thread(i, fillthreadworker, …)` and later joining them with
`join_thread(i)`.  Both functions index into a global `p_thread[]` array using
the bare index `i`.

Meanwhile `solve()` uses `spawn_thread(ctx_.thread_base + i, threadworker, …)`
— it offsets by `thread_base` so that concurrent solves in different phases use
non-overlapping slots.

`filltable` had no such offset.  When two phase workers running concurrently
both triggered a `checkextend` → `filltable` call, they would both call
`spawn_thread(0, …)`, with the second call overwriting `p_thread[0]` before the
first thread was joined.  Both phases would then join the same (second) thread,
leaking the first.

The fix adds two new fields to `prunetable`:

```cpp
int thread_base = 0;  // first p_thread[] slot for fill workers
int thread_count = 0; // number of threads (0 = all numthreads)
```

`filltable` now uses `spawn_thread(thread_base + i, …)` and
`join_thread(thread_base + i, …)`, and limits the worker count to `thread_count`
when nonzero.  `multiphase_solve` sets these fields from each phase's thread
slice immediately after constructing the prunetable.  Concurrent phases
therefore use disjoint slots in `p_thread[]` for both solving and filling,
eliminating the collision without any `no_checkextend` suppression.

---

## Command-line interface

The new `--multiphase moveset` option (repeatable) adds one moveset to the
phase pipeline.  N uses create N+1 phases.  It is paired with `--scramblealg`
(to give the scramble on the command line) or a scramble file.

Example — Kociemba-style two-phase solve of a 3×3×3 scramble:

```
twsearch --multiphase "U,D,R2,L2,F2,B2" --scramblealg "U R F" 3x3x3.tws
```

This builds:
- Phase 0: all moves, reduces to the G1 subgroup (positions reachable by
  U, D, R2, L2, F2, B2).
- Phase 1: G1 moves only, solves to the fully solved position.
