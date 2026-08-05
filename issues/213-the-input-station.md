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

**A program's input station is a pass-through whose output ports are
the program's input ports.**

It runs no box. Values arriving from outside land on it and flow onward
into the graph as ordinary deliveries. Nothing is computed at the
crossing; the values change whose program they are in.

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
2. The pass-through station kind — shared with
   [209](209-map-output-collection.md), since the two differ only in
   which side the boundary is on.
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

- Can a program have several input stations, or exactly one with
  several ports? Same question as the output side and it should get the
  same answer.
- Does an input port accept several arrows, like any other port? If a
  parent and a control socket both deliver to the same program input,
  they interleave — which is ordinary for a ring port and probably
  fine, but it is worth saying rather than discovering.
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
