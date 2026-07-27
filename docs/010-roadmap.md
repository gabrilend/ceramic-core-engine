# 010 — Roadmap

The phases are clusters of functionality, not a schedule. They are
ordered by what has to exist before what — each one can be built and
tested with only the phases beneath it present. It is entirely normal
for the last issue completed in this project to belong to phase one.

Each phase ends with a demo in `issues/completed/demos/`, runnable from
the launcher script in the project root, showing what the engine can do
with everything built so far.

---

## Phase 1 — The pool

A thread pool that knows nothing about boxes. It moves opaque task
structs between a fixed set of worker threads.

Built here: the task queue as a ring of pointers, doubling when full;
workers that sleep rather than spin; the sleeper count; the re-scan by
the last worker to fall asleep; clean termination by broadcast.

Testable entirely on its own with synthetic tasks that do arithmetic
and count themselves. The race that the re-scan exists to prevent is
the thing worth writing a test around, because it is the one failure
mode that looks like success.

Nothing in this phase mentions a station.

Described by [006 — Scheduling](006-datapath-scheduling.md).

---

## Phase 2 — Stations and the push path

The first phase where a graph runs.

Built here: the station table; ring-buffer input slots with exact cell
sizing and growth; the delivery path — take the mutex, write, check
readiness, claim values, release, build a task, push it; output ports
with fan-out; the task struct.

Maps are hand-built in C in this phase, and shims are hand-written.
Both are replaced later, and both are worth having in their crude form
first so that phase 3 and phase 6 have something already working to
plug into.

The property to test hardest is that two invocations of one station can
run concurrently without interfering — the values are claimed under the
mutex, so the second thread finds different ones.

Described by [002 — Stations and slots](002-stations-and-slots.md) and
[003 — Delivery](003-datapath-delivery.md).

---

## Phase 3 — The build path

The generator, and the end of hand-written glue.

Built here: a parser that reads designated box source files and pulls
out function declarations, struct definitions, and compare functions;
emission of one shim per box; emission of the registry that maps a name
to a shim pointer and full type information; emission of a field table
per struct; emission of compare functions for the primitives.

The generator runs as part of the build. Adding a box becomes writing a
function.

Described by [007 — The build path](007-datapath-build.md).

---

## Phase 4 — The pull path and configuration

The two slot kinds that are not buffers.

Built here: static slots and the statics table, including reading a
struct constant by walking the field table from phase 3; gatherer
slots; inline gathering during task assembly; gather chains; the
cycle check that runs when a connection is made.

This is also where "a gatherer is a station with no ring-buffer slots"
becomes a real distinction the engine acts on, and where the mutex on
the statics table lands.

Described by [004 — Gathering](004-datapath-gather.md).

---

## Phase 5 — Routing kinds

Comparators and iterators, which are variations on one step of
delivery and nothing else.

Built here: the three-entry dispatch on the way out; the comparator's
extra threshold slot and its three ports; three-way comparison through
the generated compare functions; the iterator's cursor, advanced under
the station mutex at enqueue time with the chosen port recorded in the
task struct.

Depends on phase 3 for compare functions and phase 4 for the static
slots that thresholds almost always use.

Described by [005 — Routing](005-routing.md).

---

## Phase 6 — The map file

The capstone. The two halves of a program meet for the first time.

Built here: the line-oriented parser; the two-pass loader; the
name lookup table; type checking of every wire against the registry;
every load-time validation rule; the seed sweep.

At the end of this phase, a program is a directory of C functions and a
text file, and changing the shape of the program does not require
touching the C.

Described by [008 — Map file format](008-map-file-format.md) and
[009 — Loading](009-datapath-load.md).

---

## Phase 7 — Seeing inside it

Everything that makes the engine legible while it runs. None of it is
required for correctness, all of it is required for confidence.

Candidates: reporting when a ring buffer grows, since that means a
consumer is slower than its producer and memory is quietly absorbing
the difference; per-station counts of runs and time spent; a dump of
the loaded map that reads back as a map file; the worst-case gather
chain depth recorded during the load-time walk; runtime rewiring, with
the same cycle check applied at connection time.

Also the HTML documentation set at `docs/HTML/`, cross-linked and
navigable, which is deliberately deferred to here rather than built
alongside each phase — it should be generated from the documents, not
maintained in parallel with them.

---

## Where the numbers live

This document deliberately contains no counts, sizes, or thresholds.
Worker counts, buffer capacities, and growth factors are decided in the
code and reported by the diagnostics from phase 7, so that reading this
file a year from now cannot mislead anyone about what the program
actually does.
