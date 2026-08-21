# Phase 2 progress — stations and the push path

Phase 2's goal: the first phase where a graph runs. Stations in one
flat table, ring-buffer slots with exact slot sizing and growth, the
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
| 201 — station table | **extended**, and to be re-asked | Flat array of fixed-size records; growing it means shelves, because a station holds its mutex. Every choice in it was priced when a map was one hand-written program; composing merges whole programs into one table, which supplies quite different numbers. The open question is in [212](completed/212-one-way-to-build-a-program.md). |
| 202 — ring-buffer slots | **extended** | Exact-size slots stay; the two indices go, and with them the copy that growth needs. |
| [203 — port buffer growth](completed/203-port-buffer-growth.md) | **replaced** | Doubling with unwrap became adding a page, once nothing computed a position from the capacity. The copy it was built around could not be made safe once the value copies leave the lock, so it had to stop existing rather than be ordered correctly. |
| 204 — readiness check | **extended** | Same check, second caller: writing a static reaches it too. The mutex around the claim goes. |
| 205 — delivery walk | **extended** | Loses the gather step and the destination snapshot; gains a destination that is a boundary. |
| 206 — task struct | complete | One exact-size allocation: shim, station, port, input copies, output. |
| 207 — hand-built maps | **being retired** | The scaffolding is absorbed into one construction surface, or deleted. |
| 208 — phase 2 demo | complete | Occupancy, overlap, both backlog kinds, fan-out cost, live backpressure. |
| [209 — the output station](completed/209-map-output-collection.md) | **complete** | A designation naming where results come from; unwired means hold, not discard, and the pile-up is shouted from the first doubling because it means nobody is collecting at all. The map file spells it and the dump writes it back. And **every program declares one** — asked when a caller says the program is finished, satisfied by a station with nothing wired into it, because the declaration is the interface and what flows through it is separate. Requiring it found that a map which builds a map could assemble a graph and not a program: marking a door had to become a box like the others. |
| [210 — what an input port is](completed/210-input-port-record.md) | **complete** | The record all three input kinds share, designed once — now two kinds plus unconfigured. Split into eight children, every one of them settled: six built, one refused, one construction surface with both its callers arrived. |
| 210a — the pull path removed | **complete** | The gatherer kind and everything reading it, taken out. Every box now runs on a worker that picked it up. |
| [210b — the port record](completed/210b-the-port-record.md) | **complete** | Both storages on every port, the three-value tag, slots allocated at instantiation whatever the port is currently for — which is what makes changing a port's source a field write. The map file learned the two forms it owed: a bare dash for a port with no source, and `x64` before the source for a starting depth, so a half-built program round-trips.
| [210c — a state on every slot](completed/210c-a-state-on-every-slot.md) | **complete** | Four states on every slot, and only one of them a compare-and-swap once it was clear which transitions have a single possible mover. Built first and exercised while the old locking still made a bug in it harmless, which is why nothing tore when the lock came off. |
| [210d — the copies leave the lock](completed/210d-the-copies-leave-the-lock.md) | **complete** | The mutex covers the slot states and nothing else; a delivering writer takes no lock at all; the copies happen either side of the hold, protected by the fact that a reserved or claimed slot belongs to exactly one worker. Check-all-then-flip-all, so no roll-back path exists. Large values fell from about 1180 ns a delivery to about 620. |
| [210e — growth adds a page](completed/210e-growth-adds-a-page.md) | **complete** | Append rather than copy, in equal-sized pages, so no slot that already exists ever moves. Taken *before* 210d's copies rather than after, because that is the order with no window in it — the dependency turned out to run between two halves of 210d rather than between two issues. |
| [210f — changing what a port is](completed/210f-changing-what-a-port-is.md) | **complete** | A field write, with waiting values left where they sit rather than freed — proven by a round trip through static that serves every value it was carrying. The tag became atomic, because a delivery reads it before taking any lock. |
| [210g — one way to build a station](completed/210g-one-way-to-build-a-station.md) | **complete** | One configuration surface: a station, a port, a source, a value — with conversion and constant-binding as cases of it, a refusal that travels instead of stopping, and the unqualified check that every parameter has somewhere to get a value. Both callers arrived: reading a file gave up its own port check, its own width check and its own name table, and live editing reaches the same operation through the construction boxes. Giving up the width check uncovered a hole in the one that survives. |
| [210h — optional parameters](completed/210h-optional-parameters.md) | refused | Refused: it would have been the only exemption to the rule that a station runs when every slot holds a value. The record of why, and where the case it reached for actually belongs. |
| [211 — growing the station table](completed/211-growing-the-station-table.md) | **complete** | Shelves: grow by adding one, so nothing already placed ever moves — mutex included, which is the whole reason. A removed place is reused before the table grows, and reading a map file is now that same growth, one station per line. |
| [212 — one way to build a program](completed/212-one-way-to-build-a-program.md) | **complete** | The capstone. Create, configure, wire — legal at any moment, with reading a file as one caller and no privileges. The whole-program pass is out of the loader and repeatable, so *still loading* is no longer a state anything can be in, and a file-built program and a surface-built one are proven to dump identically — which found a round-trip bug on its first run. A station is added, configured and wired into a *running* program and the running part loses nothing. A program can be started beside another, sharing only the workers. Wiring is one implementation with three faces. And a map builds a map, which is where the engine's one accepted risk becomes real — an address is eight bytes and so is a double. Composing is split into [217](217-a-program-inside-another.md), and splitting it corrected it from merging into instantiating. |
| [213 — the input station](213-the-input-station.md) | **mechanism built** | The other door: where arguments arrive. One mark on an ordinary station saying which way it faces, and a delivery from outside that refuses any station that is not one — which is the whole of what turns reachable internals into a surface. The map file spells it and both doors survive a round trip; two refusals that assumed the worst gained the escape clause they had always described. A program used as a box remains. |
| [214 — destinations without a lock](completed/214-destinations-without-a-lock.md) | **complete** | An immutable array published by one atomic write, so a delivery walk takes no lock and copies nothing — the last thing holding a station's mutex on the hot path. The scrapyard reclaims what a rewire replaced, using a per-worker counter that is odd inside a task and even outside it, and that same counter is what issues 310 and 216 need. |
| [215 — ports and slots](completed/215-ports-and-slots.md) | **complete** | The source called an input port a slot and called a slot a cell, backwards from what the documents said. Now a port is the standing interface, a slot is one place one value sits, and the word *cell* is gone from the project — source, documents, interface files, issue files and the project's own statement of the one rule. Taken ahead of the rest of this family so the work still to come is written in the words its own blueprints use. |
| [216 — removing a station](completed/216-removing-a-station.md) | **complete** | A station comes out and its place is reused. Cutting the wires that name it *first* is what makes a version tag on every wire unnecessary — the walk is paid once, rarely, instead of on every delivery. A removed place comes free when the sweep says nobody can still be inside a task built from it. |
| [217 — a program inside another](217-a-program-inside-another.md) | **open** | Split out of the capstone, and splitting it corrected it. A program brought inside another is a **template instantiated**, not a program merged: build its stations at the end of the table and translate its own numbers by where they landed. Nothing that exists is renumbered, so the invariant an index means what it meant is never approached. What it costs is the dump, since two instances of one description give a program two stations with one name. |

