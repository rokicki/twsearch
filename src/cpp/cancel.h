#ifndef CANCEL_H
#include <istream>
/*
 *   Cancelling a solve.
 *
 *   A controlling program (the twsearch bridge, or the JavaScript worker
 *   driving the WebAssembly build) can ask twsearch to abandon the scramble
 *   it is solving.  The search stops, prints "Search canceled at depth d" in
 *   place of the usual "Found ..." or "No solution found in ..." line, and
 *   twsearch goes on to the next scramble with its pruning table intact.
 *
 *   Natively, this works when scrambles are read from standard input (a
 *   scramble file name of "-").  A reader thread takes over standard input;
 *   a line consisting of just
 *
 *      !cancel
 *
 *   cancels the most recent scramble that has begun to arrive, and every
 *   other line is passed on to the scramble reader.  Because the reader
 *   thread numbers scrambles as it sees them, a cancel that arrives after a
 *   solve has finished cannot cancel the next scramble, and a cancel sent
 *   right after a scramble cannot be lost while that scramble is parsed.
 *
 *   Under WASM the search cannot see new input while it runs, so the
 *   embedding JavaScript sets Module.twsearchCanceled = true instead.  The
 *   search polls it through searchcanceled(), yielding to the JavaScript
 *   event loop (via Asyncify) every 50ms or so, so that messages asking for
 *   the cancel can be delivered.  The flag is cleared as each scramble
 *   begins.
 *
 *   Filling a level of the pruning table cannot be interrupted without
 *   corrupting the table, so a cancel during a fill takes effect when that
 *   level is done.
 */
// Returns a stream of standard input with cancel lines removed (native).
std::istream *cancelablestdin();
// Called as each scramble begins to be solved.  Weak: a build where cancels
// arrive some other way can define its own (see cancel.cpp).
void beginscramble();
// Nonzero if the scramble being solved has been canceled.  Weak, as above.
int searchcanceled();
#define CANCEL_H
#endif
