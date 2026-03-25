#include "multiphase.h"
#include "canon.h"
#include "filtermoves.h"
#include "index.h"
#include "prunetable.h"
#include "readksolve.h"
#include "subgroup.h"
#include "threads.h"
#include "util.h"
#include <atomic>
#include <climits>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
using std::atomic;
using std::condition_variable;
using std::deque;
using std::ifstream;
using std::lock_guard;
using std::make_unique;
using std::mutex;
using std::string;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;

// ---------------------------------------------------------------------------
// Work item passed between phase queues.
// ---------------------------------------------------------------------------
struct phasework {
  vector<uchar> state;    // copy of pd.totsize bytes from the setval
  int depth_so_far;       // total moves committed by preceding phases
  string moves_so_far;    // accumulated move-name string for output
};

// ---------------------------------------------------------------------------
// Per-phase worker.  Owns its puzdef, prunetable, and input queue.
// ---------------------------------------------------------------------------
struct phaseworker {
  int phase_idx;
  int total_phases;
  puzdef pd;
  unique_ptr<prunetable> pt;
  solveoptions base_opts; // options template for this phase

  // Queue of work items (states to solve)
  mutex queue_mutex;
  condition_variable queue_cv;
  deque<phasework> work_queue;
  bool input_done = false;

  // Shared counters (written atomically by last phase)
  atomic<int> *best_total;
  atomic<int> *solutions_found;

  // Next phase in the pipeline (nullptr for last phase)
  phaseworker *next = nullptr;

  void enqueue(phasework pw) {
    {
      lock_guard<mutex> lk(queue_mutex);
      work_queue.push_back(std::move(pw));
    }
    queue_cv.notify_one();
  }

  void signal_input_done() {
    {
      lock_guard<mutex> lk(queue_mutex);
      input_done = true;
    }
    queue_cv.notify_all();
  }

  // Reset queue state between successive solves (prunetable is kept).
  void reset() {
    lock_guard<mutex> lk(queue_mutex);
    work_queue.clear();
    input_done = false;
  }

  // Thread body: pull work items and solve them until input is exhausted.
  void run();
};

