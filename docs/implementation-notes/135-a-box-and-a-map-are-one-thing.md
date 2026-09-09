# 135 — A box and a map are one thing

**What is built today: none of this.** Everything below is a design
that has been decided on and not yet written, plus five designs that
were worked out in full and then rejected. Nothing here describes the
engine as it currently runs. The engine as it currently runs keeps a
box and a map firmly apart, and the whole point of the note is that it
should stop.

## The distinction, as it stands

Placing a box gives back a station index — one number.

Placing a map gives back a pair of numbers, an entrance and a result,
and throws away every door past the first of each kind.

That asymmetry is the entire difference between the two. It is not
about how many values go in, or how many come out, or what runs when.
It is about **what the act of placing something hands you back**, and
because the two hand back different shapes, everything downstream of
them has to know which it is holding.

## The collapse

Placing a box and placing a map both hand back a **receipt**: the list
of stations that placing created, in the order the description
declared them. A box's receipt holds one station. A map's receipt holds
however many its file named.

Doors are then addressed against the receipt by number — argument
zero, argument one, result zero — rather than by station index. A
box's argument zero is its first input port; a map's argument zero is
wherever its file put the mark. **The caller cannot tell which kind it
placed, and never needs to.**

That is the whole mechanism. There is no adapter, no wrapper, no
"promote a box to a map" step. The two operations were always the same
operation wearing two return types.

### What follows naturally, rather than being arranged

A map holding one box has one way in and one way out, because that is
what its single station has. Nobody enforces that; it is what counting
gives you. In the same way, a map holding four boxes may have four
ways out, and a caller wiring it up either wires all four somewhere or
leaves some unwired — and an unwired output discards, which is what
unwired outputs have always done.

**A station consumes all of its input ports together.** The readiness
rule takes one value from every port in a single claim under a single
mutex, so a box's arguments arrive as a set. A map whose arguments land
on four different stations has no such moment — each argument is
consumed by whichever station holds it, whenever that station is ready.

This is a difference in behaviour and it is not hidden, but it is also
not a rule anybody wrote. It falls out of a map with one station having
one claim, and a map with four stations having four. Treating it as a
caveat about substitution would be inventing a distinction the
mechanism does not have.

## The guarantee this costs, stated plainly

**A map's outputs are not synchronised with one another, and this is
by design.**

Two outputs of one map are two different stations, running on two
different threads, at two unrelated moments. Nothing pairs them.

The practical shape of that: an embedding program collects each output
into its own array with its own counter, and **the arrays are not
columns of a table**. Entry three of one and entry three of the other
did not come from the same input and are not related in any way. Two
arrays sitting side by side look like rows, which is exactly why this
has to be said out loud in the documentation rather than left to be
discovered.

The rule for anyone building on it: **make the outputs fungible.** If
two values have to stay together, they have to be one value.

## Rejected: naming a field of a returned struct

`out 0.2` — output port zero, field two of the struct it carries.

**The design.** A box returns a struct. The map's output marks name a
port *and a field within it*, so one station's single returned value
can appear at the map's boundary as several separate outputs.

