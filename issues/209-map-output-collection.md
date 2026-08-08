# 209 — The output station

## Current behavior

A program has no output, and no way to say where its results come from.

Values move between stations and stop there. A station whose box
returns void is a sink and produces nothing further. A station whose
output port is wired nowhere discards, which is deliberate and correct
for an unwired comparator branch. When the pool drains and the program
ends, whatever it computed exists only in whatever side effects its
boxes had.

Nothing is missing mechanically — a box can already print, append to a
file, or write a socket. What is missing is a *name*: no station is
designated as the place a program's results come from, so a program
that will be composed inside another has nowhere for the parent to wire
from, and a person reading a box file cannot tell which station is the
point of the whole thing.

The diagnostic reports are not this. They describe how the machine ran
— run counts, buffer depths, elapsed times — not what it computed.

## Intended behavior

**A program's output is a station, and it transmits rather than
computes.**

This is a correction to an earlier draft of this issue, which had the
output station be an ordinary station running an ordinary box. That
conflated two different things, and separating them is most of the
work here.

### The two things that were tangled

**Doing something with a result** — appending a line to a file, sending
a datagram, printing a record — is an ordinary box on an ordinary
station, wired into the flow like anything else. It needs no
designation and no new concept, and it was never the missing piece. A
program that writes to disk simply has a station that writes to disk.

**Saying what a program produces** is the missing piece, and it is not
a computation. It is a name for a set of ports.

### So: the output station is a pass-through

It runs no box. Its **input ports are the program's output ports** —
values arriving at them have arrived at the boundary, and a parent
wiring into a sub-program's output port reads whatever landed on the
matching input. Nothing is computed at the crossing; the values simply
change whose program they are in.

Three things follow, and all three are simplifications:

**The output type needs no declaring.** A program's output port carries
whatever type the station wired into it produces, checked by the same
wire check as everything else.

**Composition needs no mechanism.** A parent wires from a sub-program's
output port exactly as it would wire from any station. There is no
"finished" to detect, no per-program task counting, and one thread pool
serves every program, because nothing about finishing requires knowing
which program a task came from.

**The concurrency rule disappears.** The earlier draft had to warn that
an output box runs concurrently with itself, that it must be safe on
several threads at once, that it may only issue one write per
invocation, and that a file handle held as a static would never be
closed by the engine. None of that applies to something that runs no
code. It all still applies to a station whose box writes to disk —
which is the ordinary rule for any box that reaches out to the world,
stated where it belongs rather than as a special property of being an
output.

### Every program has one, even when it carries nothing

**An output station is required, and a program with nothing wired into
it emits nothing.** The parallel is a C function returning void: the
function still declares its return, and the declaration is what a
caller reads. Here the station is the declaration, and what flows
through it is a separate matter.

This is what makes a program's interface **total**. Without the
requirement there are two different ways to produce nothing — no output
station, and an output station nobody wired — and only the second is
legible. The engine cannot tell a program that deliberately does all
its work by side effect from one whose author forgot the results,
because a box is a C function and nothing about it says whether it
touches the world. Requiring the station moves that distinction from
something the engine would have to guess into something the program
states. A program that writes to disk and returns nothing declares
exactly that, and is ordinary rather than suspicious.

**The requirement is checked when the program is released to run**, not
when a file finishes being read. There is no end-of-file moment any
more — reading is a sequence of operations and several files can build
one program ([212](212-one-way-to-build-a-program.md)) — but there is
still a starting gate, where every worker parks until released
([102](completed/102-workers-and-run-loop.md)). That gate is the one
moment when a program stops being built and starts being a program, so
it is where a whole-program requirement can honestly be asked. Failing
it is an invalid operation and therefore fatal
([106](106-stopping-on-purpose.md)).

### The symmetric half

If a program has named output ports, it needs named input ports for the
same reason: a parent wiring *into* a sub-program otherwise has to
reach inside and name a station by its internal name, which is not
composition. That is the input station, and it is
[213](213-the-input-station.md) — the same design, pointed the other
way.

### Several outputs means several stations

A program that produces more than one thing has more than one output
station, each with its own inputs and its own single output port.

The alternative was one station whose output ports each owned a subset
of its input ports — fill inputs one through three and the first output
fires, fill four and five and the second does. It is expressible and it
was nearly taken, and the reason it was not is worth keeping: **it
would be the only thing in the engine with several readiness checks
over subsets of its ports.** Every other station has one check over all
of them. Several stations gets identical behaviour out of the check
that already exists, with no grouping syntax and no new shape.

What it costs is that a program has several doors out rather than one.
That turned out to be fine, and it settles something else: since a
program's several outputs are several stations, a program used as a box
never needs a box with several output ports — which retires
[506](completed/506-multi-output-boxes.md), whose whole argument was that boxes
had to catch up to what programs could do.