void phaseworker::run() {
  bool is_last = (next == nullptr);
  int totsize = pd.totsize;

  while (true) {
    phasework work;
    {
      unique_lock<mutex> lk(queue_mutex);
      queue_cv.wait(lk,
                    [this] { return !work_queue.empty() || input_done; });
      if (work_queue.empty())
        break; // input_done and nothing left
      work = std::move(work_queue.front());
      work_queue.pop_front();
    }

    // Build a snapshot of options for this work item.
    solveoptions opts = base_opts;

    // Upper bound on depth for this phase: leave room for remaining phases.
    int remaining_phases = total_phases - phase_idx - 1;
    int bt = best_total->load(memory_order_relaxed);
    int room = bt - work.depth_so_far - remaining_phases;
    if (room <= 0)
      continue; // can't improve with this starting state
    opts.maxdepth = room;

    // Convert work.state (original-space, piece values 0..n-1) into this
    // phase's orbit-space so that comparepos(pos, pd.solved) works correctly.
    //
    // After runsubgroup, pd.solved uses orbit indices (not piece numbers).
    // The orbit-space start is: orbit_start[j] = pd.solved[ work.state[j] ]
    // which is exactly mul(pd.solved, work_state, orbit_start).
    // When the phase has no subgroupmoves, pd.solved == identity, so
    // orbit_start == work.state (no change).
    stacksetval orbit_start(pd, pd.solved);
    {
      setval ws;
      ws.dat = const_cast<uchar *>(work.state.data());
      pd.mul(pd.solved, ws, orbit_start);
    }

    // Count of solutions forwarded to the next phase (or improvements printed
    // for the last phase).  Non-last phases stop once this reaches the limit
    // set by base_opts.solutionsneeded (from the -c command-line option).
    // The last phase has no such cap and keeps searching for improvements.
    ll solutions_sent = 0;
    ll phase_limit = is_last ? (1LL << 30) : base_opts.solutionsneeded;

    // Callback: called by solve() at every depth-limit leaf.
    // We only act on it if the position actually matches the phase target.
    opts.callback = [this, &work, is_last, totsize, &solutions_sent,
                     phase_limit](setval &pos, const vector<int> &movehist,
                                  int d, int /*tid*/) -> int {
      // Filter: only proceed if we've reached the target state.
      if (!pd.equivpos(pos, pd.solved))
        return 0;
      int total = work.depth_so_far + d;

      // Build the move-name string for this phase's contribution.
      string phase_moves;
      for (int i = 0; i < d; i++) {
        if (i > 0)
          phase_moves += ' ';
        phase_moves += pd.moves[movehist[i]].name;
      }

      if (is_last) {
        // Update best_total and print if this is an improvement.
        int old = best_total->load(memory_order_relaxed);
        if (total < old) {
          get_global_lock();
          if (total < best_total->load(memory_order_relaxed)) {
            best_total->store(total, memory_order_relaxed);
            solutions_found->fetch_add(1, memory_order_relaxed);
            string full = work.moves_so_far;
            if (!full.empty() && !phase_moves.empty())
              full += ' ';
            full += phase_moves;
            cout << " " << full << endl << flush;
            solutions_sent++;
          }
          release_global_lock();
        }
      } else {
        // Forward up to phase_limit solutions to the next phase.
        if (solutions_sent < phase_limit) {
          int remaining_after = total_phases - phase_idx - 2;
          if (total + remaining_after < best_total->load(memory_order_relaxed)) {
            // Reconstruct the original-space position (piece values 0..n-1)
            // by re-applying the movehist to work.state.  This preserves the
            // full puzzle state so the next phase can hash it correctly.
            vector<uchar> orig(totsize), temp(totsize);
            memcpy(orig.data(), work.state.data(), totsize);
            for (int i = 0; i < d; i++) {
              setval sv, st;
              sv.dat = orig.data();
              st.dat = temp.data();
              pd.mul(sv, pd.moves[movehist[i]].pos, st);
              memcpy(orig.data(), temp.data(), totsize);
            }
            phasework pw;
            pw.state.assign(orig.begin(), orig.end());
            pw.depth_so_far = total;
            pw.moves_so_far = work.moves_so_far;
            if (!pw.moves_so_far.empty() && !phase_moves.empty())
              pw.moves_so_far += ' ';
            pw.moves_so_far += phase_moves;
            next->enqueue(std::move(pw));
            solutions_sent++;
          }
        }
      }
      return 0; // keep searching (flushback controls termination)
    };

    // Flushback: called after each depth level.
    // For non-last phases: stop once we've forwarded phase_limit solutions.
    // Always stop when going one level deeper can't improve best_total.
    opts.flushback = [this, &work, &solutions_sent, phase_limit,
                      is_last](int d) -> int {
      if (!is_last && solutions_sent >= phase_limit)
        return 1;
      int remaining_phases_after = total_phases - phase_idx - 1;
      return work.depth_so_far + d + 1 + remaining_phases_after >=
             best_total->load(memory_order_relaxed);
    };

    // Set solutionsneeded high so the callback, not satisfied(), controls
    // termination; alloptimal=0 so we move on after the first depth hit.
    opts.solutionsneeded = 1LL << 30;

    solve(pd, *pt, orbit_start, opts);
  }

  // No more work for this phase; tell the next phase.
  if (next)
    next->signal_input_done();
}

// ---------------------------------------------------------------------------
// Build a puzdef for one phase by reading the puzzle file and applying the
// per-phase move filter and subgroup transformation.
// ---------------------------------------------------------------------------
static puzdef build_phase_puzdef(const string &twsfile, const phasespec &spec,
                                 const string &basename, int phase_id) {
  ifstream f(twsfile);
  if (f.fail())
    error("! multiphase: could not open puzzle file ", twsfile.c_str());
  // Set inputbasename so the prunetable cache files are per-phase-distinct.
  // (runsubgroup and filtermovelist call addoptionssum to distinguish the hash.)
  inputbasename = basename;
  puzdef pd = readdef(&f);
  filtermovelist(pd, spec.movelist);
  if (spec.subgroupmoves)
    runsubgroup(pd, spec.subgroupmoves);
  if (pd.baserotations.size())
    calcrotations(pd, phase_id);
  calculatesizes(pd, phase_id);
  calclooseper(pd, phase_id);
  makecanonstates(pd, phase_id);
  return pd;
}

