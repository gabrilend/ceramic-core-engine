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

## Current behavior, in progress

**The whole-program pass is out of the loader**, which is the step
that ends *still loading* as a state only one mechanism could be in.

A caller now assembles a program by whatever route and then says it is
finished: the checks run, everything that can start without waiting
for an arrival is started, and a complaint naming every fault comes
back if anything is wrong. **Nothing is started when anything is
wrong**, because a program that runs half of what it was asked to is
worse than one that refuses. Reading a file is one caller of that,
with no privileges — it parses, places, wires, and then asks for the
program to be brought up like anybody else would.

**It is repeatable, and that is the point rather than a convenience.**
A station added to a running program is checked and started by the
next call; one already started is not started twice, which needs a
mark on the station, without which bringing a grown program up again
would hand every input-less station a second run nobody asked for.

**Where a policy belongs became clearer by moving it.** The old sweep
refused a program in which nothing could start. That is right for a
*file somebody asked to be run* — it would do nothing at all — and
wrong for a repeatable pass, where starting nothing is the ordinary
outcome of bringing up a program that has already been brought up. So
the pass reports what it started and the loader decides that zero, on
a file, is a refusal.

**And the two paths are proven to be one.** A program is written to
disk and loaded; the same program is built by calling the surface; both
are dumped and the text compared byte for byte. Not equivalent — the
same. Comparing dumps is the cheapest such proof available, because
the dump walks the live station table and writes what is actually
there, so two identical dumps are two identical tables including the
things a hand-written comparison would forget to check.

Naming a station became an operation to make that possible. It was a
thing only the loader did, in one sweep once its own lookup table had
served, which works exactly while reading a file is the only way to
build a program. A program built by calling the surface would
otherwise dump as a row of indices and could not be compared to
anything.

**Writing that proof found a round-trip bug**, which is what a proof
of this kind is for. The dump wrote a deepened buffer as a depth
followed by a dash — and a bare dash means a port with *no source*.
Two different things spelled the same way: such a program could be
written down and not read back, returning with that port unwired and
any arrow into it refused. The format gained a form for it, a depth
with nothing after it, and a test now asserts the two stay different
things across a round trip.

**And wiring is one operation now, applying one set of rules.** There
used to be two: construction had its own and live editing had another,
and they were supposed to agree. They did not — **construction's was
the weaker**, and by exactly two rules. It never asked whether the
destination was a buffer, and it never compared the widths. So a
program built by hand could contain a wire that the same program read
from a file would have been refused, which is the precise shape of
fault having two paths produces: not a crash, a *capability
difference* nobody wrote down.

The rules were never about *when*. A sink has nothing to wire from
whether or not the pool has started, and a destination that is not a
buffer has nowhere to put a value either way. So there is one
implementation, taking the rewiring lock at any moment, and three
faces on it differing only in what a caller wants done with a refusal:
one returns it, one prints it and returns a code, one stops the
program.

Moving the buffer check earlier changed where a familiar refusal comes
from — it now arrives as the wire is drawn rather than when the whole
program is checked — so it had to learn to name the station and the
port, which the whole-map version did and the per-edge version did
not. A refusal that says "the destination port" names nothing anybody
can go and look at.

**And a map builds a map.** The construction operations exist as
ordinary boxes — add a station, draw a wire, write a constant, name a
station — so a program can perform them on another program with
nothing added to the engine. A test assembles a builder, points it at
an empty program, runs it, and then brings up and runs what it made,
because a station can look right and still be unrunnable and the way
to find out is to run it.

**A program is named by where it lives**, carried as a number, because
a box takes its arguments by value and cannot reach anything — the
last global pointer to a map was deleted precisely so two programs
could run in one process without seeing each other.

**That is where this engine's one accepted risk becomes real**, and it
was decided rather than discovered, which is what
[309](completed/309-types-by-width.md) asked for when it wrote the
hazard down and declined to fix it. A wire is legal when both ends
count the same bytes; an address is eight of them and so is a double.
So the engine will accept a wire feeding any eight-byte value into the
argument saying which program to build into, and the result is not a
wrong number but a write through those bytes. The boxes refuse a null
before touching anything, which catches the likeliest mistake — an
unwired port delivers zero — and past that nothing distinguishes a
real address from any other eight bytes.
[058](../docs/058-guarantees.md) says so in its own words rather than
folded into the width non-guarantee, because the consequence differs
in kind.

