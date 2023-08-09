The twsearch program is my attempt to solve, generate algorithms
for, and otherwise explore twisty puzzles such as the ones generated
by the puzzle geometry component of cubing.js.  This document
describes some of the architectural decisions and implementation
details.

Representing Moves and States:  ksolve vs twsearch

The program originated from my work with ksolve.  When I realized
that the licensing for ksolve was unclear, I started completely
from scratch, using nothing from the ksolve source.  But I did want
to be able to have ksolve compatibility so users of that program
could simply switch to mine with no effort.

This may have been an unfortunate decision, as the ksolve input
format is unnecessarily complicated and verbose.  In particular,
the syntax used for moves and positions is identical, but the
semantics are completely different with respect to orientations.
I can find no trace of why this was done this way, but for compatibility
twsearch also does this.

We start by discussing the puzzle representation used for ksolve
move definitions and positions, internal representation, and
alternatives.  We will start by teasing out the semantics of just
the permutations, and follow this up with the necessary description
of the orientations.

Permutations are well understood, with standard notations that are
well agreed upon.  We will assume the left-to-right application
order, so (p1 * p2) means apply permutation p1 first, then permutation
p2.  Further, we permit this to be written as simply p1 p2 (as we
normally do with puzzle sequence notation).

We will first discuss writing permutations on S_n as a list of n
objects (without parentheses), and later discuss the cycle notation.

For this discussion we will use the standard convention permuting
the set of numbers 1..n.  Internally we will probably instead use
the set of numbers 0..n-1, to match the convention in most programming
languages of 0-based arrays, and all input and output will be
appropriately transformed.

A permutation can be written either as a transformation (the active
form), or as a ordered sequence (the passive form).

In the active form, a permutation written as 2 3 1 4 means that the
item currently in the first slot is moved to the second position
(so sigma(1)=2)), and an item in the second position is moved to
the third position (so sigma(2)=3), and an item in the third position
is moved to the first position (so sigma(3)=1), and the item in the
fourth position is left alone.  Another way to interpret this is
that the item that used to be in the first position is in position
two, the item that used to be in position two is in position three,
and the item that used to be in position three is in position one.
When interpreting such a permutation as a position, the permutation
maps elements to slots.

In the passive form, a permutation written as 2 3 1 4 means that
the second item is currently in the first position, the third item
is currently in the second position, the first item is currently
in the third position, and the fourth item is in the fourth position.
To interpret this as a permutation, this says move the element in
the 2nd slot to the first position, and so on.  When interpreting
such a permutation as a position, the permutation maps slots to
elements, and as such is typically used for positions.

These representations are directly isomorphic; if you invert a
passive permutation, you get the active representation of the
original permutation, and vice versa.  Inversion can be performed
by the following code:

   for (i in 1..n) inv[p[i]] = i ;

So it doesn't matter if we use passive or active representations,
as long as we are clear in what we use.

So what is the difference?  Consider how we multiply two permutations.
If we are using the active representation and left-to-right application
order, then sigma_p1(a)=b means piece a moves to position b, and
then sigma_p2(b)=c means that it moves further to c, so sigma_(p1*p2)(a)
= sigma_p2(sigma_p1(a)).  Code to multiply two permutations looks
like this:

   for (i in 1..n) p3[i] = p2[p1[i]]

But if we are using the passive representation, with left-to-right
application order, then sigma_p1(a)=b means the piece that was in
b moves to a, and sigma_p2(c)=a means that the piece that was in a
moves to c, so sigma_(p1*p2)(a) = sigma_p1(sigma_p2(a)), or in code:

   for (i in 1..n) p3[i]=p1[p2[i]]

These are very different multiplication methods; getting multiplication
correct is fundamental.

In ksolve, according to the documentation, both moves and positions
are given as active permutations.  This is the standard in mathematics,
as well.  The quote from the readme is as follows:

"A permutation vector is a list of the numbers from 1 to n in some
order, such as 231456. This describes where each piece goes - for
instance, the 2 in the first spot means that piece number 1 is in
spot 2."

So this says that ksolve is using the element to slot notation,
which is the active form.  Yet, if you look at the 4x4 centers
definition file distributed with ksolve, the permutation vector for
the solved position is given as

    1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5 6 6 6 6

and this cannot be an active permutation since it would try to cram
multiple elements into the same place.  This has to be a passive
permutation, mapping slot to element, with some identical elements.
Further, if you use the standard 3x3 def file, which has the solved
position for corners as 12345678, and the U move as 41235678 (the
corner orders are given as URF, ULF, ULB, URB, DRF, DLF, DLB, DRB),
then the U move says that the 4 (URB) in the first (URF) spot means
that piece 1 (URF) is in slot 4 (URB)".  But this is absolutely not
what the U move does; the U move puts the piece 4 (URB) in slot 1
(URF).  So ksolve is clearly using passive format throughout.

