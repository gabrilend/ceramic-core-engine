# 212 — One way to build a program

The capstone of phase 2. Everything below it says how a station works;
this says how one comes to exist, and makes that the same act whether
it happens at startup, halfway through a run, or inside another
program.

## Current behavior

The shape of a program is three facts: which stations exist, where each
station's input ports get their values, and which arrows connect which
output port to which input port. Those facts come into existence
through three unrelated paths that share their rules but not their
construction, and each one can do something the other two cannot.

**Hand placement**, phase 2 scaffolding. The caller says how many
stations exist, then places each one by handing over a shim pointer, a
kind, an input count, an array of `int` element sizes, and the size of
the return value, then connects ports to destinations. It is marked in
the header as scaffolding and was deliberately kept irritating. It
cannot bind a static value to a port, because it is never given any
type names, and turning the text of a static into bytes requires
knowing the field layout. It survives only inside tests, wrapping
counters no generated box can reach.

**The loader.** It counts the station lines in a text file, allocates
the station table once at exactly that size, and places every station
by name — the moment the registry supplies the shim, the input count,
the element sizes, and the type names hand placement lacks. A second
pass resolves arrows by name, because a station may be wired to one
declared later in the file. Then whole-map validation, then the seed
sweep, then the workers are released. It can do everything, once, at
startup, from a file.

**Runtime rewiring.** Connect, disconnect, and gather-repoint on a live
map, with every per-edge rule the loader applies applied here too — the
rules were deliberately made callable per edge rather than only per map
— and with the check and the change under one rewiring lock. It works
on a running program, and it cannot add a station, because the table
was sized once and never grows.

So the loader can bind statics and nothing else can; rewiring works on
a running program and nothing else does; and nothing at all can add a
station after startup. Three routes produce identical structures — the
same station records, the same ports, the same two-integer destinations
— and are kept in agreement by hand.

There is also a concept inside the loader that exists nowhere else:
**still loading**. The table is sized before any station exists,
forward references are legal because a second pass will resolve them,
and the workers are parked until the sequence finishes. Nothing outside
the loader can be in that state, which is why nothing outside the
loader can build a program.

## Intended behavior

**There is one surface, and it has four operations: create an empty
program, add a station, configure an input port, and connect or
disconnect a wire.**

Every one of them is legal at any moment — while a program is being
read from a file, or on a running one with workers in flight — because
there is no longer any such thing as *still loading*. Every one applies
the same rules, refuses the same illegal things by name, and leaves the
program either complete or visibly incomplete, never half-built.

A station comes into existence with every port unconfigured, which is a
state it may hold indefinitely: it cannot become ready, so it cannot
run, and it becomes runnable as its ports are given sources one at a
time. That is what makes "add a station now, wire it in a moment" an
ordinary sequence rather than a window of invalidity, and it is why the
*none* tag on the port record ([210](210-input-port-record.md)) is
load-bearing here rather than a curiosity.

**A map is a box.** Once a program declares where its results come from
and construction is uniform, a program placed inside another program is
a station like any other — the parent wires into its inputs and out of
its output ports, and has no way to tell, and no reason to care, that
the thing behind the port is a graph rather than a C function. The text
file stops being a special format only one mechanism can read and
becomes one way among several to drive the surface. Reading a file,
editing a running program, and composing a sub-program become the same
calls arriving from different places.

**Adding anything to a program is one procedure with three steps, and
it is the same procedure whether the program is starting or already
running.**

1. **Create every station.** All of them, before any wiring.
2. **Draw every wire, as one step.** Names always resolve, because
   every station already exists — which is what the loader's two passes
   were for, and all that survives of them.

   Wires are attached together rather than one at a time, and the
   reason is fan-out fairness on a program that is already running.
   Attach one wire out of a producing station and values start going
   down it immediately; attach the second a moment later and it has
   already missed everything the first one received. Attaching a batch
   means every destination on a port starts from the same moment.

   This also closes the only window that would have needed output
   buffering. A station created in step 1 and fed during step 2 could
   otherwise start running before its own outgoing wires existed, and
   its results would be discarded. With the wiring done as one step
   there is no such interval, so a port wired to nothing can keep
   discarding — which it must, because an unwired comparator branch is
   the normal case and buffering it would grow without bound forever.