## What the phase established

**A station never moves, and a wire is an index rather than a
pointer.** Both were chosen for reasons that only paid off phases
later: runtime rewiring, and then a table that can grow. The invariant
is load-bearing in a way its issue could only half-see at the time —
a station holds its own mutex, and a mutex is identified by where it
lives, so moving one strands every thread parked on it.

**Only the storage a port points at was ever reallocated** — never the
port, never the station — which is what let a buffer grow while every
wire and every value in flight survived untouched.

**And then nothing was reallocated at all.** A buffer grows by adding
a page of slots to a list, so no slot that already exists ever moves
(210e). The stronger statement had to be reached rather than merely
preferred: a worker copying a value out of a slot it has claimed holds
no lock, because a claimed slot belongs to it alone, and moving that
slot underneath it is the one thing that ownership does not protect
against. Reallocation was safe only while the station's mutex covered
the whole copy. **The copy could not be given a correct ordering; it
had to stop existing.**

**User code must never run under a station's mutex.** One slow box
would freeze every thread delivering into that station. This is why the
claim dispatch table had rows it deliberately left empty, and the
reasoning outlived the rows.

**And then neither does any copying.** The mutex ended up covering the
slot states and nothing else: a search and one state write per port,
with no bytes moving inside it at all (210d). What protects the values
instead is **ownership rather than exclusion** — a slot in *reserved*
or *claimed* belongs to exactly one worker, and bytes nobody else may
touch need no lock around them. A delivering writer takes no lock
whatsoever, because writing touches one slot and one slot is already
atomic.

The phase's own account of this had said the values were *copied out*
before the lock released, and that the copying was what made two
invocations of one station safe. It was not: the exclusive claim was.
Once that was seen, the copy could move outside and the guarantee did
not change, because it had never rested on the copy.

**A static is the exception, and it is the exception for the same
reason the rule works.** Ownership is a claim about slots. A static is
peeked rather than taken, so nothing owns it, and the mutex is the only
thing between a claim reading that constant and somebody writing it —
so that one copy stays inside the hold.

**And now no user code runs anywhere but on a worker that picked up a
task.** 210a removed the pull path, which was the engine's one named
exception — a box run inline, on the thread of whoever was assembling
somebody else's work. The rule above was always the interesting half;
this is the other half arriving late.

**A second check is not redundancy, and 210g is where the phase found
out what it is instead.**

Reading a file kept a width check of its own, beside the one the
wiring operation applies. Taking it away — which is what "reading a
file has no privileges" means in practice — uncovered a hole in the
survivor: it asked the width question only of wires whose *source*
station had input ports, and a box that takes nothing and returns a
value has none. Every program in this project begins with one of
those. No program read from a file could ever have shown it, because
the loader's copy ran first.

So the second check was not a belt beside a brace. **It was a curtain
in front of a gap**, and the gap was only reachable by the newer of
the two paths — the one built precisely so that a program could be
assembled without a file. That is worth more than the fix: it is the
argument for one path, stated as something that happened rather than
as something preferred.

**A guarantee can be lost rather than moved, and 210a is where the
phase learned to say so.** Rewiring holds one lock across checking an
edge and installing it, because two threads adding separately-legal
edges could produce an illegal pair. After the pull path went, no two
legal edges can — every surviving rule concerns one edge and one
station's fixed shape. The test that proved it is retired with a note
saying why, rather than deleted quietly. **A property that stops being
true is worth a paragraph; a test that stops existing without one is
how a project forgets what it used to guarantee.**

**Slots are exactly the size of the parameter they feed**, so a write
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
