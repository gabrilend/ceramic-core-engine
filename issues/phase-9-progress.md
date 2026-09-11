# Phase 9 progress — the engine leaves home

Phase 9's goal: the engine stops being something that only exists
inside this repository. Everything in the phases beneath makes programs
run; nothing in them lets anybody build one anywhere else.

**The shape is decided and it is two files.** `src/cera.c` and
`src/cera.h` — one translation unit and one header. A consumer takes
two files, and everything the engine does not want them to call stops
being a linker symbol at all, because a function in the same
translation unit as its callers can be `static`.

| Issue | State | In one line |
|---|---|---|
| [901 — the engine becomes one file](completed/901-the-engine-becomes-one-file.md) | **complete** | Eleven bodies and seven headers concatenated in reading order into `cera.c` and `cera.h`, with a banner and a `#line` at every seam. Compiled clean under `-Wall -Wextra -Werror` with no edit to the code that moved. Thirty-three of thirty-four test outputs byte-identical to the numbered build; the thirty-fourth differs by the four lines the emitted file's include block lost. |
| [902 — the header says what is public](completed/902-the-header-says-what-is-public.md) | **complete** | The header declares 100 symbols and the engine exports exactly those. Twenty-three joints moved into the body with their documentation; two of them had never been declared in any header and were exported anyway. |
| [903 — everything else goes private](completed/903-everything-else-goes-private.md) | **complete** | 123 exported symbols became 100, and a test checks it on every run — the list was derived from the header rather than written, and the test was proven by being made to fail. Seven white-box tests now compile inside the engine. |
| [904 — the old files are removed](completed/904-the-old-files-are-removed.md) | **complete** | Gone, with their eighteen interface files and the `libs/` directory. The `#line` directives went rather than being re-pointed, and the two include paths became one. |
| [905 — the prefix](completed/905-the-prefix.md) | **complete** | Every one of the 100 exported symbols begins `cera_`. 3,008 renames and 706 respellings across 52 files, and not one byte of test output changed. The linker's export list collapsed to a single line. |
| [909 — the blueprints name their calls](completed/909-the-blueprints-name-their-calls.md) | **refused** | Every completed blueprint saying which calls it produced. Cannot be derived — the house style keeps function names out of prose, so six issues in seventy-four name one — and so has to be decided a blueprint at a time. |
| [906 — an error reaches the host](completed/906-an-error-reaches-the-host.md) | **complete** | An installable handler, called with the message and the exit code immediately before the engine dies. Building it meant building the funnel first: thirty places wrote to stderr and ended the process, each its own little ending. |
| [907 — built outside the tree](completed/907-built-outside-the-tree.md) | **complete** | The capstone. `make test` builds a program with this engine in a scratch directory that cannot see this repository, runs it, and checks the answer. It caught two real faults on its first run. |
| [908 — two maps in one process](completed/908-two-maps-in-one-process.md) | **complete**, and it needed no code | There is no process-wide active map — it went with the statics table. What was missing was a test, because a singleton is invisible until two of something exist. |
| [910 — the engine ships as a compiler](910-the-engine-ships-as-a-compiler.md) | **in progress** | `serac` builds, carries the engine as text, writes a `main`, and the engine's three baked-in paths collapsed into one name. Open questions remain unworked. |

## Why the amalgamation is the source rather than a build artifact

Note 057 surveyed three shapes and recommended the amalgamation, but as
something a **packaging script derives** from the numbered files on the
way out. This phase instead makes `cera.c` and `cera.h` the source and
deletes the numbered files.

The difference is what happens to the reading order. The script version
keeps eighteen files whose numbers say what to read when, and produces
two files nobody edits. The direct version keeps the order inside
`cera.c` as section banners, and has one copy of the engine rather than
one copy plus a derivation that can drift from it.

What the script version was protecting — that a consumer's two files are
never hand-maintained and so cannot rot — turns out not to be worth a
second copy of the engine, because the two files *are* the engine and
rot the same way any source does: by nobody running the tests.

## What the first issue turned up

**The tests passing is not the measurement.** Comparing output byte for
byte needed an instrument first, and building it found three kinds of
difference that mean nothing — process ids, timings, and *counts
produced by concurrent scheduling*. Four tests race threads and then
report what happened; their numbers differ every run on an unchanged
binary, which is those tests working. They are compared by shape, and
they are named in `scripts/111-capture-test-output.sh` rather than
detected, so a fifth is a deliberate addition rather than a silent one.

The instrument was checked by diffing two runs of the unchanged build
against each other, so it was known to read zero before it was used to
measure anything.

**Nothing in the engine needed editing to be merged.** No name collided,
no type collided, and the only preprocessor care needed was three
sections taking their private macros with them. The one thing that could
not be undefined was the set of build-time facts, because two sections
use them and the second would have lost them.

## What is already decided

**The two names carry no index.** Every other source file here takes the
next number from `.file-index-counter`, because the numbers are a
reading order across the whole tree. These two do not, because they are
the deliverable and a consumer's tree is not our reading order. This is
the one deliberate exception, and it is the same awkwardness — an entry
point called `018-station.h` — that the phase exists to remove.

**The freed numbers are not reused.** Eighteen indices come free when
the numbered sources go. A number is a position in a reading order, and
a reused number makes two things claim one position; the gap is the
record that something stood there. See
[711](711-the-index-means-reading-order.md).

**Errors still end the program.** [906](completed/906-an-error-reaches-the-host.md)
adds a way for a host to *hear* about a refusal, and deliberately no way
to survive one. An error code a caller may ignore is a fallback wearing
a return type.

## What the compiler turned up