3. **Let the construction writes take effect.** Binding a static is a
   write, and a write runs the readiness check on its station — so this
   step is not a decision about who deserves a first task, it is the
   moment the writes made during step 1 are allowed to land.

On a program that is already running, step 2 has a side effect worth
naming rather than guarding against: a wire drawn from a station that
is currently producing values means those values start arriving
immediately, so stations may become ready and enter the pool during the
wiring step rather than waiting for step 3. That is correct and it is
not a race — a value arriving down a wire that exists is what a wire is
for.

Starting a program from a file is this procedure with a reader in front
of it: read the file, create every station it names, draw every wire it
draws, let the writes land, release the gate.

**A program with no output station is refused.** It has not said where
its results come from, which means it has not said what it is for — and
the rule that starts a program starts at the output. The error names
that rather than reporting some downstream symptom like "nothing to
enqueue," because "this program never says where its results come from"
is a sentence its author can act on.

**A cycle that nothing feeds is not an error.** It sits there, waiting
for a first value nobody has sent yet, and that costs nothing — no
worker ever picks it up, so nothing spins and nothing is consumed but
the memory it occupies. Somebody may well wire it up later, which is
the whole point of being able to build a program while it runs. It is
the same decision as an unwired station being ordinary, applied to a
larger shape.

**Step 3 is not a rule about which stations deserve a first task. It is
the construction writes taking effect.**

There used to be a seed sweep here — a pass at the end of loading that
walked the program deciding who would never run otherwise and enqueued
them. It is gone, and nothing replaced it. Binding a static is a
**write**, and a write runs the ordinary readiness check on the station
holding it ([004](../docs/004-datapath-statics.md)). A station whose
inputs are all statics is ready as soon as they are bound. So the
writes that build a program are the writes that start it, and step 3 is
where they are allowed to land.

**Why they land at the end rather than as they happen.** A static bound
during step 1 would make its station ready before step 2 had drawn any
wires, so it would run and deliver its result into a program that was
not connected yet. Holding the effects until the wiring is done is the
same reason the wiring is a batch: nothing should start while the
picture is half-drawn.

Two things fall out that used to need machinery. There is no mark to
keep, because nothing re-evaluates a condition — a write happens once
and has its effect once, so running the procedure again on a grown
program cannot re-start anything it started before. And there is no
"nothing was seeded" case to refuse: a program whose construction wrote
no statics and wired nothing simply has nothing to do, which the
termination check will report on its own without a special sentence
here.

**The whole-program checks stay, as a deliberate act.** Some rules
genuinely need everything present: the reachability the seed depends
on, and the loud warning for a buffered station no arrow feeds. Those
cannot run per operation, because a half-built program legitimately
violates them. They become a pass a caller invokes when it believes the
thing is complete, rather than a fixed step in a startup sequence — and
a caller that never invokes it gets a program that runs, which is
correct, because a program assembled at runtime may never have a moment
it considers finished.

**An unwired station is therefore ordinary, not an error.** It never
becomes ready, so it never runs, and that costs nothing while the
program is running — the readiness check fires only when a delivery
arrives, and no delivery ever arrives at a station nothing feeds. An
author may deliberately place boxes before wiring them, intending to
wire them later or from somewhere else entirely. The check that used to
shout about this at the end of loading survives as something the
whole-program pass reports when asked, which is where advice belongs.

**Whether the word "map" survives is a separate, deliberate decision.**
If a program is a box, the file describes a box, and the format is a
box file. That rename reaches the parser, the dump, six documents, and
the name of a phase, so it is worth doing on purpose and in one pass
rather than drifting into place.

## Suggested implementation steps

1. The four operations, defined as the only way structure is created,
   with the per-edge rules from
   [704](completed/704-runtime-rewiring.md) applied by all of them.
   Build them over the table as it stands, before it can grow, so the
   surface is proven while the old sizing still works.
2. The station table starts empty and grows a run at a time
   ([211](211-growing-the-station-table.md)), so adding a station is
   the ordinary path taken once per station at startup rather than a
   rare event with its own machinery.
3. Port configuration through the surface, using the record from
   [210](210-input-port-record.md), including binding a static — which
   is the operation hand placement could never perform, and the reason
   there were two paths.
