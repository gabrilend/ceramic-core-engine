# 209 — The output station

## Current behavior

**Built, and the map file spells it.**

A station can be designated as a place results come from, and the
designation adds exactly one rule: when its output port is wired
nowhere, values are **held** rather than discarded. Two calls take
them out — how many wait, and take the oldest — and both exist because
a caller asked to drain results cannot write that loop with only one.

**A station that returns nothing is refused as an output**, rather
than accepted and useless: it has no output port for a parent to wire
from and nothing to hold, so the mistake is almost certainly the wrong
station.

**The pile-up is shouted from the first doubling**, not summarised at
teardown like the other two. A port backing up means uneven inputs and
the task ring backing up means slow consumers; both are performance
signals worth a line at the end. Results backing up means *nobody is
collecting them at all* — a program computing into somewhere nobody is
looking — and waiting until shutdown to say so wastes the whole run.

**The test asserts both directions**, and the second matters more than
it looks: an undesignated station runs, produces, and drops the value.
That is the rule this designation is an exception to, so a test
checking only the holding half would not have shown the exception to
be narrow.

**A fourth word on a station line says it**, `result`, read and
written back, so a program's doors survive being put on disk — which
is most of what naming them was for, since a parent wires to the doors
and a reloaded program without them is one nothing can reach.

Still to come: the load-time check that what is wired into an output
type-checks, and a demonstration of a program used as a box inside
another — which now has both halves of the seam it needs.

### What stood before

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

### So: an output station is an ordinary station, designated

**It is the same shape as any other station** — several input ports fed
by the graph, one output port — and it may run a box or not. What makes
it an output is the designation, not a different mechanism. Its
**output port is the program's output port**: a parent wires from it
exactly as it would wire from any station, and when nobody is wired
there the values are held rather than discarded, which is the one rule
this designation actually adds.

**A box here is optional and ordinary.** An output station may run a
box that shapes results into whatever a caller wants to receive — and
because that is an ordinary box, **several output stations can offer
the same results in different formats, and choosing between them is
rewiring.** Or it may run nothing, in which case it is a station with
one input and one output and the value crosses unchanged.

**The readiness check applies here exactly as it applies anywhere, box
or no box.** Nothing crosses to the output port until *every* input
port holds at least one value, and then one value is taken from each.
Three values waiting on the first input and one on the second means one
complete set moves and two stay behind, and the station waits for the
second input to be fed again. This is not a rule added for doorways; it
is [204](completed/204-readiness-check.md) doing what it already does,
and the doorway gets no exemption from it.

**Which means a program's results cannot come out of step.** A program
with several things to report through one station reports them in
complete sets or not at all, and the partial set is held rather than
emitted. That falls out of using the ordinary check rather than being
arranged.

A station with no box has one input and one output, and the value
crosses unchanged. That is not a constraint the engine imposes so much
as what having no box means — several inputs need something to combine
them, and the something is a box. If you have more than one way in, you
have a function.

Three things follow, and all three are simplifications:

**The output type needs no declaring.** A program's output port carries
whatever type the station wired into it produces, checked by the same
wire check as everything else.

**Composition needs no mechanism.** A parent wires from a sub-program's
output port exactly as it would wire from any station. There is no
"finished" to detect, no per-program task counting, and one thread pool
serves every program, because nothing about finishing requires knowing
which program a task came from.

**The concurrency rule stops being a special rule.** The earlier draft
warned that an output box runs concurrently with itself, that it must
be safe on several threads at once, that it may only issue one write
per invocation, and that a file handle held as a static would never be
closed by the engine. Every word of that is still true — and it is
true of **any** box that reaches out to the world, which is where it
now lives. Being designated an output grants a box no special
protection and imposes no special burden, because the designation
changes who may wire to the station from outside and what happens to
an unwired output, and nothing else.

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
station, each with its own input ports and its one output port.

