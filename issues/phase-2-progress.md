# Phase 2 progress — stations and the push path

Phase 2's goal: the first phase where a graph runs. Stations in one
flat table, ring-buffer slots with exact cell sizing and growth, the
delivery path, output ports with fan-out, and the real task struct —
with maps hand-built and shims hand-written as deliberate scaffolding.

**It is also the phase that grew.** Everything built here still stands
and most of it is being extended rather than replaced: the port record
designed once instead of three times, a table that grows without moving
anything, destinations that need no lock to read, both doors a program
has, and — the capstone nobody planned for — one surface for building a
program, with loading as its first caller rather than a mechanism of
its own.

| Issue | State | In one line |
|---|---|---|
| 201 — station table | **extended** | Flat array of fixed-size records; growing it means shelves, because a station holds its mutex. |
| 202 — ring-buffer slots | **extended** | Exact-size cells stay; the two indices go, and with them the copy that growth needs. |
| 203 — slot buffer growth | **extended** | Doubling with unwrap becomes adding a page, once nothing computes a position from the capacity. |
| 204 — readiness check | **extended** | Same check, second caller: writing a static reaches it too. The mutex around the claim goes. |
| 205 — delivery walk | **extended** | Loses the gather step and the destination snapshot; gains a destination that is a boundary. |
| 206 — task struct | complete | One exact-size allocation: shim, station, port, input copies, output. |
| 207 — hand-built maps | **being retired** | The scaffolding is absorbed into one construction surface, or deleted. |
| 208 — phase 2 demo | complete | Occupancy, overlap, both backlog kinds, fan-out cost, live backpressure. |
| 209 — the output station | open | A pass-through naming where results come from; unwired means hold, not discard. |
| 210 — what an input port is | **parent, in progress** | The record all three input kinds share, designed once — now two kinds plus unconfigured. Split into eight children; see below. |
| 210a — the pull path removed | **complete** | The gatherer kind and everything reading it, taken out. Every box now runs on a worker that picked it up. |
| [210b — the port record](completed/210b-the-port-record.md) | **complete** | Both storages on every port, the three-value tag, cells allocated at instantiation whatever the port is currently for — which is what makes changing a port's source a field write. The map file learned the two forms it owed: a bare dash for a port with no source, and `x64` before the source for a starting depth, so a half-built program round-trips.
| 210c — a state on every cell | open | Four states, one atomic swap each; then the copies leave the lock, write side first. |
| [210d — the copies leave the lock](210d-the-copies-leave-the-lock.md) | open | The mutex narrows to the cell states; the value copies move outside it, protected by the fact that a claimed cell belongs to exactly one worker. Check-all-then-flip-all, so no roll-back path exists. |
| 210e — growth adds a page | open | Append rather than copy — which a claim that scans rather than computing a position makes necessary, not merely nicer. |
| 210f — changing what a port is | open | A field write, with waiting values left where they sit rather than freed. |
| 210g — one way to build a station | open | One configuration surface; a hand-built program and a loaded one dump identically. |
| [210h — optional parameters](completed/210h-optional-parameters.md) | refused | Refused: it would have been the only exemption to the rule that a station runs when every slot holds a value. The record of why, and where the case it reached for actually belongs. |
| [211 — growing the station table](completed/211-growing-the-station-table.md) | **complete** | Shelves: grow by adding one, so nothing already placed ever moves — mutex included, which is the whole reason. A removed place is reused before the table grows, and reading a map file is now that same growth, one station per line. |
| 212 — one way to build a program | open | The capstone. Create, configure, wire — legal at any moment, loading as one caller. |
| 213 — the input station | open | The other door: where arguments arrive, and what makes a program composable. |
| [214 — destinations without a lock](completed/214-destinations-without-a-lock.md) | **complete** | An immutable array published by one atomic write, so a delivery walk takes no lock and copies nothing — the last thing holding a station's mutex on the hot path. The scrapyard reclaims what a rewire replaced, using a per-worker counter that is odd inside a task and even outside it, and that same counter is what issues 310 and 216 need. |
| [215 — ports and slots](215-ports-and-slots.md) | open | A naming debt paid: the source calls a port a slot and a slot a cell, which is backwards from what every document says. One deliberate pass, reaching a filename. |
| [216 — removing a station](completed/216-removing-a-station.md) | **complete** | A station comes out and its place is reused. Cutting the wires that name it *first* is what makes a version tag on every wire unnecessary — the walk is paid once, rarely, instead of on every delivery. A removed place comes free when the sweep says nobody can still be inside a task built from it. |

## What the phase established

**A station never moves, and a wire is an index rather than a
pointer.** Both were chosen for reasons that only paid off phases
later: runtime rewiring, and then a table that can grow. The invariant
is load-bearing in a way its issue could only half-see at the time —
a station holds its own mutex, and a mutex is identified by where it
lives, so moving one strands every thread parked on it.

**Only the storage a port points at is ever reallocated.** Never the
port, never the station. That is what lets a buffer grow while every
wire and every value in flight survives untouched.

**User code must never run under a station's mutex.** One slow box
would freeze every thread delivering into that station. This is why the
claim dispatch table had rows it deliberately left empty, and the
reasoning outlived the rows.

**And now no user code runs anywhere but on a worker that picked up a
task.** 210a removed the pull path, which was the engine's one named
exception — a box run inline, on the thread of whoever was assembling
somebody else's work. The rule above was always the interesting half;
this is the other half arriving late.

**A guarantee can be lost rather than moved, and 210a is where the
phase learned to say so.** Rewiring holds one lock across checking an
edge and installing it, because two threads adding separately-legal
edges could produce an illegal pair. After the pull path went, no two
legal edges can — every surviving rule concerns one edge and one
station's fixed shape. The test that proved it is retired with a note
saying why, rather than deleted quietly. **A property that stops being
true is worth a paragraph; a test that stops existing without one is
how a project forgets what it used to guarantee.**

**Cells are exactly the size of the parameter they feed**, so a write
is a memory copy into a fixed offset with no allocation on the hot
path. Everything since has been arranged to keep that true.

**A port wired to nothing discards**, which is right for an unwired
comparator branch — the normal case, not an oversight. The one
exception is a program's own results, and it is an exception because
discarding those would mean the program did nothing.

Notes for the phase: the station layer landed as two files — structure
and motion — with the phase's mechanisms built together and proven
issue by issue, same pattern as phase 1. Building the demo surfaced the
phase's best finding: the two kinds of backlog (slot versus task ring)
and which mismatch produces which — docs 002 was corrected to match.
There is now a third kind, and it belongs beside them: an output buffer
piling up means nobody is collecting the program's results at all.
