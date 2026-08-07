# --fastindex vs --jit --jit-style 1, 3x3x3 corners-first

Real full solve (not a microbenchmark): the 50-move scramble used throughout
this project's A/B testing, against `3x3x3_cornersfirst.tws` (CORNERS
reordered to `setdefs[0]`, n=8, `--fastindex`-eligible), `twsearch_current`
== `symm-permrank` HEAD, default thread count, 2 reps each, `/usr/bin/time
-l`. Both configurations found the identical solution (max depth 18,
matching probe/node counts within normal threading variance).

| | `--jit --jit-style 1` (previous best) | `--fastindex` | delta |
|---|---|---|---|
| wall | 219.4s | 208.4s | **-5.0%** |
| instructions | 22.81T | 21.49T | **-5.8%** |
| cycles | 7.91T | 7.14T | **-9.8%** |
| IPC | 2.88 | 3.01 | +4.4% |

`--fastindex` wins outright here: fewer instructions *and* better IPC,
landing at -9.8% cycles overall vs. the fastest JIT variant on this
machine -- with none of JIT's deployment cost (no spawning a compiler, no
`dlopen`, no self-check-against-a-shared-library dance; the 40320-entry
table itself builds in a few milliseconds, see `--fastindex -v2`'s timing
output).

That's the payoff for accepting `--fastindex`'s prerequisite: `setdefs[0]`
must be reordered to a small (n<=20, ideally much smaller) set. The cost of
doing that -- CORNERS (n=8) vs. EDGES (n=12) as the primary discriminator
-- is real but modest, ~11-16% slower per `slowmodm2` call in isolation
(see `-T`'s "moves plus symmetry" line, an escalating single-threaded
scramble-length sweep, and a full single-threaded run of this exact
scramble: 1560.0s edges vs. 1767.7s corners). `--fastindex` more than pays
that back.

Raw logs: `logs/fastindex_vs_jit_{jit,fastindex}_run{1,2}.log` (gitignored,
regenerate with the commands below if not present).

```sh
SEQ="B F D' D2 D2 B2 B R' F' L2 F2 F F' D D2 R2 F R' D R' L2 D2 L' D' F B' L2 U2 D' B2 D U2 B F' L B U' U' D' F2 L' R' B L F' R' U U' D F'"
echo "$SEQ" | /usr/bin/time -l ./build/bin/twsearch_current --jit --jit-style 1 --nowrite -M 4000 -s samples/symm/3x3x3_cornersfirst.tws
echo "$SEQ" | /usr/bin/time -l ./build/bin/twsearch_current --fastindex --nowrite -M 4000 -s samples/symm/3x3x3_cornersfirst.tws
```
