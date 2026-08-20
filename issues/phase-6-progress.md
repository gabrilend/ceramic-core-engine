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
([212](212-one-way-to-build-a-program.md)). Building a program and
editing a running one stopped being two things.

| Issue | State | In one line |
|---|---|---|
| 601 — map file parser | **losing a form, keeping its shape** | Reads and never constructs — which is exactly what lets the loader shrink to a reader. |
| 602 — loader first pass | **becoming a caller** | Stations from the registry; the "still loading" state disappears with the pre-sized table. |
| 603 — loader second pass | **becoming an ordering** | Resolve names after every station exists — permanent reason, temporary machinery. |
| 604 — load-time validation | **losing two rules** | Both were about a pull path; the whole-program remainder becomes a report, not a refusal. |
| 605 — the seed sweep | **removed entirely** | Nothing replaced it: construction's own writes are what start a program. |
| 606 — phase 6 demo | complete | One binary, three programs; edits followed; every refusal shown; everything at once. |

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
