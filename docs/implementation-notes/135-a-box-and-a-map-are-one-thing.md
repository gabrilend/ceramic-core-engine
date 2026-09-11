# 135 — A box and a map are one thing

**Placing a box and placing a map are one operation handing back one
thing** — a receipt naming the stations that placing created. A box's
receipt holds one station; a map's holds however many its file named. The
caller cannot tell which kind it placed, and never needs to.

That was built. [140](../140-a-map-inside-a-map.md) is what it became,
and [058](../058-guarantees.md) carries the guarantee it cost: a map's
outputs are two stations on two threads at two unrelated moments, so an
embedding program's collection arrays are **not** columns of a table.

What this note keeps is the five designs that were worked out in full on
the way here and turned down. None of them is built and none is
scheduled; they are written down so the next person to reach the same
fork arrives with the map already drawn.

## Rejected: naming a field of a returned struct

`out 0.2` — output port zero, field two of the struct it carries.

**The design.** A box returns a struct. The map's output marks name a
port *and a field within it*, so one station's single returned value can
appear at the map's boundary as several separate outputs.

**Why it would work.** C guarantees struct members are laid out in
declaration order (C11 §6.7.2.1), so field two is always after field one,
on every compiler. Padding is not guaranteed, but the engine never
guesses at it — the generator computes real offsets with `offsetof` at
build time and publishes them in the field table it already maintains for
reading constants from text.

**What it buys, and it is the only thing that buys it.** Outputs that
stay together. Both fields came from one invocation of one box, so an
embedding program collecting them into two arrays would find entry three
of each genuinely related — the one case where the arrays *are* columns
of a table.

It is the map-boundary twin of the box-side design in
[506](../../issues/completed/506-multi-output-boxes.md), needing the same
machinery: a per-output offset and size within a returned value, with
delivery copying from an offset it is told rather than from the start.

**Why it is rejected: simplicity.** It adds a second addressing mode to
the output side of the map format. Every reader would have to learn that
an output can be a port, or a port and a field, and every tool touching
output marks — the parser, the dump, the canvas, the composition path —
grows a case. The pairing it buys is obtainable without it: emit the two
values as one struct and let the consumer take it apart.

**The cost of the rejection is real**: there is now no way to get paired
outputs across a map boundary. A map publishing a quotient and a
remainder as two separate outputs cannot promise they belong to each
other.

## Already rejected, twice: a box with several output ports

A fourth station kind — a box returns a struct and the station delivers
field *i* to output port *i*.

This exists in full and was already retired in
[506](../../issues/completed/506-multi-output-boxes.md), because the
parity argument that made it urgent stopped being true: a map's several
outputs are several *stations*, so a map used as a box presents several
separate output points rather than one box with several ports. Only the
efficiency case survived — a box computing two related results from
shared work has to be two boxes computing the shared part twice — and
that was judged not worth a mechanism until it showed up in a measurement
rather than an argument.

Reaching it again from a different direction added one reason: **it makes
a box and a map more different, not less.** A box's struct fields arrive
together from one call; a map's outputs arrive at different times from
different stations. Unifying on the struct would force a map to buffer
partial results until every field was present — a synchronisation the map
never promised. It also cannot skip a field: every port receives a value
every run.

The sentence 506 left behind is the one that settles it:

> A comparator's three ports and an iterator's many carry the *same*
> value routed to one place. Ports are a choice of destination, not a set
> of results.

## Rejected: out-parameter boxes

**The design.** `void divide(int a, int b, int *quotient, int *remainder)`.
The engine allocates room for both outputs inside the task, passes
pointers in, and afterwards delivers each to its own output port. The box
stays a plain C function with no emitter handle and no macro.

**Why it is rejected.** The engine checks sizes, not types. A pointer is
eight bytes and that is the whole of what the engine can see, so deciding
which eight-byte parameters are outputs means trusting a rule read off
the text of a C declaration — a heuristic in the one place the engine
currently has none. And a box taking a pointer parameter looks exactly
like one a programmer would call with the address of a local, so the
scheme's safety depends on a convention its own signature does not
express.

**The cost of the rejection**: a box that computes two new values must
return a struct, and something downstream must take it apart.

## Rejected: poison-filling output buffers

**The design.** Fill a box's output buffers with a recognisable pattern
before the call and check for it afterwards, so a box that forgot to
write an output fails loudly instead of delivering whatever bytes were
lying there.

**Why it is rejected.** It existed only to patch the out-parameter design
above, and with that gone there is nothing to detect — a C return value
always exists. It was also poor on its own terms: a memset and a memcmp
per output per invocation on the hottest path in the engine, catching a
mistake the pattern would miss whenever the poison happened to be a legal
value for that type.

## Rejected: a side file for large constant values

**The design.** Struct values move out of the map file into a separate
JSON document, referenced from the map by path — `json.client.name`.

**Why it is rejected.** It reintroduces the duplicate-spelling problem it
was meant to solve, wearing a path prefix; JSON has no fixed-width types,
so a `float` field and a `char[16]` field both round-trip badly; and it
makes a captured program two files that must travel together, when the
whole value of a dump is that it is one file you can read straight back.

**What replaces it.** Large data goes behind a box that reads it. The map
names a path as an ordinary constant, and a station whose only port holds
a constant is seeded and runs exactly once — which is what "read this at
startup" already means here, spelled as a station a reader can see.
