# 213 — The input station

The other door. [209](209-map-output-collection.md) names where a
program's results come from; this names where its arguments arrive.
They are one design and it is worth reading them together.

## Current behavior

A program has no declared entry.

Values get into a running program in exactly one way that anybody
outside it can arrange: somebody with a pointer to the map calls the
delivery entry point naming a station and a port. That works, and it is
what the seed and the tests both use, but it means **the caller has to
know the program's insides.** There is no port that belongs to the
program rather than to one of its stations.

Three consequences:

**A program cannot be composed.** A parent wanting to feed a sub-program
has to reach in and name a station by whatever the sub-program's author
called it. Rename that station and every parent breaks. That is not
encapsulation; the sub-program has no surface, only internals that
happen to be reachable.

**A program cannot take arguments.** Nothing corresponds to a
parameter list. A program run from a shell gets whatever its statics
say and nothing else, so "run this with a different input file" means
editing the box file rather than passing something.

**A leaf box and a composite box do not look alike.** A C function
announces what it takes and what it returns in its signature. A program
announces neither. So the claim that a program can be used as a box is
aspirational — there is nothing on the outside of a program for a
parent to wire to.

## Intended behavior

**A program's input station is an ordinary station, designated — and
its input ports are the program's input ports.**

Same shape as anything else: input ports, one output port, and a box
that may or may not be there. What makes it an input is the
designation, which says that **these are the ports the outside world is
allowed to deliver to.** Everything after that is an ordinary delivery
travelling an ordinary wire.

**A box here is optional.** With none, the station has one input and
one output and the value crosses unchanged. With one, arguments can be
checked, combined, or reshaped on the way in — using a function
somebody would have written anyway rather than one written to satisfy
the engine.

**One output port, so one station per argument group.** A box returns
one value; a station therefore has one output port; so a program that
takes several unrelated arguments has several input stations rather
than one station with several outputs. The alternative needs a C
function returning several values, which does not exist — and faking it
with a struct that something downstream takes apart means **a special
function written to fit the engine**, which is exactly what a person
adopting this should never have to do.

Fan-out is not the same thing and is already free: one input station's
single output port may feed as many interior stations as you wire it
to, because every output port has always had a list of destinations.
One argument reaching six places needs no mechanism at all.

### Values that must stay together must travel together

**Two values delivered to two ports of the same station are not a
pair**, and this is the trap the door opens onto. A station with an
object port and a colour port, fed by two callers who each want their
own object painted their own colour, will pair them **arbitrarily** —
the first caller's object with the second caller's colour is a
perfectly legal outcome.

The reason is already written down and is deliberate:
[210](210-input-port-record.md) states that values may leave a port in
a different order than they arrived, because with positions gone a
reader takes the first ready cell its scan finds, and rollback opens
gaps wherever it happens. If order within one port is not promised,
correspondence across two ports certainly is not.

**So anything that must arrive as a unit is one struct on one port.**
The engine already supports this all the way down: the generator emits
a field table per struct, the reader turns brace text into correctly
laid-out bytes using compiler-computed offsets, and command-line
arguments in brace syntax come along free. The object and its colour
are one value, indivisible by anything the scheduler does.

The alternative — one input station per caller — also works and needs
no argument, but it scales with callers rather than with the interface,
which is the wrong axis.

This belongs in [058](../docs/058-guarantees.md) as a stated
non-guarantee beside the ordering one, because it is the form in which
that non-guarantee will actually bite somebody.

**Where "outside" is depends on who is running it, and the mechanism is
the same either way.** For a sub-program, the outside is the parent map,
delivering typed values down an ordinary wire. For a program started
from a shell, the outside is the runner, delivering the command line.
For a program embedded in a C application, the outside is that
application, handing over typed values directly.

**Command-line arguments cost nothing to build.** They arrive as text,
and the engine already has a text-to-bytes reader — the one that turns
a static's `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into correctly
laid-out bytes by walking the field table the generator emitted. Point
it at the argument list instead of at a statics line and it is the same
code, the same compiler-computed offsets, and the same error messages
naming the field that was wrong. Struct arguments on the command line
come along for free, in the brace syntax that already exists.

