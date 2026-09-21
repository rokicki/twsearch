# Managing the pruning table cache

A proposal.  Nothing here is implemented yet.

## Where things stand

Pruning tables go in `XDG_CACHE_HOME` if it is set, otherwise
`~/Library/Caches/twsearch` on macOS, `~/.cache/twsearch` on Linux, and
`LOCALAPPDATA` on Windows (`util.cpp`).  `--cachedir` overrides all of it.

A file is named `tws9-<basename>[-q]-<size>[-o<optionssum>].dat`, where
`<basename>` comes from the `.tws` filename.  The puzzle's own checksum goes
into the name only when the basename is unknown (`prunetable.cpp`,
`addsumdat`).

Nothing is ever deleted.  There is no size limit and no expiry.  `auto`, the
default, writes only when reading a table back looks faster than rebuilding
it, so cheap tables never reach the disk; that is the only thing holding the
size down today.

Correctness does not depend on any of this.  Each file carries the puzzle
checksum in its header and is rebuilt when it does not match, so a name
collision costs work, never a wrong answer.

On one development machine the cache is 24 GB, holds a 32 GB-sized entry,
and every puzzle that arrived through `--serve` shares the single basename
`TwizzlePuzzle`, so those overwrite each other.

## Who has to be happy

1. **A tight disk.**  Keeps it close to full.  Must never be pushed over the
   edge, and must not have to know this feature exists.
2. **A roomy disk.**  Wants tables kept across puzzle switches.  Also must
   not have to know the feature exists.
3. **A big machine.**  2 TB of disk, 1 TB of RAM, and a 1 TB pruning table
   worth writing even when it takes 90% of what is free.  This has to be
   easy, not a fight with a safety rail.
4. **Someone who forgets.**  Uses twsearch for a week, stops, and three
   months later goes hunting for big files.  twsearch is not running in
   those three months, so nothing can reclaim anything.  The only defenses
   are a ceiling low enough that the pile is not alarming and a directory
   that says plainly what it is.

## Ideas

- Compute the budget from free space on every run, so a disk that fills over
  months gets the cache trimmed the next time twsearch runs rather than at
  the moment the user is already stuck.
- Measure the fraction against free space plus what the cache already holds.
  Against free space alone the budget ratchets down every run, because the
  cache's own bytes have stopped being free.
- Expire by age unconditionally.  A year without a read is enough to say a
  table is not interesting.
- Order eviction by last read, which needs the mtime stamped on a successful
  read; atime is not reliable under relatime.
- Refuse an oversized write rather than evicting everything to fit it.
- Keep a free space floor that no automatic write may cross.
- Order eviction by value instead of age: `auto` already estimates rebuild
  time, so a table that is expensive per byte could outrank an older cheap
  one.  Wants new header fields; later.
- Keep the readme.txt already written into the cache directory, and give
  files readable names so someone who finds them knows what they are.
- Say one line when files are removed.  Rare enough not to be noise.
- Let the whole thing be turned off.

## Proposed policy

Recomputed on every run, with nothing configured:

```
budget = 25% of (available + current cache size)
floor  = max(5% of capacity, 5G)    // an automatic write never crosses this
```

No absolute cap.  A 2 TB disk with 1 TB free settles at a quarter of a
terabyte of tables and stays there, which is a reasonable thing to do with
space that is going unused.  The fraction is what keeps it honest: the cache
can never be the thing that fills a disk, since it never takes more than a
quarter of what is going spare, and the budget is recomputed on every run, so
a disk filling up with other things shrinks the budget and the next sweep
trims to it.

Sweep once per process, the first time the cache directory is used:

1. Delete anything not read in 365 days, whatever the budget says.
2. While the total is over budget, delete least recently read first.
3. Never delete what this process wrote or is reading.
4. Never delete a file larger than the whole budget.  Removing it cannot
   make anything else fit, and its size means someone asked for it
   deliberately; rule 1 still reclaims it eventually.

On a successful read, set the file's mtime to now, so "last read" is real.

Before an automatic write, skip the write, silently but for a `-v2` line, if
the table alone is over budget or would take free space below the floor.  A
skipped write costs a rebuild later and nothing else.

What each user gets, without setting anything: the tight disk writes almost
nothing, because `available` is small and the floor blocks what is left.  The
roomy disk gets a quarter of its free space, far more than enough to keep
every puzzle anyone switches between.  The one who forgets is the case this
serves least well, and is covered by the sweep running on every invocation,
by the year rule, and by a readme that says the files are safe to delete.  If
twsearch is never run again, nothing reclaims anything, and the honest
statement is that the pile is a quarter of what was free when they stopped.

## Writing a table that does not fit the policy

Case 3 is the reason the policy governs `auto` and not the user.

`--writeprunetables always` means the user asked for the file.  The budget
and the floor do not apply to it: twsearch writes it, 1 TB or not.  If the
write fails for space, warn, remove the partial file, and carry on with the
search, which does not need the table.

Sweep rule 4 keeps such a file from being deleted on the next run by a
budget far smaller than it.  Reading it refreshes its mtime, so a table in
use stays; one that goes unread for a year does not.

`--limitcache 90%` makes it permanent rather than per run.

When an automatic write is skipped for size, say so in one line, naming both
ways to keep it.  That is how case 3 finds the options without reading any
documentation.

## Options

- `--limitcache <size>` sets the budget.  Takes `50G`, `500M`, `10%`, and
  `0` or `none` to cache nothing.  Persisted.
- `--clearcache` deletes the tables, reports what it freed, leaves the
  setting alone.
- `--cacheinfo` lists puzzle, size, and last read, with the total and the
  budget in force.  What someone wants before deciding to raise the limit.

The setting belongs in a config directory, not in the cache directory: a
limit someone chose should survive `--clearcache`.  That means resolving
`XDG_CONFIG_HOME`, else `~/Library/Application Support/twsearch` on macOS,
`~/.config/twsearch` on Linux, `APPDATA` on Windows, mirroring what
`prune_table_dir` already does.

## Implementation notes

`std::filesystem` covers all of it: `space` for capacity and available (use
`available`, not `free`), `directory_iterator` for the sweep,
`last_write_time` to read and to stamp, `remove` with an `error_code`.  Take
the `error_code` overloads throughout so an odd filesystem cannot throw out
of a search.

Query `space` on the cache directory, not on the root: the cache can be on
another volume, which is also how case 3 points a huge table at a big disk
with `--cachedir`.

On macOS, APFS counts purgeable space (local Time Machine snapshots) as
available, so the figure runs high.  The floor absorbs that.

Races to tolerate rather than prevent: two twsearch processes sweeping at
once, where a delete that hits ENOENT is fine; Windows refusing to delete a
file another process has open, which is a skip; and mtimes in the future
from clock skew, which count as recent.

The sweep is one pass over a directory holding tens of files, so it can run
on every invocation.

## Not part of this

**Per puzzle names.**  Naming a cache file for the puzzle it belongs to,
rather than for whatever the caller named the `.tws` file, is a separate
change, and it is what makes the cache grow in the first place: today every
puzzle from `--serve` overwrites one file.  It should follow this one.

**A separate budget for `--serve`.**  Two policies are messier than one and
the benefit is not there.

**Value based eviction.**  Wants rebuild cost in the header; worth doing
later, and the policy above does not preclude it.

## Open questions

- Is 25% the right fraction?  It decides both how well case 2 is served and
  how big a surprise case 4 gets.
- Is one year the only age rule, or is there also a shorter one that applies
  when space is tight?
- Config directory or cache directory for the persisted setting?
