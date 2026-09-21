# The bridge protocol

How a web page has a solver on the reader's own machine do its searching,
instead of a WebAssembly build in the browser: the difference between four
minutes and for ever on a 3x3x3.

Two programs in this repository speak it:

- `twsearch --serve` (`src/cpp/serve.cpp`), which needs nothing but twsearch;
- `src/js/twsearch-bridge.mjs`, the same thing under node.

A page cannot tell them apart, and this document is what another solver would
implement to take their place.  `test/bridge-test.mjs` is the protocol's test
suite; point it at anything that claims to speak this.

## Shape

HTTP on the loopback interface, port 2023 by default.  Requests and replies
are JSON, except that a search replies with a stream of JSON lines
(`application/x-ndjson`) so that a page can show what the solver is doing
while it does it.

The `protocol` number in `/v1/info` is 1.  A change that would confuse an
existing page means a new number.

## What a page asks for

### `GET /v1/info`

```json
{"bridge": "twsearch-bridge", "protocol": 1,
 "version": "v0.1.5", "threads": 16, "maxMem": 8192}
```

A page calls this to find out whether a solver is there at all, so it must be
cheap and must not start anything.  Only `bridge` has to be there, and it is
the whole of the answer a page needs: the rest is for a reader wondering what
is doing the work.  `threads` and `maxMem` (megabytes) say what it is willing
to use; `version` is optional.

Say nothing here that names the person running it.  An answer used to carry
the solver's path, which on most machines holds their account name, and a
page that reaches this at all can read it.

### `POST /v1/solve`

```json
{"id": "solve-7",
 "tws": "Name 3x3x3\nSet CORNERS 8 3\n...",
 "args": ["-v2", "--checkbeforesolve"],
 "scramble": "ScrambleState explorer\nCORNERS\n..."}
```

`tws` is the puzzle, in the format `docs/ksolveformat.txt` describes;
`scramble` is the position to solve, as a `Scramble`, `ScrambleState`,
`ScrambleAlg`, or `CPOS` block; `args` are search options (below).  `id`
names this search so that it can be cancelled.

The reply is `200` and a stream of JSON lines, one event each:

```
{"type":"out","text":"Filling depth 7 val 2 "}
{"type":"out","text":"saw 73312415 (1000000) in 1.5 rate 48.8\n"}
{"type":"out","text":" F2 R U' R'\n"}
{"type":"done"}
```

- `out` and `err` carry the solver's output as it appears, **in whatever
  pieces it arrives in**, not a line at a time.  A page shows the text as it
  comes, so a half-written line such as `Filling depth 8 val 2 ` must be sent
  before the search stops to do that work, not held until the line ends.
- `done` says this position is finished, whether or not a solution was found.
- `error` with a `message` says the search could not run or could not
  finish.  It ends the stream just as `done` does.

Exactly one `done` or `error` ends each stream.

A bad request is `400` with a plain-text reason; a request from a page that
is not allowed is `403`.

### `POST /v1/cancel`

```json
{"id": "solve-7", "abandon": false}
```

Answers `204`.  With no `id`, cancels whatever is running.  A cancelled
search ends the way a finished one does: the solver says so in its output and
the stream ends with `done`, rather than with an `error`.

`abandon` asks for the search to stop **now**, giving up whatever it has
built, for a reader who does not want to wait for a pruning table to finish.
Without it, a cancel that arrives during such a step takes effect when that
step ends.

### `OPTIONS` on any of these

The preflight.  See below.

## What the solver does about it

**One puzzle at a time.**  A solver keeps what it has worked out about a
puzzle (twsearch keeps its pruning tables), so asking for the same puzzle and
options again must reuse that: the second search of a 3x3x3 takes seconds
where the first took minutes.  A request with a different puzzle or different
options replaces it, and whatever was running is cancelled with an `error`.

**Searches queue.**  A request that arrives while another is running waits
its turn rather than being refused.