**Why it would work.** C guarantees that struct members are laid out
in declaration order (C11 §6.7.2.1: members "have addresses that
increase in the order in which they are declared"), so field two is
always after field one, on every compiler. Padding between them is not
guaranteed, but the engine never guesses at it — the generator
computes real offsets with `offsetof` at build time and publishes them
in the field table it already maintains for reading constants from
text. The offset is sitting in a table the engine reads on every
static port bind.

**What it buys, and it is the only thing that buys it.** Outputs that
stay together. Both fields came from one invocation of one box, so an
embedding program collecting them into two arrays would find entry
three of each genuinely related — the one case where the arrays *are*
columns of a table.

**It is the map-boundary twin of the box-side design in
[506](../../issues/completed/506-multi-output-boxes.md)**, and it needs
the same machinery: a per-output offset and size within a returned
value, taken from field tables that already exist, with delivery copying
from an offset it is told rather than from the start. The difference is
only where the mark sits — 506 puts it on the box, this puts it on the
map's output.

**Why it is rejected: simplicity.** It adds a second addressing mode to
the output side of the map format. Every reader of a map file would
have to learn that an output can be a port, or a port and a field, and
every tool that touches output marks — the parser, the dump, the
canvas, the composition path — grows a case. The pairing it buys is
obtainable without it: a map that needs two values to stay together
emits them as one struct value and lets the consumer take it apart,
which is what the engine does today and what it will keep doing.

The cost of the rejection is real and worth naming: **there is now no
way to get paired outputs across a map boundary.** A map that wants to
publish a quotient and a remainder as two separate outputs cannot
promise they belong to each other. It publishes one struct output
instead, or it accepts that the two streams are independent. That is
the price of one addressing mode instead of two.

## Already rejected, twice: a box with several output ports

A fourth station kind beside plain, comparator and iterator — a box
returns a struct and the station delivers field *i* to output port *i*.

**This design already exists, in full, and was already retired**, in
[506 — Boxes with several output ports](../../issues/completed/506-multi-output-boxes.md).
It was retired because the parity argument that made it urgent stopped
being true: a map's several outputs are several *stations*, so a map
used as a box presents several separate output points rather than one
box with several ports, and there was nothing for a box to catch up to.
Only the efficiency case survived — a box computing two related results
from shared work has to be two boxes computing the shared part twice —
and that was judged not worth a mechanism until it showed up in a
measurement rather than an argument.

Reaching it again from a different direction changed nothing, and added
one reason to the pile: **it makes a box and a map more different, not
less.** A box's struct fields arrive together from one call; a map's
outputs arrive at different times from different stations. Unifying on
the struct would force a map to buffer partial results until every field
was present — a synchronisation the map never promised. It also cannot
skip a field: every port receives a value every run.

The sentence 506 left behind is the one that settles it, and it belongs
wherever ports are described:

> A comparator's three ports and an iterator's many carry the *same*
> value routed to one place. Ports are a choice of destination, not a
> set of results.

## Rejected: out-parameter boxes

**The design.** `void divide(int a, int b, int *quotient, int *remainder)`.
The engine allocates room for both outputs inside the task, passes
pointers in, and afterwards delivers each to its own output port. The
box stays a plain C function, testable on its own, with no emitter
handle and no macro. Every pointer parameter would be an output, with
no exceptions, which would also have retired the borrowed `const char *`
parameter form.

**Why it is rejected.** The engine checks sizes, not types. A pointer
is eight bytes and that is the whole of what the engine can see, so
deciding which eight-byte parameters are outputs would mean trusting a
rule read off the text of a C declaration — a heuristic in the one
place the engine currently has none. And a box taking a pointer
parameter looks exactly like one a programmer would call with the
address of a local, so the scheme's safety depends on a convention its
own signature does not express.

The cost of the rejection: **a box that computes two new values must
return a struct, and something downstream must take it apart.** That is
the state today and it works; it is only inelegant.

## Rejected: poison-filling output buffers

**The design.** Fill a box's output buffers with a recognisable pattern
before the call and check for it afterwards, so a box that forgot to
write an output fails loudly instead of delivering whatever bytes were
lying there.

**Why it is rejected.** It existed only to patch the out-parameter
design above, and with that gone there is nothing to detect — a C
return value always exists. It was also poor on its own terms: a memset
and a memcmp per output per invocation on the hottest path in the
engine, catching a mistake the pattern would miss whenever the poison
happened to be a legal value for that type.

## Rejected: a side file for large constant values

**The design.** Struct values move out of the map file into a separate
JSON document, referenced from the map by path — `json.client.name`.

**Why it is rejected.** It reintroduces the duplicate-spelling problem
it was meant to solve, wearing a path prefix; JSON has no fixed-width
types, so a `float` field and a `char[16]` field both round-trip
badly; and it makes a captured program two files that must travel
together, when the whole value of a dump is that it is one file you can
read straight back.

**What replaces it.** Large data goes behind a box that reads it. The
map names a path as an ordinary constant, and a station whose only port
holds a constant is seeded and runs exactly once — which is what
"read this at startup" already means here, spelled as a station a
reader can see.