**And a program can be started beside another.** Its own station
table, its own rewiring lock, its own everything — sharing only the
workers. It cannot be wired to, which is the point rather than a
limitation: a wire is a pair of indices and an index means something
only inside one table, so reaching a program started beside you is
what its entrance is for. A test starts one, feeds it, reads it, and
outlives it.

**This issue's claim about the shared pool was false as built**, and
finding out cost one line to fix. It said the pool can be shared
"because nothing about finishing requires knowing which program a task
came from". Finishing requires exactly that: it resolves a station
index, and an index means nothing without the table it indexes — so
the map came from the pool's own context, which bound one pool to one
program.

The fix was the shape the pool already had. A task carries a station
number and an exit number that the pool ferries without interpreting,
which is what keeps it ignorant of maps; the owning program joins them
as one more opaque field. **The map was already being passed to task
construction and explicitly discarded**, so carrying it cost nothing
at all. The claim is true now, and it was not before.

Still to come: composing, which merges two tables into one — the other
half of this section, and the one that needs real work rather than a
field.

## Intended behavior

**There is one surface, and it has five operations: create an empty
program, add a station, remove a station, configure an input port, and
connect or disconnect a wire.**

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
*none* tag on the port record ([210b](completed/210b-the-port-record.md)) is
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

### Composing and starting are two different acts

A program can bring another program into itself, or set one going
beside itself, and these are not variations of one thing. **The
difference falls out of what a wire is.**

**A wire is an index** — a station number and a port number, valid
forever, never a pointer. An index is only meaningful inside one
station table, and a station table lives inside a map. So two stations
can be wired together exactly when they sit in the same map, and never
otherwise.

**Composing merges.** A map that contains another map produces **one**
station table holding both sets of stations, joined at the input and
output stations that mark the seam. A parent wires across that seam
like it wires anything else, because after the merge there is no seam
to cross — there are stations with indices, the way there always were.

So maps-inside-maps is real recursion and it is **resolved entirely
when the graph is built**. It costs nothing at run time: no boundary to
check, no per-program bookkeeping, no dispatch that asks which program
a station belongs to. The nesting is a fact about how the graph was
described, not about how it runs.

**Starting is separate.** A fresh map has its own station table, its
own rewiring lock, and its own everything except the pool, which is
shared because nothing about finishing requires knowing which program
a task came from. **You cannot wire to it**, because indices do not
cross maps. You reach it the way anything outside reaches a program:
by delivering into its input station, which
[213](213-the-input-station.md) already allows without qualification —
several callers delivering to one program input is several arrows into
one port, and the engine deliberately does not remember who called.

**Which one you want follows from whether you want isolation.**

| | composed | started |
|---|---|---|
| station table | shared with the parent | its own |
| wiring across | ordinary wires | not possible |
| talking across | not needed; it is one graph | deliver into the input station |
| a wedge or a fatal edit in the child | takes the parent with it | does not |

**A program that runs other programs wants the second**, and that is
the whole of what such a thing is. It reads a description, compiles
what the description names, starts a fresh map, adds stations to *that*
map, and feeds it through its input station — never merging, so a
program it runs can be as broken as it likes.

There is no tool to build for this and no mode to add. A program that
runs other programs **is a map**, whose boxes happen to be: read a
description, compile a source, add a station, draw a wire, write a
constant. The last three are this issue's own operations, which
therefore need to exist as boxes and not only as C calls — a plain
function taking values and returning one, like everything else.

**And a box may reach a map, since a map handle travels as a value on
a wire.** That is what retiring the old prohibition bought: the handle
is given to a box the way any value is given, so nothing process-wide
comes back and several maps still cannot see each other.

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

1. **Done but for removal.** Creating a program, adding a station,
   naming one, configuring a port and wiring are one surface, and
   every one of them applies the same rules and returns a refusal
   rather than choosing its own way to complain. Removal already
   exists ([216](completed/216-removing-a-station.md)) and keeps its
   own shape for now, since it needs the quiescence sweep the others
   do not.

   The finding: the two wiring paths were **not** equivalent.
   Construction's was missing two of live editing's rules, so a
   program built by hand could hold a wire a file could not. That is
   what having two paths produces — not a crash, a capability
   difference nobody wrote down.
2. The station table starts empty and grows a run at a time
   ([211](completed/211-growing-the-station-table.md)), so adding a station is
   the ordinary path taken once per station at startup rather than a
   rare event with its own machinery.
