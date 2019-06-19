Search program for twisty puzzles.  Much like ksolve but, due to licensing issues on
that program, we have coded it completely from scratch.  We start from a
base of compatibility but do not guarantee it.

Important options (you should specify these):

   -M xxx:  megabytes of memory to use max; should be ~ 1/2 of your RAM and ideally a power of two; defaults to 8192 (8GB).

   -t xxx:  number of threads to use; defaults to 4.

Sample usage:

   ./twsearch samples/3x3x3.tws samples/tperm.scr

   ./twsearch -g samples/2x2x2.tws

   ./twsearch -c 20 --moves 2L,2R,U,F samples/4x4x4.tws samples/flip.tws

   ./twsearch --moves F,R,D,B,L --scramblealg U samples/3x3x3.tws

   ./twsearch --moves U,R,F -q -g samples/kilominx.tws

What is working so far:

* Parsing ksolve file
* God's algorithm
* Optimal solver for random positions
* Canonical sequence data
* Tree search using canonical sequences
* Write pruning tables
* Read pruning tables
* Parse scramble file
* Solve scramble positions
* QTM solves/pruning tables

Things to do:

* Add algebraic support for when reading scrambles
* Symmetry reduction
* Add grip information; derive moves according to SiGN
* Print antipodes on two-bit God's algorithm
* Coset solvers

Things to consider:

* Ignore pieces
* Blocking moves
