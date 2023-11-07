The core code of twsearch is as follows:

puzdef.h contains definitions for the puzzle, including moves and states.

readksolve.h contains functions to read the ksolve-style text file (usually
given the extension .tws) and create a puzzle definition file.

parsemoves.h contains code to parse move lists.

filtermoves.h contains some more specific code to take the set of move
restrictions given on the command line and combine it with the move lists
in the puzzle definition to give the actual active move list.

index.h contains code to turn permuations or orientations into indices and
back again.

solve.h contains the main search code to solve positions.

prunetable.h contains code to create and maintain pruning tables.

canon.h contains code to eliminate redundancies in search sequences.

twsearch.h is the main program that parses options and invokes the
various other components of the code.

Most of the rest of the code is for additional features and performance.

threads.h contains code to manage threading and synchronization.

workchunks.h contains code to partition a search tree across threads and
microthreads and eliminate redundant search.

calcsymm.h contains code to calculate the symmetry group of the rotations
of a puzzle.

rotations.h contains code to find representatives for symmetrical
state classes.

Then, we have code that performs specific functions that are not just
turning states into move sequences.

god.h contains code for doing God's algorithm searches in various ways.
For larger puzzles this will calculate the number of positions at
specific distances, until memory is exhausted.  This is the -g option
of twsearch.

antipode.h contains code for storing and printing antipodes from God's
number searches.

generatingset.h contains an implementation of the Schreier-Sims algorithm.
This is used to check if a particular state actually can be solved with
a given move set, or to calculate the actual state size of a particular
puzzle.

findalgo.h searches for useful algorithms on a puzzle, usually ones
that only move a few pieces; it implements the -A option of twsearch.

descsets.h takes a puzzle definition and prints out a description of
what moves move what pieces.  This can be used to help understand a
particular .tws file.  It implements the --describesets functionality.

cmdlineops.h implements a bunch of the streaming features of twsearch,
such as -s (solve a bunch of positions), -i (invert a bunch of sequences),
-u (uniqify a set of sequences), and so on.

shorten.h attempts to find a shorter move sequence that has the same
effect as an input move sequence; it works by optimally solving
increasingly long subsequences of the input move sequence, and when
it finds better subsequences it plugs them in and iterates the
process.  This implements the --shortenseqs command line option.

ordertree.h contains generic code for walking a search tree; it can be 
treated as a template for specialized tasks.

test.h contains some microbenchmarking code tied to the -T option.

Then we have some code that is not fully functional yet, or does things
that are still a bit experimental.

coset.h contains some preliminary code for doing coset searches; it
probably needs to be rewritten from scratch.

unrotate.h is some initial work to move rotations in a move sequence
to the end of that move sequence; this code is not fully functional yet.

orderedsgs.h uses the schreier-sims algorithm (reimplemented here) to
calculate subgroup sizes based on subsets that fix pieces.  It was used
to help calculate bases and strong generating sets of specific puzzles.

Finally, we have some utility code that is of general use.

city.h refers to the main state hashing code we use.

util.h is a generic utility file for odds and ends that don't really fit
anywhere else.