### An unwired output port holds its values

Not discards, and not writes.

**Discarding is right for every other port and wrong for this one.** An
unwired comparator branch is the normal case, so a port wired to
nothing throws its value away, and that is deliberate. But discarding a
program's *results* means the program did nothing. So this is the one
port kind where nothing-wired means hold on to it, and the rule and its
exception can both be true because they are about different things.

**Writing them to a file was considered at length and dropped.** The
engine would have needed a file handle, a way to append safely from
several threads, a decision about what a failed write means, a
platform-dependent default path, and a format — and it would have been
the first time the engine touched the filesystem for program data
rather than for diagnostics. All of that disappears if the values
simply stay where they are.

**Growth is warned about, every single time.** An ordinary buffer
growing means a consumer is slower than its producer, which is a
performance signal and is reported at teardown past a threshold. An
*output* buffer growing means **nobody is collecting the program's
results at all**, which is a different diagnosis and deserves to be
loud immediately rather than summarised later. It joins the two the
phase 7 report already distinguishes — a slot piling up means uneven
inputs, the task ring piling up means slow consumers — as a third.

**Reading them is the caller's business.** The engine's contract ends
at the buffer: results accumulate at the output station's port, and
whoever embedded the engine takes them out. That is the exact mirror of
writing a static from outside the graph, and together the two are a
program's whole surface to the world — everything else, including what
a file would have been for, is ordinary C written by whoever is running
the thing.

A program nobody reads from buffers until it runs out of memory. The
warning is the notice, and it fires from the first doubling.

## Suggested implementation steps

1. Box file syntax for declaring that a station is an output, and
   reader support for it. Several stations may be, and each has one
   output port.
2. The station kind that runs no box: placement, and a delivery walk
   that recognises arrival at the boundary rather than calling a shim.
3. The unwired case — hold the values rather than discarding — and the
   growth warning, which fires from the first doubling and says which
   station, unlike the ordinary growth report which waits and
   summarises.
4. The call that takes values out, from outside the graph, mirroring
   the call that writes a static in.
5. Carrying the declaration through the dump, so a dumped program still
   names its outputs and round-trips.
6. The load-time check that a declared output station exists and that
   what is wired into it type-checks — the ordinary wire check applied
   at one more place.
7. A demonstration program used as a box inside another, wired from its
   output ports, with the parent unable to tell whether the thing
   behind the port is a graph or a C function — and the same program
   run alone, buffering, warning, and drained by its caller.

## Open questions

**Answered:**

- *Does a program with no output station get refused?* Yes, and the
  reason no longer leans on the deleted seeding rule. A program may
  only be wired into and out of through its input and output stations,
  so those stations are the interface, and an interface that is
  sometimes absent is not one. A program producing nothing declares it
  by having the station with nothing wired in — the same way a C
  function returning void still declares a return. Written up under
  "every program has one, even when it carries nothing" above,
  including where the check happens now that files no longer end.

- *Can anything be sequenced after a void sub-program?* No, and that is
  what void **means** here rather than a gap in it. **Ordering in this
  engine is wiring.** One thing happens after another because it
  consumes what that other produced; there is no separate notion of
  sequence anywhere in the design and none is wanted. So a program with
  nothing wired out of its output station is exactly a program nothing
  depends on, which is "produces nothing" said in the engine's own
  vocabulary.

  Where ordering genuinely matters, the answer is not a valueless token
  announcing completion — it is that the program should produce
  something, and then it is not void. That something may be as small as
  an acknowledgement, and it is still a value carrying a meaning (the
  file is written, the row is committed) rather than a bare pulse. A
  dataflow engine already spells "afterwards" as "downstream," and a
  second mechanism would give one idea two spellings.

  So the promise above stands unqualified: composition needs no
  finishing to detect, because everything anyone would detect it *for*
  is already an edge.

**Still open:**

- An output buffer that nobody drains grows until memory runs out. The
  warning names it from the first doubling, which is the honest signal
  — but whether there should also be a ceiling, and what reaching one
  would do, is unanswered. Refusing to accept more results is not
  obviously better than running out of memory, and blocking the
  producer is worse than both.

## Related

- [213 — The input station](213-the-input-station.md), the same design
  pointed the other way
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  where a program becoming usable as a box is the point
- [506 — Boxes with several output ports](completed/506-multi-output-boxes.md),
  the box side of the same question
- [205 — The delivery walk](completed/205-delivery-walk.md), which
  gains one case: a destination that is a boundary rather than a slot
- [104 — Termination by last sleeper](completed/104-termination-by-last-sleeper.md),
  untouched — an earlier draft of this issue would have added a final
  act to it, and the reason it does not is worth keeping visible
- [003 — Delivery](../docs/003-datapath-delivery.md) and
  [008 — Map file format](../docs/008-map-file-format.md), each of
  which needs a section
