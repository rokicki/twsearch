#ifndef MULTIPHASE_H
#include "puzdef.h"
#include "solve.h"
#include <string>
#include <vector>
/*
 *   N-phase solver — generalises Kociemba's two-phase algorithm to an
 *   arbitrary sequence of phases with independent move sets and subgroup
 *   targets (matching the Perl script mmsolve.pl).
 *
 *   All phases run concurrently in separate threads.  Phase k sends its
 *   solutions (as positions + accumulated depth) to phase k+1 via a
 *   condition-variable queue.  A shared best_total causes earlier phases
 *   to prune search via the flushback hook as soon as the last phase
 *   finds a complete solution.
 */

/*
 *   One element of the phase specification.
 *   movelist      — comma-separated move names allowed in this phase.
 *                   nullptr means all moves from the puzzle file.
 *   subgroupmoves — comma-separated moves that define the subgroup that
 *                   this phase must reduce the position into.  nullptr
 *                   means the standard solved position.
 *   maxmem        — memory budget for this phase's pruning table.
 */
struct phasespec {
  const char *movelist;
  const char *subgroupmoves;
  ull maxmem;
};

/*
 *   Opaque handle to a set of pre-built phases (prunetables + puzdef).
 *   Build once with multiphase_prepare(), solve many positions with
 *   multiphase_solve_one(), then free with multiphase_destroy().
 */
struct multiphase_state;

/*
 *   Build all phase prunetables.  Expensive — call once, reuse for many
 *   positions.  Returns a heap-allocated handle; caller must eventually
 *   pass it to multiphase_destroy().
 */
multiphase_state *multiphase_prepare(const std::string &twsfile,
                                     const std::vector<phasespec> &specs);

/*
 *   Solve one position using pre-built phases.  Thread-safe with respect
 *   to the prunetables (each call re-spawns the pipeline worker threads).
 *   Returns the best total move count, or -1 if no solution was found.
 */
int multiphase_solve_one(multiphase_state *st, const setval &start,
                         int totsize);

/*
 *   Free a handle returned by multiphase_prepare().
 */
void multiphase_destroy(multiphase_state *st);

/*
 *   Convenience: prepare + solve_one + destroy.  Equivalent to the old API.
 */
int multiphase_solve(const std::string &twsfile,
                     const std::vector<phasespec> &specs, const setval &start,
                     int totsize);
#define MULTIPHASE_H
#endif
