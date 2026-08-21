# 506 — Boxes with several output ports

## Current behavior

**RETIRED. The reason this became urgent stopped being true.**

The argument was parity: a program with several output stations, used
as a box, *is* a box with several outputs — so if programs and boxes
are to be the same kind of thing, boxes need what programs already
have. That was the whole case for the mechanism.

A program's several outputs are now several **stations**
([209](../209-map-output-collection.md)), each with one output port. So a
program used as a box presents several separate output points, not one
box with several ports, and there is nothing for a box to catch up to.
A box returns one value because a C function returns one value, and
that is now simply true rather than a limitation.

**What survives is only the efficiency case**, and it is not worth a
mechanism. A box computing two related results from shared work has to
be two boxes computing the shared part twice. That is a real cost in a
real program, and if it ever shows up in a measurement rather than in
an argument, this file is where the design already exists — the return
struct, the per-port offset and size as compiler-computed expressions,
delivery copying from an offset it is told rather than from the start
of the value. The machinery it would need is still all there in the
field tables, and still unasked for.

**One thing here outlived the issue** and belongs wherever ports are
described: a comparator's three ports and an iterator's many carry the
*same* value routed to one place. Ports are a choice of destination,
not a set of results. That distinction is what makes "a box produces
one value" and "a station has several ports" both true at once, and it
is the sentence this issue was written to break.

The rest is kept as the design that was not built.

### What it was written against

A station already has several output ports — one for a plain box, three
for a comparator, as many as the map declares for an iterator. But every
one of those ports carries **the same single value**, and the station's
kind decides which port that value goes down. Ports are a choice of
destination, not a set of results.

That is because a box returns one value, because a C function returns
one value.

So there is no way for one run of one box to produce two different
results and send them to two different places. The map author's
workaround is to run two boxes over the same input, which computes the
shared part twice and states nothing about the two results belonging
together.

## Intended behavior

**A box may have several output ports, each carrying a different value,
all produced by one run.**

The reason this became urgent: a map with several output stations, used
as a box, *is* a box with several outputs. If maps and boxes are to be
the same kind of thing, boxes need what maps already have. This is the
box side catching up to the map side.

**For a compiled box, the mechanism is the return struct.** A box returns
one value, and that value may be a struct. A tag on the box declares
which of its fields leaves by which port. The generator already emits a
field table for every struct it sees, with every offset written as an
`offsetof` expression the compiler computes — so the machinery to find
field three of a returned struct exists and is already correct. Delivery
copies from an offset it is told rather than from the start of the
value.

This works for a compiled box for one specific reason: **a C function
returns once, so all of its outputs are produced at the same instant.**
There is no such thing as a box that fills port one now and port two
later. The struct is not a container of unrelated results; it is one
result with several parts, and fanning its fields to ports states
exactly that.

**For a map, the struct is the wrong mechanism, and would be wasteful.**
A map's output stations fire independently, on their own schedules. A
map whose first output station completes during the early part of a run
and whose second completes later would, under a struct return, emit a
half-filled struct on every delivery — the first half populated and the
second half meaningless, then the reverse. It would also impose a
synchronization that does not exist: nothing says the two outputs belong
to the same moment, because they do not.

**A map's output ports are simply wires.** Each output station delivers
into the parent whenever it becomes ready, independently of the others.
This needs no new mechanism at all — it is composition, already
designed in issue 209.

**Both present the same interface, which is the point.** From outside,
a station with several output ports is a station that values come out
of, on numbered wires. Whether two values on two ports came from one run
of a compiled function or from two internal stations that finished
minutes apart is the producer's business, and no downstream station can
tell or needs to. That is what encapsulation buys, and it is why a map
can be used exactly like a box without being implemented like one.

**One run can make several stations ready at once.** Each port delivers
its own value to its own destinations, so a single invocation can
complete several downstream input sets and produce several tasks. That
is a widening of the engine's central move, not a change to it.

**This is orthogonal to the routing kinds, and the combination needs a
decision.** A comparator or an iterator chooses *which one* port a
single value goes down; a multi-output box sends *different* values down
*all* of its ports. Both are meaningful and they compose confusingly —
a comparator whose value is a struct, routed by a comparison on one
field, fanning other fields elsewhere, is possible to describe and hard
to read. The first cut should refuse the combination and say so by name,
leaving the question open rather than answering it by accident.

**What a port means becomes uniform.** After this, an output port is
"one of the things this station produces", and the routing kinds become
what they always were underneath: a rule about which port is *used* on a
given run. A plain box uses port zero. A comparator uses one of three. A
multi-output box uses all of them. Same noun, three rules.

## Suggested implementation steps

1. The declaration: how a box source marks that its return struct's
   fields are separate outputs, and which field goes to which port.
   Whatever the syntax, the generator must be able to see it while
   parsing declarations it already parses.
2. Registry support: a box record carries, per port, the offset and size
   of the field that leaves by it — both as compiler-computed
   expressions, never as numbers the generator worked out.
3. Delivery: the walk copies from the port's declared offset within the
   return value rather than from its start. A single-output box is the
   same walk with offset zero and the whole size, so there is one path,
   not two.
4. Load-time refusal of a multi-output box placed as a comparator or an
   iterator, naming the station and both kinds.
5. Map file syntax for wiring several output ports of one station to
   different destinations — largely already expressible, since ports are
   already numbered in the file.
6. A test that one run of one box completes two downstream stations that
   share no other input, which is the behavior that cannot be produced
   any other way today.

## Related

- [501 — Routing dispatch](501-routing-dispatch.md), whose
  meaning of "port" this widens
- [502 — Comparator](502-comparator.md) and
  [504 — Iterator](504-iterator.md), the two kinds this must
  refuse to combine with, at first
- [304 — Struct field tables](304-struct-field-tables.md),
  which already provides the offsets this needs
- [209 — The output station](../209-map-output-collection.md), where a map
  with several outputs makes this necessary
- [005 — Routing](../../docs/005-routing.md), which needs rewriting around
  the wider meaning of a port
