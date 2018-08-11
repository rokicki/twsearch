Search program for twisty puzzles.  Much like ksolve but, due to licensing issues on
that program, we have coded it completely from scratch.  We start from a
base of compatibility but do not guarantee it.

What is working so far:

* Parsing ksolve file
* God's algorithm
* Optimal solver for random positions
* Canonical sequence data
* Tree search using canonical sequences
* Write pruning tables

Things to do:

* Read pruning tables
* Print antipodes on two-bit God's algorithm
* If table size too large, don't degenerate (i.e., 2a.ksolve)
* Parse scramble file
* Solve scramble positions
* Coset solvers

Things to do

* Multithreading
* Symmetry reduction

Things to consider:

* Ignore pieces
* Blocking moves
