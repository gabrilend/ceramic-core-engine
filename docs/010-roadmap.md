# 010 — Roadmap

The phases are clusters of functionality, not a schedule. They are
ordered by what has to exist before what — each can be built and tested
with only the phases beneath it present. It is entirely normal for the
last issue completed in this project to belong to phase one.

Each phase ends with a demo in `issues/completed/demos/`, runnable from
the launcher script in the project root.

---

## Phase 1 — The pool

A thread pool that knows nothing about boxes. It moves opaque task
structs between a fixed set of worker threads.

Built here: the task queue as a ring of pointers, doubling when full;
workers that sleep rather than spin; the sleeper count; the re-scan by
the last worker to fall asleep; clean termination by broadcast.

Testable on its own with synthetic tasks. The race the re-scan prevents
is the thing worth a test, because it is the one failure mode that looks
like success. Nothing in this phase mentions a station.

Described by [006 — Scheduling](006-datapath-scheduling.md).

---

## Phase 2 — Stations and the push path

The first phase where a graph runs.

Built here: the station table; ring-buffer input ports with exact slot
sizing and growth; the delivery path — take the mutex, write, check
readiness, claim values, release, build a task, push it; output ports
with fan-out; the task struct.

Also here: the input port record, holding all three sources at once so
changing where an argument comes from is a field write; a station table
that grows a shelf at a time, so no station ever moves and no mutex is
ever relocated; and the phase's capstone — one surface for creating a
station, configuring a port, and drawing a wire, legal at any moment,
with reading a file as its first caller rather than a mechanism of its
own.

Maps are hand-built in C here, and shims are hand-written. Both are
replaced later, and both are worth having in crude form first so phases 3
and 6 have something working to plug into.

The property to test hardest is that two invocations of one station can
run concurrently without interfering.

Described by [002 — Stations and ports](002-stations-and-ports.md) and
[003 — Delivery](003-datapath-delivery.md).

---

## Phase 3 — The build path

The generator, and the end of hand-written glue.

Built here: a parser that reads designated box source files and pulls out
function declarations, struct definitions, and compare functions; one
shim per box; one placement function per box, carrying every size the C
compiler folded; a field table per struct; compare functions for the
primitives; and the step that turns a map description into the
construction calls it names, so nothing parses a map while it runs.

Adding a box becomes writing a function.

Described by [007 — The build path](007-datapath-build.md).

---

## Phase 4 — Configuration

The input that is not a buffer.

Built here: static values, held by the port that reads them, including
reading a struct constant by walking the field table from phase 3;
peeking rather than consuming, so a static never gates readiness; and
writing one — from the file at construction, from outside the graph while
it runs, or down a wire whose destination is a static port.

The addition carrying the most weight is the smallest: **a write is an
event.** Writing a static runs the ordinary readiness check on the
station holding it, which is what lets a chain wired through statics
behave like a recalculation graph, and what makes construction itself the
thing that starts a program.

Described by [004 — Statics and recalculation](004-datapath-statics.md),
with [056](implementation-notes/056-no-pull-path.md) for the pull path
this phase once carried.

---

## Phase 5 — Routing kinds

Comparators and iterators, which are variations on one step of delivery
and nothing else.

Built here: the three-entry dispatch on the way out; the comparator's
extra threshold port and its three ports; three-way comparison through
the generated compare functions; the iterator's cursor, advanced under
the station mutex at enqueue time with the chosen port recorded in the
task struct.

Depends on phase 3 for compare functions and phase 4 for the static ports
that thresholds almost always use.

Described by [005 — Routing](005-routing.md).

---

## Phase 6 — The map file

The capstone. The two halves of a program meet for the first time.

Built here: the line-oriented format and its reader; every wire written
at both ends and refused when the two disagree; type checking of every
wire against the emitted sizes; the whole-program checks and the seed,
gathered into one repeatable act a caller performs when it says a program
is finished.

At the end of this phase, a program is a directory of C functions and a
text file, and changing the shape of the program does not require
touching the C.

Described by [008 — Map file format](008-map-file-format.md) and
[009 — Loading](009-datapath-load.md).

---

## Phase 7 — Seeing inside it

Everything that makes the engine legible while it runs. None of it is
required for correctness, all of it is required for confidence.

Built here: reporting when a ring buffer grows; per-station counts of
runs and time spent; a dump of the loaded map that reads back as a map
file; runtime rewiring, with every rule applied at connection time; and a
capture that writes down a running program including the values waiting
on its ports.

Also the HTML documentation set at `docs/HTML/`, deferred to here rather
than built alongside each phase because it should be generated from the
documents, not maintained in parallel with them. The set grows an
introduction and a readable record: a slideshow that shows the datapath
moving rather than describing it, and the conversation logs rendered as a
book.

---

## Phase 8 — The workbench

Tools that stand outside the engine and help somebody write a program for
it. Everything up to here makes maps run; nothing yet helps anyone
compose one except a text editor.

Built here: a canvas in the browser where stations are placed, named,
given a kind and a box function, and wired; static values filled in;
every rule applied as a wire is drawn; and a download of the map file
together with the C source for the functions it used. Boxes come from a
bundled drawer or from a C file the page reads locally, parsed by the
build's own generator compiled to WebAssembly so the page and the build
cannot disagree about what a box is. Nothing is stored on a server.

The constraint the whole phase is held to: you can also write a map in a
text editor. If the canvas can ever express something the format cannot,
the format is what needs fixing.

---

## Phase 9 — The engine leaves home

Everything above makes programs run inside this repository. This phase is
the engine becoming something somebody else can take away.

Built here: the whole engine as one translation unit and one header —
`src/cera.c` and `src/cera.h`; a header narrowed to what generated code
binds to and what a consumer calls; every remaining definition marked
`static`, which is the step the rest exists for, because a function in
the same translation unit as its callers is not a linker symbol at all;
one prefix on what is left visible; an installable handler that lets a
host hear about a refusal without letting it survive one; and a test that
builds a program with this engine in a directory that cannot see this
repository.

The two names carry no index, the project's one deliberate exception to
the numbering: an entry point called `018-station.h` announcing the
eighteenth thing to read in somebody else's tree is exactly the
awkwardness this phase removes. The reading order moves inside `cera.c`,
held by a banner at each seam.

Depends on everything, which is what makes it last: the surface it
publishes is only knowable once there is nothing further to add to it.

Described by [057 — Packaging](implementation-notes/057-packaging.md).

---

## Where the numbers live

This document deliberately contains no counts, sizes, or thresholds.
Worker counts, buffer capacities, and growth factors are decided in the
code and reported by the diagnostics from phase 7.
