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
 *   Run an N-phase solve.
 *
 *   twsfile — path to the .tws puzzle-definition file (read once per phase).
 *   specs   — one entry per phase; phase 0 gets the full scramble, phase N-1
 *             produces the final move sequence.
 *   start   — the scrambled position expressed in specs[0]'s puzzle encoding.
 *   totsize — pd.totsize for start (needed to copy the position bytes).
 *
 *   Returns the total move count of the best complete solution found,
 *   or -1 if no solution was found.
 *
 *   Solutions are printed to cout as they are found (shortest first).
 */
int multiphase_solve(const std::string &twsfile,
                     const std::vector<phasespec> &specs, const setval &start,
                     int totsize);
#define MULTIPHASE_H
#endif