**The reason is the C function underneath.** A box returns one value,
so a station has one output port, so a program output is one station.
The alternative — one station whose output ports each owned a subset of
its input ports, fill one through three and the first output fires,
fill four and five and the second — would need a box returning several
values, which C does not have. Faking it means a struct the box builds
and something downstream takes apart, and **that is a special function
somebody has to write in order to fit the engine**, which is the thing
this design will not ask for. A box should be a function somebody was
going to write anyway.

It has a second cost that seals it: such a station would be the only
thing in the engine with several readiness checks over subsets of its
ports, where every other station has one check over all of them.
Several stations get identical behaviour from the check that already
exists, with no grouping syntax and no new shape.

What it buys is that a program has several doors out rather than one.
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

**And because the readiness check applies here too, an output station
can pile up in two places that mean opposite things.** Values stuck on
its *input* ports mean the program is producing its several results
unevenly — one branch of the graph outrunning another, and the complete
set never forming. Values stuck on its *output* port mean complete sets
are forming fine and nobody outside is taking them. The first is a
problem inside the program, the second is a problem with whoever
started it, and a report that ran them together would send somebody to
look in the wrong place. They must be named separately.

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

1. **Done.** A fourth word on a station line — `result` — with the
   reader and the dump both carrying it. Several stations may be, each
   with its one output port.

   Not `out`, which already names a port on the lines beneath a
   station. One word meaning a port in one place and a whole station
   in another reads fine and round-trips wrong.
2. **Done for the designation.** A designated station is an ordinary
   station and the delivery walk recognises the boundary: a value
   reaching an output port wired nowhere is held rather than dropped.

   The station running *no box at all* was not built and turned out
   not to be needed. A door that shapes nothing is a station running
   an identity box — an ordinary function somebody was going to write
   anyway, rather than a new kind of station. The count of things this
   engine has stays where it was.
3. **Done.** Values are held rather than discarded, and the warning
   fires from the first doubling and names the station. The two read
   differently, as required: a doorway backing up on its inputs is the
   ordinary uneven-inputs report, and one backing up on its output
   says nobody is collecting the program's results at all.
4. **Done.** Both calls: take the oldest, and say how many wait.
   Oldest first, which is the one ordering this engine can still
   honestly offer — one station produced them all in sequence, so
   unlike values leaving a port, this order means something.
5. **Done.** The dump writes the word and a test reloads a written-out
   program to prove both doors survived.
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

- *Should an output buffer have a ceiling?* **No.** The warning is the
  contract and running out of memory is the enforcement. Both
  alternatives cost complexity to buy something worse: refusing new
  results keeps the program running while silently discarding what it
  computed, which is the exact harm this port kind exists to prevent
  arriving late instead of at once; and blocking the producer stalls
  upstream on a consumer that may never come, which is
  indistinguishable from a wedge — a condition
  [106](106-stopping-on-purpose.md) establishes the engine cannot
  detect from inside.

  A ceiling would also need a number, and any number is wrong on a
  machine with more memory and wrong differently on one with less. The
  same reasoning that keeps a clock out of the shutdown path keeps a
  depth limit out of this one.

  Exhaustion is no longer a vague bad ending, either. It is an invalid
  operation, so it stops the program with the full report written on
  the way out and the exit code meaning a resource no edit can fix —
  naming every station, every buffer depth, and which one was
  swallowing everything.

  **A program deliberately drained only at the end is the caller's to
  arrange.** The engine's contract already ends at the buffer, and
  somebody who wants to run without a collector attached writes the
  watcher that drains before memory fills. That is one more piece of
  ordinary C in the place where all the other ordinary C lives, and it
  is preferable to a mechanism every program pays for so that one
  program need not be written carefully.

  **This puts a requirement on step 4.** The call that takes values out
  needs a companion that says how many are waiting, because a caller
  told to write their own drainer cannot write one that never asks how
  full the thing is.

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
