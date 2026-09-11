# Phase 6 progress — the map file

Phase 6's goal: the capstone. A program becomes a directory of C
functions and a text file, and changing the shape of the program never
touches the C.

**That goal was met and then widened.** Phase 6 built loading as its
own mechanism — parse, place, resolve, validate, seed, release, in that
fixed order, with a notion of *still loading* that existed nowhere
else. What replaced it is one surface for creating a station,
configuring a port, and drawing a wire, legal at any moment, with
**loading as its first caller rather than a path of its own**
([212](completed/212-one-way-to-build-a-program.md)). Building a program and
editing a running one stopped being two things.

| Issue | State | In one line |
|---|---|---|
| 601 — map file parser | **losing a form, keeping its shape** | Reads and never constructs — which is exactly what lets the loader shrink to a reader. |
| 602 — loader first pass | **becoming a caller** | Stations from the registry; the "still loading" state disappears with the pre-sized table. |
| 603 — loader second pass | **becoming an ordering** | Resolve names after every station exists — permanent reason, temporary machinery. |
| 604 — load-time validation | **moved out of the loader** | Both were about a pull path; the whole-program remainder becomes a report, not a refusal. |
| 605 — the seed sweep | **moved, not removed** | Nothing replaced it: construction's own writes are what start a program. |
| 606 — phase 6 demo | complete | One binary, three programs; edits followed; every refusal shown; everything at once. |
| [607 — no reserved words](completed/607-no-reserved-words.md) | complete | Every line announces itself — `station`, `in`, `out`, `statics` — so a name never sits where a keyword sits and no word is reserved. The three words were the symptom; the problem was that a station line was defined as *what remains*, and a negative definition can only narrow. |
| [601a — a wire is written at both ends](completed/601a-a-wire-is-written-at-both-ends.md) | complete | Every wire appears twice in a file, once on each end, and loading refuses when the two disagree — four distinguishable mistakes, all of them collected before any is reported. Reading one station now tells the whole truth about it, where before that meant scanning the file for arrows that named it. Nothing changed at run time: the second declaration is checked and dropped, because delivery only ever asks where a value goes. The dump derives the receiving ends rather than remembering them, so they cannot drift; a tool migrates old maps, including the ones written as C string literals inside tests. |
| [601b — the dollar sign means the boundary](completed/601b-the-dollar-sign-means-the-boundary.md) | complete | `$N` means one thing now: this port crosses the map's boundary, at this position, with the `in` or `out` keyword carrying the direction. It used to point into a `statics` section — a second spelling of a constant the dump never wrote — while reading exactly like a shell positional. That section is gone, and so are `entry` and `result` on the station line; all three are refused by name, because a file written before the change has them and its author wants to be told what replaced them. |
| [601c — the format stops guessing](completed/601c-the-format-stops-guessing.md) | complete | Strings are quoted, braced values continue across lines, an over-long line is refused rather than split, and a `#` inside a string is a character. Four places where the reader decided for itself what somebody probably meant. |
| [608 — the station line reads at a glance](completed/608-the-station-line-reads-at-a-glance.md) | complete | The kind is the first word of the line and spelled out; the box function is in brackets. Five kind words parse and the writers emit the three long ones. This is the change that collected on 607's promise — the format gained four keywords and took no name away from any map, which the reserved-word test now demonstrates rather than asserts. Two writers turned out to exist where the plan assumed one. |


## What naming the file turned up

**Three issues, and each one was the next one's precondition.**
[609](609-a-station-line-names-its-file.md) made every station line say
which file its box is in. That made the question *what does a file name
mean* unavoidable, which is [610](610-a-map-says-where-to-look.md):
relative to the description, with a block of shortcuts so a path is not
spelled out every time. And that made a dump able to name its own
directory, which is [611](611-a-dump-is-a-program.md).

**The generator and the compiler had two ideas of what a filename
meant.** The generator matched it as a suffix of a box's path; the
compiler joined it to the description's directory and opened exactly
that. A description could satisfy one and be refused by the other, and
for a while every description in this project did. They call one
function now.