4. Split the whole-program checks and the seed out of the loader into
   one pass a caller invokes, repeatable, seeding only what it has not
   already seeded.
5. Reduce the loader to a reader: each line of text becomes one call on
   the surface, stopping at the first refusal and naming the file and
   the line. The two-pass structure survives only as "resolve names
   after every station exists," not as two kinds of pass.
6. Retire hand placement, either by giving it the registry type names
   it lacks or by deleting it and giving the test harness another way
   to reach its counters.
7. A test that a program read from a file and a program built by
   calling the surface directly produce identical dumps. This is the
   proof that there is one construction path rather than two that agree
   by coincidence, and it is the cheapest such proof available because
   [703](completed/703-map-dump.md) already writes a program back out
   by walking the live table.
8. A test that a station added to a running program, then configured,
   then wired, receives values and produces them — and that everything
   already running is undisturbed across all three steps.

## Open questions

**Answered, kept because the reasoning is the design:**

- *What refuses what, when — does the caller declare whether a bad
  instruction is fatal, or does the reader decide on its own?* Neither.
  There is one policy and it is fatal. An invalid operation ends the
  program, having first gathered everything it can say about what went
  wrong: stop handing out new work, quiet what can be quieted, collect
  the reasons, say them, and only then die.

  This overturns the choice rewiring made and reasoned in
  [704](completed/704-runtime-rewiring.md), where a refusal returned
  minus one and named itself rather than killing a running engine. The
  cost of that choice was already written down as a debt in the
  first-pass report: **a caller can ignore a return value**, and an
  ignored refusal leaves a program running that somebody believes they
  just edited. Dying removes the debt instead of managing it.

  The surface still *returns* a refusal rather than dying where the
  failure happens, because the loader's rule from
  [604](completed/604-load-time-validation.md) is to collect every
  failure in a file and present them together rather than stopping on
  the first. A refusal travels upward, accumulates, and the crash
  happens once with the whole list. A single instruction arriving alone
  simply produces a list of one.

  **And "never half-built" stops needing enforcement.** A program
  cannot be left partly constructed by ignored refusals, because a
  refusal ends the process. The property that the surface could not
  guarantee on its own is now guaranteed by there being no surviving
  path in which it is violated.

- *Should the reader run the whole-program checks when it reaches the
  end of a file?* No. Those checks became a **report** — a program with
  unreachable stations is ordinary, not wrong — and a report is
  something a caller asks for. Reading a file stays only a sequence of
  operations, so several files can be read into one program.
- *Must step 3 avoid re-enqueueing what it already enqueued?* The
  question dissolved with the seed sweep. Nothing re-evaluates a
  condition: a write happens once and has its effect once.
- *Where does "nothing was seeded" refuse?* Nowhere. There is no seed
  sweep to refuse, and a program with nothing to do reports that
  through the ordinary termination check.
- *Should a refusal for out-of-memory read differently from one for a
  misspelled box name?* Yes, and not only in its wording — a misspelled
  name is something a caller can fix and retry, and out-of-memory is
  not, so anything that retries on failure must be able to tell them
  apart in code rather than in prose.

## Related

- [210 — What an input port is](210-input-port-record.md), the record
  the configure operation writes, and where the *none* state comes from
- [211 — Growing the station table](211-growing-the-station-table.md),
  which makes "add a station" possible at all
- [704 — Rewiring while it runs](completed/704-runtime-rewiring.md),
  whose per-edge rules and rewiring lock every operation here inherits
- [602 — The loader, first pass](completed/602-loader-first-pass.md)
  and [603 — The loader, second pass](completed/603-loader-second-pass.md),
  which become a reader
- [604 — Load-time validation](completed/604-load-time-validation.md),
  split into per-edge rules already reused and a whole-program pass
- [605 — The seed sweep](completed/605-the-seed-sweep.md), which stops
  being a phase
- [703 — The map dump](completed/703-map-dump.md), which becomes the
  exact inverse of the reader
- [209 — The output station](209-map-output-collection.md) and
  [506 — Boxes with several output ports](completed/506-multi-output-boxes.md),
  the two halves of a program being usable as a box
- [008 — Map file format](../docs/008-map-file-format.md) and
  [009 — Loading](../docs/009-datapath-load.md), both of which this
  rewrites