// ---------------------------------------------------------------------------
// Public entry point.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// multiphase_state: holds pre-built phases for repeated solving.
// ---------------------------------------------------------------------------
struct multiphase_state {
  vector<unique_ptr<phaseworker>> phases;
  atomic<int> best_total{INT_MAX};
  atomic<int> solutions_found{0};
  int totsize = 0;
};

multiphase_state *multiphase_prepare(const string &twsfile,
                                     const vector<phasespec> &specs) {
  int n = (int)specs.size();
  if (n == 0)
    return nullptr;

  // Derive a base name from the puzzle file path (strip dirs and extension).
  string basename;
  {
    int sawdot = 0;
    for (char c : twsfile) {
      if (c == '.')
        sawdot = 1;
      else if (c == '/' || c == '\\') {
        sawdot = 0;
        basename.clear();
      } else if (!sawdot)
        basename.push_back(c);
    }
  }

  auto *st = new multiphase_state();
  st->phases.reserve(n);

  // Divide threads evenly across phases.
  int threads_per_phase = max(1, numthreads / n);

  // Build all phases sequentially (prunetable construction is not thread-safe
  // across independent instances; build one at a time, run concurrently).
  for (int i = 0; i < n; i++) {
    auto pw = make_unique<phaseworker>();
    pw->phase_idx = i;
    pw->total_phases = n;
    pw->best_total = &st->best_total;
    pw->solutions_found = &st->solutions_found;
    pw->pd = build_phase_puzdef(twsfile, specs[i], basename, i);

    // Thread-pool slice for this phase.
    int tbase = i * threads_per_phase;
    int tcount = (i == n - 1) ? max(1, numthreads - tbase)
                               : threads_per_phase;
    pw->base_opts.thread_base = tbase;
    pw->base_opts.thread_count = tcount;
    pw->base_opts.noearlysolutions = 1;
    pw->base_opts.solutionsneeded = g_opts.solutionsneeded;
    pw->base_opts.nodedupe = (i < n - 1) ? 1 : g_opts.nodedupe;
    pw->base_opts.phase_id = i;

    ull mem = specs[i].maxmem;
    pw->pt = make_unique<prunetable>(pw->pd, mem, i);
    pw->pt->thread_base = tbase;
    pw->pt->thread_count = tcount;

    st->phases.push_back(std::move(pw));
  }

  // Link the chain.
  for (int i = 0; i < n - 1; i++)
    st->phases[i]->next = st->phases[i + 1].get();

  return st;
}

int multiphase_solve_one(multiphase_state *st, const setval &start,
                         int totsize) {
  if (!st || st->phases.empty())
    return -1;
  int n = (int)st->phases.size();

  // Reset state for this solve.
  st->best_total.store(INT_MAX, memory_order_relaxed);
  st->solutions_found.store(0, memory_order_relaxed);
  for (auto &pw : st->phases)
    pw->reset();

  if (quiet == 0)
    cout << "Solving" << endl << flush;
  double starttime = walltime();

  // Start each phase's worker thread.
  vector<thread> threads;
  threads.reserve(n);
  for (int i = 0; i < n; i++)
    threads.emplace_back([&st, i] { st->phases[i]->run(); });

  // Enqueue the starting position into phase 0 and mark its input done.
  {
    phasework pw;
    pw.state.assign(start.dat, start.dat + totsize);
    pw.depth_so_far = 0;
    st->phases[0]->enqueue(std::move(pw));
  }
  st->phases[0]->signal_input_done();

  // Wait for all phases to finish.
  for (auto &t : threads)
    t.join();

  int bt = st->best_total.load();
  int sf = st->solutions_found.load();
  if (quiet == 0) {
    double elapsed = walltime() - starttime;
    if (bt == INT_MAX)
      cout << "No solution found in " << elapsed << endl << flush;
    else
      cout << "Found " << sf << " solution" << (sf != 1 ? "s" : "")
           << " max depth " << bt << " in " << elapsed << endl << flush;
  }
  return (bt == INT_MAX) ? -1 : bt;
}

void multiphase_destroy(multiphase_state *st) { delete st; }

int multiphase_solve(const string &twsfile, const vector<phasespec> &specs,
                     const setval &start, int totsize) {
  multiphase_state *st = multiphase_prepare(twsfile, specs);
  if (!st)
    return -1;
  int result = multiphase_solve_one(st, start, totsize);
  multiphase_destroy(st);
  return result;
}
