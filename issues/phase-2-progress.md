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
| 210 — what an input port is | open | The record all three input kinds share, designed once — now two kinds plus unconfigured. |
| 211 — growing the station table | open | Shelves: grow by adding, so nothing already placed ever moves. |
| 212 — one way to build a program | open | The capstone. Create, configure, wire — legal at any moment, loading as one caller. |
| 213 — the input station | open | The other door: where arguments arrive, and what makes a program composable. |
| 214 — destinations without a lock | open | An immutable array published by one atomic write; the scrapyard reclaims the old ones. |

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