**A page that goes away** (the connection closes) cancels its search.

**Nothing outlives the server.**  twsearch searches in a child process, and a
search can hold many gigabytes; a server that is killed outright never gets
to tidy up, so each child watches for the server going away and exits when it
does.  A solver that searches in another process should do the same.

## Keeping the reader's machine theirs

A program that any web page could drive is a program that needs care.  All of
this is required of an implementation, not optional:

- **Listen on 127.0.0.1 only.**  Never on a public address.
- **Check the `Host` header** names this machine (`127.0.0.1`, `localhost`,
  `[::1]`), which is what stops another site from pointing a name it controls
  at this address (DNS rebinding).
- **Check the `Origin` header** against a list.  Pages served from this
  machine are allowed; others must be named (twsearch's `--allow-origin`).
  Answer `403` otherwise, and echo the allowed origin back in
  `Access-Control-Allow-Origin`.
- **Answer the preflight**, including `Access-Control-Allow-Private-Network:
  true`, which Chrome requires before a page from the public internet may
  address a private one.
- **Allow only the options that make sense**, by name, and refuse the rest:
  this is a page choosing what a program on someone's computer does.
  twsearch allows the search options (`-c`, `--moves`, `--maxdepth`, the
  ones that ignore sets, ...) and refuses anything that writes where it
  likes, such as `--cachedir`.
- **Cap the memory.**  Whatever a page asks for with `-M`, no more than the
  figure the reader started the solver with; if the page asks for nothing,
  hand it that same figure.
- **Pass down the reader's pruning table settings.**  twsearch gives each
  search the `--writeprunetables` and `--cachedir` it was started with; a
  page's `--writeprunetables` overrides the first.
- **Limit the request size.**  32MB is generous for a puzzle definition.

## Keeping what crossed the bridge

`twsearch --serve --echo` writes the whole conversation to standard output:
the puzzle when it arrives, each scramble as it is sent to the solver, and
the solver's output as it goes back, exactly as the page sees it and in the
pieces it arrives in, so a line the solver is still writing ("Filling depth
8 val 2 ") shows progress here as it does there.  Redirect
it to a file and a session can be read, kept, or picked over later.

The remarks it adds are comments, and the puzzle and scramble blocks are
written as they are, so those can be lifted out into a `.tws` file and run
again.  The transcript as a whole is not a `.tws` file.

## Where the page comes from

A browser will not let a page from the open web reach a program on the
reader's machine: Chrome refuses outright, whatever headers are answered, and
what it offers instead (a permission the reader grants) is not something a
page can rely on.  Two shapes avoid the question entirely, because the page
is then as local as the solver:

- the solver answers `GET /` with the page, as `twsearch --serve` does: the
  page and the searches it asks for share an origin, so nothing is
  cross-origin at all;
- the reader opens a copy of the page from their own filesystem, which sends
  `Origin: null`.

`Origin: null` is not a safe thing to allow, and twsearch does not: a page in
a sandboxed iframe sends it too, and any site on the web can put one of those
on any page it serves, so a solver that trusts `null` can be driven by all of
them.  `twsearch --serve --allow-origin null` takes that on deliberately, for
someone running a page from their own disk who knows what it costs.  The
first shape above wants none of this and is the one to reach for.

Either way, a page kept on a web site remains the way to try a solver in a
browser, with the searching done there.

## For another solver

The text in `out` events is the solver's own, and a page that wants to show
solutions has to read it.  twsearch's is what the Twizzle Explorer knows:

- a solution is a line beginning with a space, holding the moves;
- a position it finishes with is announced by a line beginning `Found `,
  `No solution found in `, `Ignoring unsolvable position.`, or `Search
  canceled at depth `.

Another solver that emits those has a page that already works.  A solver
whose output looks nothing like that is the reason to raise the protocol
number and add an event of its own — say `{"type":"solution","moves":"..."}`
— which a page could prefer when it sees a higher number, falling back to
reading the text otherwise.
