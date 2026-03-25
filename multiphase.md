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

The `flushback` hook stops the IDA* loop as soon as:
- a non-last phase has forwarded its solution quota (see below), or
- going one level deeper cannot improve `best_total`.

The `callback` hook is called at every depth-limit leaf where the position
matches the phase's target (`pd.equivpos(pos, pd.solved)`).  For intermediate
phases it reconstructs the original-space position (by replaying `movehist`
against the item's original-space state), builds a new `phasework` item, and
enqueues it to the next phase — but only if `total + remaining_phases <
best_total`.  For the last phase it updates `best_total` under the global lock
and prints the complete solution.

### Solution quota per phase (`-c` option)

Each intermediate phase maintains a `solutions_sent` counter.  It stops
forwarding work to the next phase once `solutions_sent` reaches
`base_opts.solutionsneeded`, which is copied from `g_opts.solutionsneeded` (set
by the `-c` command-line option, default 1).  The flushback returns immediately
once the quota is met.

With `-c 1` (the default) each intermediate phase forwards exactly one candidate
to the next — the shortest IDA* solution it finds.  This produces behaviour
equivalent to the sequential shell script `mmsolve.pl` and runs in roughly the
same time (~1.7 s for the nine-phase megaminx example vs 1.2 s sequentially).

With `-c N` for N > 1, each intermediate phase forwards up to N candidates
before stopping, giving later phases more paths to explore and potentially
finding a shorter total solution at the cost of additional search time.

The last phase is never capped: it keeps searching for improvements until the
standard depth bound (`depth_so_far + d + 1 >= best_total`) prevents progress.

### `noearlysolutions` set on all phases

Each phase's `base_opts.noearlysolutions` is forced to 1.  This suppresses
sub-optimal solutions at the current IDA* depth level, keeping the pipeline
clean: only minimum-depth solutions for each phase are forwarded or printed.

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

## Fix: memory budget per phase

The original `cmdlineops.cpp` allocated `maxmem / nphases` bytes to each
phase's pruning table, giving each of N phases only 1/N of the available
memory.  With nine megaminx phases this left each table with roughly 111 MB
instead of 1 GB, producing extremely shallow pruning tables and very slow IDA*
searches.

The fix is simple: each phase now receives the full `maxmem` budget.  Pruning
tables for different phases are independent objects (each `phaseworker` owns
one), so they do not compete for the same allocation.

---

## Fix: negative thread count for last phase

With more phases than threads (e.g. 9 phases, 1 thread), the expression
`numthreads - i * threads_per_phase` for the last phase could go negative,
producing a negative `thread_count`.  Both `solve()` and `filltable()` use
`min(wthreads, thread_count)`, so a negative value caused an empty work vector
and an immediate crash when the worker tried to index `solveparams[0]`.

The fix clamps the last phase's count: `max(1, numthreads - tbase)`.

---

## Fix: null-pointer dereference setting `pt->phase_id` before construction

In the phase-building loop, `pw->pt->phase_id = i` was written before
`pw->pt` was constructed via `make_unique<prunetable>(...)`.  The assignment
dereferenced a null `unique_ptr`, causing a SIGSEGV caught by AddressSanitizer
as a UBSan "member access within null pointer" error.

The fix moves the assignment to after the `make_unique` call.  AddressSanitizer
(`make BUILD=asan`) is now wired into the build system (see below) and was used
to diagnose this bug.

---

## Build system: `BUILD=asan` variant

The Makefile now supports two build variants selected by the `BUILD` variable:

```
make              # release build → build/bin/twsearch   (-O3)
make BUILD=asan   # sanitized build → build-asan/bin/twsearch (-O1 -fsanitize=address,undefined)
```

All object files and the final binary go into `build/` or `build-asan/`
respectively, so both variants can coexist without a clean.  `make clean`
removes only the directory for the current `BUILD` variant.

---

## Output: phase-prefixed log lines

When running a multiphase solve, output lines from different phases are
interleaved.  To make the output readable, every informational `cout` line that
is gated on `quiet == 0` is now prefixed with `"Phase N: "` when it is emitted
in the context of a multiphase phase.

A pure helper function `log_prefix(int phase_id)` in `util.h`/`util.cpp`
returns `"Phase N: "` when `phase_id >= 0` and `""` otherwise.  Each output
call site receives `phase_id` explicitly — there is no global or thread-local
state — so concurrent phases running in separate threads each carry their own
identifier independently.

The phase identifier is threaded through:

| Site | How |
|---|---|
| `calculatesizes()` | new `phase_id` parameter (default -1) |
| `calclooseper()` | new `phase_id` parameter (default -1) |
| `makecanonstates()` | new `phase_id` parameter (default -1) |
| `calcrotations()` | new `phase_id` parameter (default -1) |
| `prunetable` constructor and `filltable()` | `phase_id` field set at construction |
| `solve()` Depth lines | `solveoptions::phase_id` field |

`multiphase_solve()` passes the loop index `i` to `build_phase_puzdef()`,
which forwards it to all four setup functions, and also passes it to the
`prunetable` constructor and sets `base_opts.phase_id`.

Lines covered: `Rotation group size`, `State size`, `Requiring`, `Found N
canonical move states`, `For memsize`, `Trying to allocate`, `Initializing
memory`, `Filling depth`, `Filled depth`, `Depth`.

The `Solved`/`EDGE`/…/`End` block emitted by `runsubgroup` after orbit
relabelling is now suppressed unless `verbose > 1` (it was previously shown at
`verbose > 0`).

---

## Command-line interface

The new `--multiphase moveset` option (repeatable) adds one moveset to the
phase pipeline.  N uses create N+1 phases.  It is paired with `--scramblealg`
(to give the scramble on the command line) or a scramble file.

The `-c N` option (solutions needed) controls how many candidate states each
intermediate phase forwards to the next.  The default of 1 gives fast greedy
behaviour; higher values trade time for solution quality.

Example — Kociemba-style two-phase solve of a 3×3×3 scramble:

```
twsearch --multiphase "U,D,R2,L2,F2,B2" --scramblealg "U R F" 3x3x3.tws
```

This builds:
- Phase 0: all moves, reduces to the G1 subgroup (positions reachable by
  U, D, R2, L2, F2, B2).
- Phase 1: G1 moves only, solves to the fully solved position.

Example — nine-phase megaminx solve with up to 5 candidates per phase:

```
twsearch -M 1000 --nowrite -c 5 \
  --multiphase R,U,F,L,BR,BL,FR,FL,DR \
  --multiphase R,U,F,L,BR,BL,FR,FL \
  --multiphase R,U,F,L,BR,BL,FR \
  --multiphase R,U,F,L,BR,BL \
  --multiphase R,U,F,L,BR \
  --multiphase R,U,F,L \
  --multiphase R,U,F \
  --multiphase R,U \
  --scramblealg "..." megaminx.tws
```