If you look at the ksolve code, in the move routine and in the merge
moves routine (which are both effectively multiplication, but
actually differ in terms of orientation, which we will be getting
to), the actual code used is:

   for (i in 1..n) p3[i] = p1[p2[i]]

This matches the passive mode multiplication procedure we derived above.

In group theory, with no identical pieces, there is no a-priori
difference between a move and a position; they are both simply a
permutation.  One can think of a move as the position obtained after
making that move from the identity position; similarly, one can
think of a position as the composite move required to obtain that
position from the identity.

So far this agrees with the tests I've performed on ksolve as well
as its code, though it contradicts its documentation.  It's also
the most logical code; if we need to support identical pieces, our
positions must be passive, not active, and if our positions are
passive, it's easiest if our moves are passive as well.  This also
means that the move descriptions are the same as descriptions of
positions after the move, assuming the solved state is the identity
permutation, which makes writing them fairly easy.  Further, the
permutation vector in the solved state doesn't matter at all, except
in that it allows the definition of identical pieces.

Let's move on to orientations.

Orientations are just a concise way of specifying larger permutations.
For instance, on the 3x3x3 cube, there are 54 stickers, 9 on each
face; if you assume the centers do not move, then there are only
48.  When describing this as a group to software such as GAP or
Mathematica, you would describe it as a group generated by elements
of S_48 or S_54, and be done.  But there are good reasons to separate
the permutation into orbits (what ksolve calls sets) and further
to rewrite some of the orbits in terms of smaller permutations and
orientations rather than a larger permutation.

Each orientation value expands a single permutation element in the
small permutation to several in the larger permutation element.
These elements collectively move together, potentially cycling due
to rotations in three-dimensional space.  So instead of having 24
stickers moving on eight cubies, we can instead have the eight
cubies moving, and maintain how far each cubie is rotated against
some standard configuration.  This standard configuration involves
marking a single face as the primary face on every cubie, and then
keeping track of how far clockwise from the marked slot face the
actual cubie face is.

In passive notation the permutation maps slots to elements, so our
orientation should also map slots to orientations.  So if the value
at index a in our permutation is b and in our orientation is c,
that would seem to mean that piece b is currently in slot a in
orientation c.  This is a good representation in that it has the
following property:  orientation can be separated from permutation
for state exploration purposes, because when multiplying to states
s1 and s2, the orientation change depends only on s2 (the "move",
usually) and not on s1.  So we can index the orientation separately
from the permutation, which enables a lot of nice functionality.
Other orientation conventions complicate this.

(This is even nicer if you can arrange so the largest number of
moves do not affect the orientation in the solved state, because
then a pruning table created from orientation tends to have somewhat
higher values.)

Ksolve uses this representation for states, but critically, not for
moves.  For moves, ksolve maps cubie numbers to orientations.  So
ksolve states and moves are not the same representation of a
permutation, and this affects (and complicates) the code.

To convert a state to a move:

   m.o[p[i]] = s.o[i]

Note that the permutations are the same.  To convert a move to a
state:

   s.o[i] = m.o[p[i]]

So for move 13425,01201 we convert to state 13425,02011.
For state 13425,02011 we convert to move 13425,01201.

sub movetostate      no[i]=o[p[i]] ;
sub statetomove      no[p[i]]=o[i] ;
sub sss      no[i] = (o1[p2[i]]+o2[i])%omod ;
sub sms      no[i] = (o1[p2[i]]+o2[p2[i]])%omod ;
sub mss      no[i] = (o1[p1[p2[i]]]+o2[i])%omod ;
sub mms      no[i] = (o1[p1[p2[i]]]+o2[p2[i]])%omod ;
sub mmm      no[p1[i]] = (o1[p1[i]]+o2[i])%omod ;
sub smm      no[p1[i]] = (o1[i]+o2[i])%omod ;
sub msm      no[p1[p2[i]]] = (o1[p1[p2[i]]]+o2[i])%omod ;
sub ssm      no[p1[p2[i]]] = (o1[p2[i]]+o2[i])%omod ;
sub sinv      no[p[i]] = (omod-o[i])%omod ;
sub minv      no[i] = (omod-o[p[i]])%omod ;

The twsearch program uses state representation everywhere.  In order
for this to work we need to convert the ksolve moves read in to be
in state representation rather than ksolve's move representation.

With identical pieces states always have to be on the left side of
any multiplication, but this is true for ksolve as well.

The twsearch input description

