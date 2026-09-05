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
| [909 — the blueprints name their calls](909-the-blueprints-name-their-calls.md) | open, **split out of 902** | Every completed blueprint saying which calls it produced. Cannot be derived — the house style keeps function names out of prose, so six issues in seventy-four name one — and so has to be decided a blueprint at a time. |
| [906 — an error reaches the host](906-an-error-reaches-the-host.md) | open | An installable handler called with the message before the engine dies. The engine still dies. |
| [907 — built outside the tree](907-built-outside-the-tree.md) | open | The capstone: `make test` builds a program with this engine in a scratch directory that cannot see this repository, and runs it. |
| [908 — two maps in one process](908-two-maps-in-one-process.md) | open, **not on the critical path** | The process-wide active map, threaded through the task instead. Or, failing that, written down where somebody will read it. |

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

**Errors still end the program.** [906](906-an-error-reaches-the-host.md)
adds a way for a host to *hear* about a refusal, and deliberately no way
to survive one. An error code a caller may ignore is a fallback wearing
a return type.

## Where the phase stands

**Five of nine done, and the engine can now leave.** It is two files,
it publishes 100 symbols and every one of them says whose they are,
nothing else escapes at all, and a test checks that on every run.

What is left is not about shape:

- **[906](906-an-error-reaches-the-host.md)** — a host cannot hear why
  the engine stopped, only that it did.
- **[907](907-built-outside-the-tree.md)** — nothing has yet compiled
  this engine from a directory that cannot see this repository, so every
  claim above is still inference from reading rather than from doing.
- **[908](908-two-maps-in-one-process.md)** — one process-wide variable
  means one map, silently.

And [909](909-the-blueprints-name-their-calls.md), which is about the
record rather than the engine: the blueprints do not say what the calls
they describe are called.

907 is the one that matters most, because it is the only one that can
prove the rest.

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

[905](905-the-prefix.md) is unblocked and its cost is unchanged — it was
never the price of deciding, it was the price of the respelling having
been half-done for months.

## What is not decided

Nothing in this phase, currently.
