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

Built here: the station table; ring-buffer input ports with exact slot
sizing and growth; the delivery path — take the mutex, write, check
readiness, claim values, release, build a task, push it; output ports
with fan-out; the task struct.

Also here: the input port record, which holds all three sources at once
so changing where an argument comes from is a field write; a station
table that starts empty and grows a shelf at a time, so no station ever
moves and no mutex is ever relocated; and the phase's capstone — one
surface for creating a station, configuring a port, and drawing a wire,
legal at any moment. Reading a program from a file becomes the first
caller of that surface rather than a mechanism of its own, which is
what makes a program usable as a box inside another program.

Maps are hand-built in C in this phase, and shims are hand-written.
Both are replaced later, and both are worth having in their crude form
first so that phase 3 and phase 6 have something already working to
plug into.

The property to test hardest is that two invocations of one station can
run concurrently without interfering — the values are claimed under the
mutex, so the second thread finds different ones.

Described by [002 — Stations and ports](002-stations-and-ports.md) and
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

## Phase 4 — Configuration

The input that is not a buffer.

Built here: static values, held by the port that reads them, including
reading a struct constant by walking the field table from phase 3;
peeking rather than consuming, so a static is always full and never
gates readiness; and writing one — from the file at construction, from
outside the graph while it runs, or down a wire whose destination
happens to be a static port.

The addition that carries the most weight is the smallest: **a write is
an event.** Writing a static runs the ordinary readiness check on the
station holding it, which is what lets a chain of stations wired
through statics behave like a recalculation graph, and what makes
construction itself the thing that starts a program.

This phase originally built a pull path as well — gatherer ports,
inline gathering, chains, and a cycle check. All of it is being
removed; [056](implementation-notes/056-no-pull-path.md) records what
it was for, the three timings considered for it, and the accounting
problem that ended it.

Described by [004 — Statics and recalculation](004-datapath-statics.md).

---

## Phase 5 — Routing kinds

Comparators and iterators, which are variations on one step of
delivery and nothing else.

Built here: the three-entry dispatch on the way out; the comparator's
extra threshold port and its three ports; three-way comparison through
the generated compare functions; the iterator's cursor, advanced under
the station mutex at enqueue time with the chosen port recorded in the
task struct.

Depends on phase 3 for compare functions and phase 4 for the static
ports that thresholds almost always use.

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
maintained in parallel with them. The set grows an introduction and a
readable record: a slideshow that shows the datapath moving rather than
describing it, and the conversation logs rendered as a book.

---

## Phase 8 — The workbench

Tools that stand outside the engine and help somebody write a program
for it. Everything up to here makes maps run; nothing yet helps anyone
compose one except a text editor.

Built here: a canvas in the browser where stations are placed, named,
given a kind and a box function, and wired; static values filled in;
every load-time rule applied as a wire is drawn rather than at startup;
and a download of the map file together with the C source for the
functions it used. Boxes come from a bundled drawer or from a C file
the page reads locally, parsed by the build's own generator compiled to
WebAssembly so the page and the build cannot disagree about what a box
is. Nothing is stored on a server.

Depends on phase 6 for the format it emits and phase 3 for the parser
it borrows — which is now a standalone C program, so the page and the
build can share one implementation rather than two that must agree.

The constraint the whole phase is held to: you can also write a map in
a text editor. The canvas is an alternative to writing the file by
hand, never a prerequisite for it. If the canvas can ever express
something the format cannot, the format is what needs fixing.

---

## Where the numbers live

This document deliberately contains no counts, sizes, or thresholds.
Worker counts, buffer capacities, and growth factors are decided in the
code and reported by the diagnostics from phase 7, so that reading this
file a year from now cannot mislead anyone about what the program
actually does.
