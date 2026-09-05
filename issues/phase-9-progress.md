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
| [902 — the header says what is public](902-the-header-says-what-is-public.md) | open | Narrow the header to what generated code binds to and what a consumer calls; everything else moves into the body. This is also where the completed issues learn to state a signature. |
| [903 — everything else goes private](903-everything-else-goes-private.md) | open | Every definition not in the header becomes `static`. The white-box tests move inside the translation unit. This is the step the whole phase exists for. |
| [904 — the old files are removed](904-the-old-files-are-removed.md) | open | The eighteen numbered sources and their interface files go, once the output has been compared byte for byte. |
| [905 — the prefix](905-the-prefix.md) | open, **blocked on a decision** | One prefix on every public name. Which prefix is an open question with three candidates and no default; asking is step one. |
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

## What is not decided

**The prefix.** Three candidates, no default, and a one-shot decision
because applying the second one costs what the first did. See
[905](905-the-prefix.md), which cannot start until it is answered.