**The migration tool was blind twice.** It could not see boxes written
into a shell heredoc — it mis-resolved a test's own `swallow` against a
project box of the same name — and it could not see map text inside C
string literals, which was ninety-eight addresses across eight files.
The second was fixable and the tool was extended. The first is not: the
tool would have to run the shell script to know what it writes. So it
reports every name it could not place, and that is the whole of the
defence.

**Addresses got longer and a fixed buffer noticed.** A test held its map
text in a thousand bytes, which was ample for bare function names and
not for paths.

**And the shortcut's trailing slash meant something for about an hour.**
It was going to be how a directory shortcut was told from a file one.
There is no such distinction: a shortcut is a piece of path standing in
for the first segment of an address, and `libs` and `libs/` are the same
piece of path, the way they are to a shell. Doubled slashes collapse for
the same reason.

## What the phase established, and what survived being widened

**The parser reads and never constructs.** This is the decision that
aged best. Because parsing produces a *description* and something else
builds from it, the loader could later be reduced to "turn each line
into one call" without having to be disentangled from the thing it
feeds.

**The registry supplies everything about a station; the file supplies
only a name.** Shim, parameter count, sizes, type names — all from the
box record, so the file cannot disagree with the C. That is why
placement by name is the only placement worth having, and why the
hand-placement path from phase 2 is being retired rather than repaired.

**Check a wire at the first moment both ends are known.** Per edge, not
per program — which is what let runtime rewiring reuse the same check
on a running program, and what makes the whole-program pass shrink to
the few things that genuinely need everything present.

**Collect failures and print them together.** Somebody fixing a new
program wants the whole list, not one error per run. That outlives any
particular rule in the list.

**And the message is the deliverable.** *"head → wrong.1: box returns
int, slot takes double"* names both stations, the port, and both types.
It is the standard that shape-based comparison
([309](completed/309-types-by-width.md)) has to meet or beat when type names stop
being what a wire is checked against.

Notes for the phase: parser and loader are separate files, read versus
build. The gather input line resolved in the second pass rather than
the first, because its source was a name and names may point forward —
a wrinkle in 602's split that the report records, and one that stopped
existing along with the line itself.

## How it came to be this way

These are the turns the design actually took, lifted out of the source
comments where they had been sitting. They describe states the engine is
no longer in, which is why they are here rather than beside the code: a
comment is for what is true now.

### Validation and seeding stopped belonging to the loader

Reading a map file was a privileged act: it validated the whole program
and seeded the first tasks in a phase nothing else could enter, so there
was a state called *still loading* that only the loader could be in. A
station added any other way had no route through those checks. Both
became one repeatable call a caller makes after assembling a program by
whatever route, and the privileged state stopped existing.

### The pool learned to carry one more thing it does not understand

A pool served exactly one program, and the reason was mechanical:
finishing a task means resolving a station number, a number means nothing
without the table it indexes, and that table came from the pool's own
context. Making a task carry which program it belongs to — ferried
without ever being looked at, like the station number and the exit number
beside it — was the whole change, and it is what lets one pool serve
several programs at once. That is how a program can set another going
beside itself.

### The format had two writers, and only one of them was in the plan

The generator's map writer and the engine's own dump each held a copy of
the kind letter and a copy of the station-line format, tied together by
nothing. Changing the format in one left the other emitting the old
shape, which surfaced in the worst way available: a map the engine had
just written, refused by the reader that had just loaded it. Both were
corrected and the duplication was left standing, so this is a thing that
will bite again — the two are not derived from each other and nothing
compares them.

The other half of the same lesson is about tools rather than writers. A
tool that recognises a station line in order to do something else — the
one that writes the receiving end of every wire — matched only the word
`station`. Left alone it would have gone on finding plain stations and
silently skipping every comparator and iterator, with its `--check` mode
reporting the file clean. A pattern that has to be updated alongside a
format is a second definition of that format, wherever it happens to
live.