**The scratch directory was never needed, and the question dissolved
rather than being answered.** The blueprint had one question left: where
to put the engine when `serac` writes it out, and whether to keep it
between runs. Both answers had a real cost — rewriting a megabyte every
time, or a stale copy that could be compiled against.

Neither was paid, because the premise was wrong. A C compiler will read
a whole program from a pipe. The only thing forcing a file to exist was
one `#include` of the engine's header, and an include is a filesystem
lookup — which concatenation dissolves, by putting the header's text in
front and deleting the line. That is the same move this engine already
made when eleven sources became `cera.c`. The linker's export list was
the other file, and naming the exported family in a flag does what
handing over the file did.

Measured before it was relied on: the one-translation-unit binary
exports the identical set of symbols and, stripped, comes to the same
size as the one the Makefile builds, differing by 412 bytes of text. The
change is mechanism only; the product is unchanged.

**A compiler that announces where it put things cannot be used by a
program.** `serac` printed its output path on success, and the engine
runs it as a subprocess when a description arrives at run time — so that
sentence landed in the middle of somebody's results. Silence on success
is the convention for a reason that is not taste.

**The default output path can land on an input.** `--emit-c` over
`shapes.map` wants to write `shapes.c`, and a box source of that name
beside it is an entirely ordinary thing to have. It destroyed the source
and then failed to compile it, and the message was about a brace on a
line nobody wrote. Found by a test doing exactly that by accident, which
is the useful kind.

**A `const char *` result cannot happen, and the branch handling it was
code nothing could reach.** The parser already refuses a box that returns
a borrowed pointer — the bytes live wherever it points and a value on a
wire has no owner. Written before that was checked, removed after.

## Where the phase stands

**Done. The engine has left.** It is two files. It
publishes 101 symbols and every one says whose it is; nothing else
escapes at all, and a test checks that on every run. A host installing
one function hears why it stopped. Two programs run side by side in one
process on separate pools.

And the claim is no longer inference. `make test` copies the engine into
a directory that cannot see this repository, writes a box and a map and
a program from nothing, builds them the way a consumer would, and checks
the answer that comes back.

[909](completed/909-the-blueprints-name-their-calls.md) was refused: a
blueprint describes what to build, and the name a call ends up with is
not part of that. The list of calls exists in `src/cera.info.md`,
derived from the header, which was the useful half.

**And then the leaving got shorter.**
[910](910-the-engine-ships-as-a-compiler.md) says the engine is not four
things a consumer assembles but one command, and most of it stands: a
description and some C are a program, built by `serac`, which carries
the engine inside itself and writes the `main` nobody wanted to write.
The out-of-tree test now does its work twice — once the old way, four
files and three compiler invocations, and once in one command — so the
two claims are checked against each other rather than one replacing the
other on trust.

It is not finished, because its open questions have not been worked
through. They are listed at the end of the issue and repeated under
*What is not decided* below.

## What the last three turned up

**A funnel had to exist before a handler could hang off it.** Thirty
places wrote a sentence to stderr and then ended the process, each its
own little ending, so there was nowhere for a host to be told from.
Collecting them named a distinction the code was already making without
saying so: most refusals are a fault outside the engine and exit with a
code, while six in the delivery path are the engine finding a fault in
itself and abort, because a core is the evidence.

**One death stays outside the funnel on purpose.** The quit signal's
path takes no locks, since the reason it arrived may be a lock nobody
will release — and a handler is somebody else's code, which may take
one.

**A singleton is invisible until two of something exist.** The
process-wide active map had already gone with the statics table, and
nothing had noticed either way, because no test ever built two programs
and ran them at once.

**The portability test caught two faults on its first run**, which is
the whole argument for having it. It was writing its scratch tree into
the RAM tier that is mounted without execute permission, and its map
reached for one of this repository's own demo boxes — a test for
portability quietly borrowing from home, which is the exact failure it
exists to catch.

## The engine has one name and two spellings of it

Asking which prefix to use produced the observation that ends the
question:

```
soramech
ceramic
```

**The same word.** Identical consonants in order — S, R, M, K — every
vowel reduced to almost nothing, and a silent `h` on the end. Four of
the letters line up outright when the two are stacked.

So this engine was never renamed when it was distilled out of the larger
project onto this branch. It was **respelled**, and the respelling was
half-finished and nobody noticed, because a half-finished respelling
does not feel like a collision — there was only ever one name to
collide with.

What that leaves is not a choice between names but a choice of spelling,
and the answer follows from the files rather than from anybody's taste:
a consumer holds `cera.h`, writes `#include "cera.h"`, and should not
then have to call something spelled `cera_`. The include and the call
agree, or the consumer's first line hands them a puzzle.

[905](completed/905-the-prefix.md) is unblocked and its cost is unchanged — it was
never the price of deciding, it was the price of the respelling having
been half-done for months.

## How it came to be this way

### Two programs stopped sharing a pool of workers

There was briefly a call that started one program beside another so the
two shared a pool. Workers are mostly idle, so sharing them looks like
the thrifty arrangement — until the second program's teardown takes the
first one's workers with it. One program per pool costs threads that were
not doing anything anyway, and it is the arrangement the rest of the
design already assumed: a station table belongs to one pool the way it
belongs to one processor.

## What is not decided

**Issue 910's open questions**, which are recorded in the issue file and
have not been worked through: what `serac` does when the compiler it
bakes in is absent, whether the result bound is the right shape, whether
`--main=` earns its place beside `--emit-c` and `--unpack`, what root a
box source outside the description's directory should be shortened
against, and the fact that the map format now has two writers and three
readers with nothing deriving any of them from each other.