The twsearch input description format is cribbed directly from ksolve
(although I don't support all the features that ksolve has, and I do
support some features that ksolve does not have).  The parser for the
format is ad-hoc C++ code because I did not want to introduce extra
dependencies on parsers or lexers.  The input description is line-based,
with comments preceded by a hash symbol (#) and blank lines ignored.

To motivate this section, we list here the initial part of the 3x3x3
twsearch definition file (omitting some moves for brevity):

# PuzzleGeometry 0.1 Copyright 2018 Tomas Rokicki.
Name PuzzleGeometryPuzzle

Set EDGE 12 2
Set CORNER 8 3

Solved
EDGE
1 2 3 4 5 6 7 8 9 10 11 12
0 0 0 0 0 0 0 0 0 0 0 0
CORNER
1 2 3 4 5 6 7 8
0 0 0 0 0 0 0 0
End

Move F
EDGE
10 1 3 4 2 6 7 8 9 5 11 12
1 1 0 0 1 0 0 0 0 1 0 0
CORNER
7 1 3 2 5 6 4 8
2 1 0 2 0 0 1 0
End

Tokens on a line are whitespace separated.  The first command in the file
must be a Name command, which must be followed by a single token that
is used as the puzzle name.  (The name is not presently used for anything.)

After that there must be one or more Set commands.  Each Set command
defines pieces of a particular orbit, which consists of pieces that might be
exchanged for each other.  For instance, on the standard 3x3x3, the two
orbits are corners and edges; corners can never be swapped with edges, and
edges can never be swapped with corners.

Each Set command must have a total of four tokens: first, the Set command
itself, then a name for the set (which must be unique among the sets),
then a count of pieces in that set, and finally, a count of the number
of ways that a piece might be oriented (which must be an integer between
1 and 126 inclusive).  The orientation count must be supplied even if it is
1.  There is no current limit on the count of sets.

Following the Set commands there must be a Solved command.  The Solved
command is a single token on one line, followed by a position block,
which is terminated by an End line.  The position block contains zero
or more set position chunks.  Each set position chunk consists of two or
three lines.  The first line is the name of the set, on a single line.
Next is the identity of elements of that set.  Finally, we have the
optional orientation values of elements of that set; if not provided,
all zeros are assumed.

As a future-looking extension, the "Solved" command can be replaced with
the "StartState" command, with the following change in behavior.
The element numbering is zero-based rather than one-based.  This is
consistent with cubing.js states (and permutations) and with the
internal representation.

For set position blocks, we use element identities which are numbered
from 1 to the count of distinct identities.  There may be duplications
in the case some elements are indistinguishable, but the range of
values must be contiguous from 1.  If all pieces are distinguishable,
then the element identity must consist of some permutation of the
numbers 1 through the number of elements.

In a position block, if any sets are not listed, they default to the
identity permutation (with all pieces distinguishable) and the all-zero
orientation vector.

For the element orientation specification, in addition to values from
0 to one less than the orientation count for the set, an element can
have the distinguished value '?' which indicates an orientation
wildcard.  All elements that are indistinguishable and share the same
numeric identifier must either have a numeric orientation or they must
all be orientation wildcards.

After the Solved block, we then have a sequence of Move or Illegal
blocks.  We'll describe the Move blocks first.

Each Move block describes either a move (something that changes the
puzzle state) or a rotation (something that rotates the whole puzzle
in space without actually changing the position of any piece with
respect to any other piece).  The Move command is on a line with two
tokens, and the second token is the unique name of the move or rotation.

Rotations are distinguished from Moves because they consist of one or
more upper case letters or underscores followed by a lowercase v, or
they consist of x, y, or z possibly followed by 2 or a single quote.
Anything else is a move.  So Uv, FRBv, Z_Av, and x' are rotations,
while X, F, xx, and T are just moves.

Following the Move command line is a transformation block.  The
transformation block has the same syntax as a position block, except
the element identity line is instead a permutation line, and must
always contain a proper permutation with the numbers 1 through the
number of elements.  Also, if an orientation vector is provided for
a set transformation block, all values must be numeric; orientation
wildcards are not permitted as part of a transformation.

Identical pieces are useful in puzzles like Rubik's Revenge (the 4x4x4)
where there are some pieces that are generally indistinguishable,
but they, with orientation wildcards, are also useful when evaluating
different steps of solution methods.  For instance, when solving the
last layer edge orientation step of the 3x3x3, you might make all last
layer edges identical and oriented, and all last layer corners identical
but unoriented.

Move extension

When specifying moves, only a "base" move needs to be described.  A base
move is one like "U" on the 3x3x3; it's the one that generally is not
decorated with a suffix like an apostrophe or number.  For every such
move or rotation given, twsearch will generate the decorated versions by
calculating the order of the move (the number of times it must be
executed to yield the identity), and then selecting appropriate decorations.
For instance, on the 3x3x3, twsearch displays the following output
indicating what decorated moves it generates:

Created new moves F2 F' B2 B' D2 D' U2 U' L2 L' R2 R'

For the megaminx, it will generate moves including U2'; in general
it generates repetitions in both the clockwise direction of up to
and including half a full rotation, and in the counterclockwise
direction of up to but not including half a full rotation.

As a future-looking extension, the "Move" command can be replaced
with the "MoveTransformation" command, with the following changes
in behavior.  First, the numbering is zero-based rather than
one-based.  Secondly, the orientations are given in the same way
as for states, rather than in the awkward ksolve-style representation.
We recommend any new software interacting with twsearch use this
format as it is more logical and easier to reason about than the
ksolve format.

Symmetry and rotations

In general when calculating solutions or most other operations,
rotations are not used; only moves are used.  However, rotations
can be used when providing an algorithm or scramble sequence as
input.  Further, when generating pruning tables or performing a
God's algorithm search, rotations are used to perform symmetry
reduction, which can increase the speed and memory effectiveness
of the algorithms.  Twsearch will calculate the full rotation group
from the given rotations and perform symmetry reduction on that
group.  Twsearch does not yet perform mirror symmetry reduction or
inverse "symmetry" reduction (but it may in the fugure).  If a move
subset is specified, it will only use the symmetries that preserve
that move subset; for instance, on the 3x3x3, if you specify the
move subset U,R,F, then only the three-element symmetry group that
preserves the URF corner will be used.

Internal puzzle representation

The primary internal data structures are the puzzle definition
(puzdef), the set definition (setdef), and the set value (setval).
The setval is a bad name because it actually stores the full state
of all sets in one contiguous memory array.  We will describe these
structures from the bottom up, starting with the setval, then the
setdef, then the puzdef.

The setval is just a contiguous array of unsigned bytes, with two
bytes for each set element.  The values for the sets are stored in
the same order the sets occur in the input file, which is the same
order they occur in the setdef array in the puzdef.  For a particular
set, with say n elements, we have n unsigned characters storing the
permutation (for the case of a transformation) or the element numbers
(in the case of a state); then we have a further n unsigned characters
storing the orientation values in the same order.  Permutations and
element ids are stored using a base of zero (despite being read
with a base of 1).

The permutation portion of a state or transformation is always
represented in passive mode:  array element i holding value j means
that a piece with id j is in slot i (for a state), or that the
element in slot j should be moved to slot i (for a permutation).
States can hold duplicated values within the permutation elements,
but transformations must always contain each of the values 0..n-1
exactly once.

The orientation is stored exactly as given in the input for states,
with the value in slot i representing how much twisting is applied
to the cubie that is currently in slot i (thus, orientation is moved
along with the cubies).  If an orientation is a wildcard (given as
a question mark on the input), then it is stored as the special
value 2m (where m is the modulus of the orientation).  As a special
performance optimization, to avoid unpredictable branches and slow
division operations, when two orientations are added together when
performing a move, a small table is used rather than an actual
arithmetic instruction, and this table preserves the don't care
special values of 2m.

Storing the entire state of the puzzle (or transformation in the
case of a move) in one contiguous sequence of bytes helps cache
locality and simplifies some operations such as hashing of a state,
as we will see.

The setval structure itself is just a pointer to the actual array
of unsigned characters; since it is a single pointer, it is normally
stored in a register.  The setval constructor takes a pointer to
an appropriately-sized memory chunk, but does not manage the
lifetime of the memory chunk itself.

Currently twsearch uses two distinct types, stacksetval and
allocsetval, which manage lifetimes of setvals.  allocsetval is
intended for general but infrequent use, as in when reading a
puzzle definition or doing low-frequency things.  stacksetval is
intended to be used in a stack-based manner, as in automatic
values that track some recursive control flow.  I intended
stacksetval to be very fast (that is, not churn the memory
allocator) by maintaining a stack of available values on the
puzzle definition, but this fails to take into account threading.
So all the performance-critical routines, such as search,
allocate a vector of allocsetvals, one vector per thread, and
pass this vector into the recursion to use so memory allocation is
not on the critical path.

<><> setdef <><>

<><> puzdef <><>

Canonical sequences

Hashing

Indexing

Compact internal state representation

Pruning tables in memory

Reading and writing pruning tables

Compression of pruning tables

Solve routines

Schreier-Sims

God's Algorithm searches

Compact external puzzle representation

Utility routines
* Uniquify
* Conversion algorithm -> position
* Shorten
* Invert

All options

Extra stuff

To do

* Make solve be iterative rather than recursive
  * Can prefetch/parallelize memory lookups better
  * Can pause and restart search (ala generators)
  * Might be faster

* Improve compression and decompression speed

* Support moves-from-algorithms in input description

* When --showpositions or --showmoves, use StartState and/or
  MoveRepresentation rather than the older formats

* Tighten setdef; if orientationmod=1 don't store orientation;
  consider combining orientaiton and permutation if their
  product is small enough (and don't cares "work").

* Fix allocsetval and stacksetval to be the same class and to
  properly manage the state memory.
