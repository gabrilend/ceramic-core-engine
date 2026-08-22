# Phase 4 progress — configuration

Phase 4's goal, as it was set: the two input kinds that are not
buffers. Statics — values that are simply always there — and gatherers,
the one place the engine ran backwards so a value could be fresh at the
moment it was used.

**Half of that is being undone, deliberately.** There is no pull path
any more; nothing runs backwards, and no box runs anywhere but on a
worker that picked up a task for it.
[056](../docs/implementation-notes/056-no-pull-path.md) is the record
of what pulling was for, the three timings weighed for it, and the
counting problem that ended the search. What replaced it is one
sentence: **writing a static runs the ordinary readiness check on the
station holding it**, which turns a chain of stations wired through
statics into a recalculation graph and makes construction's own writes
the thing that starts a program.

So the phase is now about configuration, and it is the phase whose
scope shrank the most.

| Issue | State | In one line |
|---|---|---|
| [401 — static values](completed/401-static-ports.md) | **complete** | The value lives on the port that reads it and the shared table is gone. Claiming happens under the station's own mutex beside the ring pops, so an invocation's inputs are all true at one instant; two ports given one file entry are independent afterwards; and two maps run in one process without seeing each other, which the table had made impossible. |
| 402 — struct constants | complete | One reader walks field tables and brace text; malformed is fatal at bind. |
| 403 — gatherer ports | **being removed** | Built and proven; the whole capability goes with the pull path. |
| 404 — chains and cycles | **being removed** | Goes with it; the push-loop finding outlives the mechanism. |
| [405 — changing a static while it runs](completed/405-statics-mutation.md) | complete | A write naming a station and a port, size-checked, under the station's own mutex — the lock the claim already takes. The process-wide map pointer went with it, which is what lets two programs run in one process. And a **wire** may now deliver into a static port, overwriting the constant: a constant that is computed rather than written down, visible in the map file because it is a property of the wire rather than of the box. |
| 406 — phase 4 demo | **needs rewriting** | Four of its five scenes demonstrate the pull path. |
| 407 — gathering at pickup | superseded | Designed a better timing for a path that then ceased to exist. |
| [408 — values back into text](408-values-back-into-text.md) | open, **partly landed** | The mirror of 402's reader, built with 401 because deleting the statics table deleted the strings the dump was echoing. What remains is a hole and a technique: there are no escape rules on either side, so a string constant holding a quote does not round-trip; and both directions are runtime walks that become generated code with the registry work. |

## What the phase actually established

**Statics were right and are staying**, with one correction: a value
belongs to the port that reads it, not to a table the whole program
shares. That table is what forced a process to hold exactly one map,
through the ambient pointer a box needed to reach it — a feature the
design already distrusted, quietly charging the engine its ability to
compose.

**One generalized reader beats one parser per type.** Issue 402 walks a
field table and brace text together, recursing into nested structs,
with every offset coming from the compiler. That decision is why struct
constants, command-line arguments, and eventually a program's results
can all be the same text going through the same code.

**Text versus bytes is the phase's real finding**, and it got sharper
after the fact. Bytes are exact only against the exact build that wrote
them; text resolves its layout when it is read, so it survives a
rebuild that would silently change what the same bytes meant. That is
why a static's value is parsed at bind rather than re-parsed at every
claim, and why a program's results are text.

**And the pull path taught something by failing.** Not that it was a
bad idea — it was the only way to make one promise true — but that a
pulled value arriving by delivery lands in a queue whose depth nothing
keeps in step with the station's other ports, and a pulled value
produced inside an invocation has to run user code somewhere it does
not belong. Between those two, the promise was not worth its price.
This engine is not for timing-critical work, and saying so plainly is
worth more than the mechanism was.