**The input ports need no declared types.** If input port zero feeds a
station whose box takes an `int` there, then input port zero is an
`int`, resolved from the registry and checked by the ordinary wire
check. A port fanning out to two stations that expect different types
is caught the same way. Nothing new goes in the file, which keeps the
format's standing rule that it carries no types anywhere.

**A program with no arguments has an input station with no ports**, and
that is not a special case needing special handling. It has nothing to
receive, so nothing arrives, and the program starts the way any program
starts: the statics bound during construction are writes, and a write
runs the readiness check on the station holding it
([004](../docs/004-datapath-statics.md)). The door is for arguments;
starting is a separate thing that happens everywhere.

**And a leaf box and a composite box finally look alike from outside.**
One announces its interface in a C signature, the other in two
stations. A parent wiring to either cannot tell, and has no reason to
want to.

## Suggested implementation steps

1. Box file syntax for declaring which station is the input, alongside
   the output declaration, and reader support for both.
2. **The designation, not a new station kind** — shared with
   [209](209-map-output-collection.md), since the two differ only in
   which side the boundary is on. An ordinary station carries a mark
   saying it is a door and which way it faces. The only genuinely new
   mechanism either issue needs is a station that runs no box, which is
   the ordinary shape with one input, one output, and nothing in
   between.
3. Deriving each input port's type from what it feeds, and refusing a
   port whose destinations disagree.
4. Delivery from outside: a call naming a program, a port, and a value,
   with the same per-wire type check every other delivery gets.
5. The command-line path, reusing the statics text reader, with a test
   passing a struct argument in brace syntax.
6. A test that one program is used as a box inside another — wired to
   its input ports, read from its output ports, with the parent never
   naming anything inside it.

## Open questions

**Answered:**

- *Several input stations, or one with several ports?* **Several
  stations, each with one output port** — and the reason is not the one
  the output side recorded. Readiness is a property of input ports, and
  an input station's ports on the graph side are output ports, which
  the engine has always allowed many of; a comparator has three and an
  iterator has as many as you wire. So the subset-readiness argument
  does not reach this side at all.

  What decides it is C. A box returns one value, so a station has one
  output port. Several outputs would mean a box returning several
  values, and faking that with a struct built by one box and taken
  apart by another is **a function written to fit the engine** — the
  one cost this design refuses to impose. Somebody bringing existing C
  to this should be able to bring it unchanged.

  Written up above, along with the reminder that fan-out already covers
  the case people usually mean: one argument reaching many interior
  stations is one output port with many destinations, which has always
  worked.

- *Does an input port accept several arrows, like any other port?*
  Yes, and nothing about it is special: several callers delivering to
  one program input is several arrows into one port, which every port
  in the engine has always allowed. They interleave, exactly as two
  upstream stations feeding one port already interleave.

  **The engine does not remember who called, and does not need to.**
  Where a result should go travels *in the values*, not in the identity
  of the sender. A shared factory taking an object and a colour returns
  the blue one down the blue wire because a routing station read the
  colour, not because the engine tracked which caller sent it. This is
  the same principle as ordering being wiring: everything the engine
  might have been asked to remember is instead something the graph can
  be asked to carry.

- *Does a program dump that cannot mention its outside callers still
  round-trip?* Yes. A program's file records the program's shape, and
  who is entitled to deliver into it is not part of that shape any more
  than a C function's callers are part of its signature. The dump
  round-trips the program; it was never claiming to round-trip the
  world around it.

**Still open:**
- A program run from a shell whose input station has ports that the
  command line does not fill: those ports never receive a value, so the
  stations behind them never become ready and the program sits. Is that
  a refusal at startup, a report, or simply what happens?

## Related

- [209 — The output station](209-map-output-collection.md), the same
  design pointed the other way, and where the pass-through kind is
  described
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which is what makes a program constructible and therefore composable
- [004 — Statics and recalculation](../docs/004-datapath-statics.md),
  where starting a program is explained without reference to this
- [402 — Struct constants](completed/402-struct-constants.md), whose
  text reader the command-line path reuses unchanged
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  one declaration and a note about arguments
