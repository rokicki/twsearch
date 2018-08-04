Search program for twisty puzzles.  Much like ksolve but, due to licensing issues on
that program, we have coded it completely from scratch.  We start from a
base of compatibility but do not guarantee it.

What is working so far:

* Parsing ksolve file
* God's algorithm with two bits per state

Things to do:

* God's algorithm with long long per state
* God's algorithm with > 64 bits per state
* Parse scramble file
* Solve scramble positions

Things to design:

* Multithreading
* Symmetry reduction

Things to consider:

* Ignore pieces
* Blocking moves
