# `twsearch`

Search program for twisty puzzles. Much like [KSolve](https://github.com/cubing/ksolve) but, due to licensing issues on that program, we have coded it completely from scratch  We start from a
base of compatibility but do not guarantee it.

## Running `twsearch`

If you have a C++ toolchain on your computer, you can run:

```shell
# Check out and build the binary
git clone https://github.com/cubing/twsearch && cd twsearch
make build

# Run a search
./build/bin/twsearch samples/main/3x3x3.tws samples/main/tperm.scr
```

On Windows, you can use the GCC toolchain with glibc to build it.  We don't
support MSVC as a standard build platform, but the following command with
the 64-bit MSVC compiler will give an executable that works in single-threaded
mode:

```shell
cl /o twsearch.exe /EHsc src\cpp\*.cpp src\cpp\cityhash\src\city.cc
```

### Usage

Important options (you likely want to specify these):

   `-M` *#* megabytes of memory to use max; should be ~ 1/2 of your RAM; defaults to 8192 (8GB).

   `-t` *#*  number of threads to use; defaults to the number of threads your CPU makes available.

   `--nowrite`  don't write pruning tables to disk (regenerate in memory each time).

Sample usage:

```shell
./build/bin/twsearch samples/main/3x3x3.tws samples/main/tperm.scr

./build/bin/twsearch samples/symm/3x3x3.tws samples/main/tperm.scr

./build/bin/twsearch -g samples/main/2x2x2.tws

./build/bin/twsearch -c 20 --moves 2L,2R,U,F samples/main/4x4x4.tws samples/main/flip.scr

./build/bin/twsearch --moves F,R,D,B,L --scramblealg U samples/main/3x3x3.tws

./build/bin/twsearch --moves U,R,F -q -g samples/main/kilominx.tws
```

The maximum memory setting should be used carefully; on a machine running
Windows or OS-X with heavy browser usage and other programs, you may want
to set it to only one quarter of your physical RAM.  On a dedicated Linux
or BSD machine, you can probably set it to 90% of your physical RAM.
The memory size you set also sets the size of the pruning tables that are
written to disk.  Although they are compressed, the compression can
vary from 1.1X to more than 30X, but in general the longer the pruning
table takes to generate, the worse the compression.  All pruning tables
are written with extensions of .dat, so you might want to clean them
up occasionally if you start to run out of disk space.

For a full list of options, just execute `build/bin/twsearch` with no
arguments.  These are the options as of this writing:

Options:

`-A`  Try to find useful algorithms for a given puzzle.  We look for
   algorithms that affect few pieces.  The -A option can be immediately
   followed by s to mean strict (only print one solution of a given length
   with a given signature), 1 to mean basic algo search, 2 to mean
   find algos by repeated executions, and 3 to mean find commutators.

`-a` *num*  Set the number of antipodes to print.  The default is 20.

`--alloptimal`  Find all optimal solutions.

`-C`  Show canonical sequence counts.  The option can be followed
   immediately by a number of levels (e.g., -C20).

`-c` *num*  Number of solutions to generate.

`--cachedir` *dirname*  Use the specified directory to cache pruning tables.

`--cancelseqs`  Read a set of move sequences on standard input and merge any
   nearly adjacent moves according to canonical sequences.  This does not
   reorder moves so the result is canonical; it just cancels moves.

`--checkbeforesolve`  Check each position for solvability using generating
   set before attempting to solve.

`--compact`  Print and parse positions on standard input and output
    in a one-line compact format.

`--describesets`  Print a table of what moves affect what pieces.

`--distinguishall`  Override distinguishable pieces (use the superpuzzle).

`-F`  When running God's number searches, force the use of arrays and
   sorting rather than canonical sequences or bit arrays.

`-g`  Calculate the number of positions at each depth, as far as memory
   allows.  Print antipodal positions.

`-H`  Use 128-bit hash instead of full state for God's number searches.

`-i`  Read a set of move sequences on standard input and echo the
   inverted sequences.

`-M` *num*  Set maximum memory use in megabytes.

`--maxdepth` *num*  Maximum depth for searches.

`--maxwrong` *num*  Read a set of move sequences on standard input and for each,
   if the number of wrong pieces is less than or equal to the integer
   given, echo the number of wrong pieces and the input sequence.

`--mergeseqs`  Read a set of move sequences on standard input and merge any
   nearly adjacent moves according to canonical sequences.  This also
   reorders moves so the end result is a canonical sequence.

`--microthreads` *num*  Use this many microthreads on each thread.

`--mindepth` *num*  Minimum depth for searches.

`--moves` *moves*  Restrict search to the given moves.

`--newcanon` *num*  Use search-based canonical sequences to the given depth.

`--nocenters`  Omit any puzzle sets with recognizable center names.

`--nocorners`  Omit any puzzle sets with recognizable corner names.

`--noearlysolutions`  Emit any solutions whose prefix is also a solution.

`--noedges`  Omit any puzzle sets with recognizable edge names.

`--noorientation`  Ignore orientations for all sets.

`--nowrite`  Do not write pruning tables.

`-o`  Read a set of move sequences on standard input and show the
   order of each.

`--omit` *setname*  Omit the following set name from the puzzle.  You can provide
   as many separate omit options, each with a separate set name, as you want.

`--ordertree`  Print shortest sequences of a particular order of the superpuzzle.

`--orientationgroup` *num*  Treat adjacent piece groups of this size as
   orientations.

`-q`  Use only minimal (quarter) turns.

`--quiet`  Eliminate extraneous output.

`-R` *num*  Seed for random number generator.

`-r` *num*  Show num random positions.  The positions are generated by
   doing 500 random moves, so for big puzzles they might not be very random.

`--randomstart`  Randomize move order when solving.

`-S`  Test solves by doing increasingly long random sequences.
   An integer argument can be provided appended to the S (as in -S5) to
   indicate the number of random moves to apply at each step.

`-s`  Read a set of move sequences on standard input and perform an
   optimal solve on each.  If the option is given as -si, only look for
   improvements in total solution length.

`--schreiersims`  Run the Schreier-Sims algorithm to calculate the state
   space size of the puzzle.

`--scramblealg` *moveseq*  Give a scramble as a sequence of moves on the
   command line.

`--shortenseqs`  Read a set of move sequences on standard input and attempt
   to shorten each by optimally solving increasingly longer subsequences.

`--showmoves`  Read a set of move sequences on standard input and show the
   equivalent move definition on standard output.

`--showpositions`  Read a set of move sequences on standard input and show the
   resulting position on standard output.

`--showsymmetry`  Read a set of move sequences on standard input and show the
   symmetry order of each.

`--startprunedepth` *num*  Initial depth for pruning tables (default is 3).

`-T`  Run microbenchmark tests.

`-t` *num*  Use this many threads.

`-U`  Read a set of move sequences on standard input and only echo
   those that are unique with respect to symmetry.  If an integer is
   attached to the -U option, exit after that many unique sequences have
   been seen.

`-u`  Read a set of move sequences on standard input and only echo
   those that are unique.  If an integer is attacheck to the -u option,
   exit after that many unique sequences have been seen.

`--unrotateseqs`  Read a set of move sequences on standard input and attempt
   to move all rotations to the end of the sequence.

`-v`  Increase verbosity level.  If followed immediately by a digit, set
   that verbosity level.

`--writeprunetables` *never|auto|always*  Specify when or if pruning tables
   should be written  The default is auto, which writes only when the program
   thinks the pruning table will be faster to read than to regenerate.

## Pruning Tables

The pruning tables are written to a system-dependent cache directory.
On Unix, the default system directory is `~/.cache/`, and on Apple
platforms the default system directory is `~/Library/Caches/`; on
both of these platforms the default can be overrriden by setting the
`XDG_CACHE_HOME` environment variable.
On Windows, the default location is obtained from the `LOCALAPPDATA`
environment variable.

Unless a directory is explicitly specified with the `--cachedir` option,
a `twsearch` subdirectory will be created for the pruning tables.

In environment values and in the `--cachedir` option, a leading tilde
will be expanded with the contents of the `HOME` environment variable.

## Status

What is working so far:

- Parsing ksolve file
- God's algorithm
- Optimal solver for random positions
- Canonical sequence data
- Tree search using canonical sequences
- Write pruning tables
- Read pruning tables
- Parse scramble file
- Solve scramble positions
- QTM solves/pruning tables
- Symmetry reduction (except mirroring)

Things to do:

- Add algebraic support for when reading scrambles
- Add grip information; derive moves according to SiGN
- Print antipodes on two-bit God's algorithm
- Coset solvers

Things to consider:

- Ignore pieces
- Blocking moves

## License

This work is dual-licensed under the Mozilla Public License 2.0 and GPL 3.0 (or
any later version). If you use this work, you can choose either (or both) license terms to adhere to.

`SPDX-License-Identifier: MPL-2.0 OR GPL-3.0-or-later`