3. Port configuration through the surface, using the record from
   [210](210-input-port-record.md), including binding a static — which
   is the operation hand placement could never perform, and the reason
   there were two paths.
4. **Done.** The whole-program checks and the seed are one pass a
   caller invokes, repeatable, seeding only what it has not already
   seeded — with a mark per station, so a second bring-up does not
   re-run what the first started.

   One thing had to be resolved rather than implemented: this issue
   and [210g](210g-one-way-to-build-a-station.md) disagreed about a
   port with no source. That issue wanted it fatal; this one says a
   station may hold one indefinitely, and
   [210b](completed/210b-the-port-record.md) made the map file able to
   spell one so a half-built program round-trips. The later two win,
   and the check reports rather than refuses.
5. Reduce the loader to a reader: each line of text becomes one call on
   the surface, stopping at the first refusal and naming the file and
   the line. The two-pass structure survives only as "resolve names
   after every station exists," not as two kinds of pass.
6. Hand placement stays and becomes the primitive — a generated
   placement function per box writes a station directly, and placing by
   name is that function being called. Built in
   [311b](311b-placement-instead-of-records.md).
7. **Done.** The operations exist as boxes — add a station, draw a
   wire, write a constant, name a station — each a plain C function
   taking values and returning one, in an ordinary box source the
   engine has no idea is special.

   A program is named by its address, carried as a number, because a
   box cannot reach anything ambient. That makes 309's recorded
   hazard real: the engine cannot tell an address from any other
   eight-byte value. Decided deliberately, refused where refusal is
   possible, and stated in 058.
8. **Done**, and it earned its keep immediately by finding a
   round-trip bug: a deepened buffer and a port with no source were
   spelled the same way in the file format, so the first could be
   written down and not read back. Naming a station had to become an
   operation for the test to be writable at all.
9. A test that a station added to a running program, then configured,
   then wired, receives values and produces them — and that everything
   already running is undisturbed across all three steps.
10. **Done.** A program starts a second beside it, feeds it through
    its entrance, reads its result, and outlives it. The test also
    proves the negative — a wire cannot reach across — and proves it
    the honest way: both programs have a station 0, they are different
    stations, and asking to wire from one into the other's index is
    simply a wire inside the first program to a station it does not
    have. Nothing refuses it on grounds of *programs*; indices just do
    not cross.

## Open questions

**Open: does the station table still hold up once one map holds many
programs' worth of boxes?**

Composing merges: a program brought into another produces **one**
station table holding both sets of stations. Nothing about that is
resolved at run time, which is the appeal — no boundary to check, no
per-program bookkeeping, no dispatch asking which program a station
belongs to. The nesting is a fact about how the graph was described
rather than about how it runs.

But the table was designed when a map was one program somebody wrote
by hand, and composing changes the numbers it lives under. Every
question below was settled at that size and should be asked again at
the other one, *before* composing is built rather than after:

- **Shelves.** Growing by adding a shelf keeps every station still,
  which is load-bearing because a station holds its own mutex. That
  was priced against a program growing a station at a time. A merge
  adds a whole program's worth at once, and the shelf size was chosen
  for the other shape entirely.
- **Wires as indices.** A wire is a pair of numbers valid forever
  inside one table. Merging means every wire in the incoming program
  is renumbered by an offset. That is mechanical, and it is also the
  first thing in this engine that ever *rewrites* a wire — worth
  looking hard at, because "an index means what it meant" has been
  true without exception until now.
- **The whole-program passes.** Bringing a program up walks every
  station and, for each, every other station's destinations, looking
  for arrows that land on it. That is quadratic in the table, which
  nobody minded at a dozen stations. Composing several programs into
  one is exactly how a table stops being a dozen stations.
- **A mutex per station.** Fine at a dozen; a merged table of
  thousands is thousands of mutexes allocated whether or not anything
  ever contends for them.
- **What a name means after a merge.** Two programs may each have a
  station called `gate`. Names are for writing a program back out as a
  file that reads in again, so a merged program with two `gate`s
  cannot be written down — which makes this a question about the file
  format as much as about the table.

None of these is known to be a problem. The point is that they were
all decided under one set of numbers and composing supplies another,
and the cheapest moment to find out is while composing is still a
design.

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
- [211 — Growing the station table](completed/211-growing-the-station-table.md),
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
