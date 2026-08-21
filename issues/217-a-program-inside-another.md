# 217 — A program inside another

Split out of [212](completed/212-one-way-to-build-a-program.md), whose
ten implementation steps are built and whose intended behaviour ends
with a sentence it never got to: **a map is a box.** This is that
sentence.

## Current behavior

**A program can be started beside another, and cannot be brought
inside one.**

Starting beside works and is proven: a fresh program with its own
station table, its own rewiring lock, its own everything except the
workers, reached only through its doors. A test starts one, feeds it,
reads it, and outlives it
([212](completed/212-one-way-to-build-a-program.md)).

What has no answer is the other half. A program that wants to *use*
another program — not run it alongside, but wire into it and out of it
the way it wires into any station — has nowhere to put it. There is no
operation that says "and here is a graph, place it".

Everything that operation would need already exists. Stations are
added one at a time, into a table that grows without moving anything
([211](completed/211-growing-the-station-table.md)). Wires are drawn
by one operation that applies every rule at any moment. Ports are
configured by one operation. A program declares where its arguments
arrive and where its results come from
([213](213-the-input-station.md),
[209](completed/209-map-output-collection.md)). Reading a file is already
nothing but a sequence of those calls
([210g](completed/210g-one-way-to-build-a-station.md)).

## Intended behavior

**Bringing a program inside another is instantiating a template, not
merging two programs.**

This is the correction that makes the whole thing small, and getting
it the other way round is what made it look large. Nothing running is
being joined to anything else running. There is no second live program
whose stations are picked up and carried, no handle that becomes
invalid, no table stitched onto another table.

There is a **description** — a text file, or whatever else can produce
one — and there is a program with some number of stations in it
already. Instantiating the description builds new stations at the end
of that table and wires them together. It is importing a library: the
description is the header, the stations it produces are the machinery
that gets constructed, and one description can be instantiated any
number of times into the same program with nothing shared between the
copies.

**So a wire is not rewritten. It is translated.** A description says
its third station's output goes to its fifth station's second port.
Instantiated into a program that already holds fifty-five stations,
that becomes station fifty-eight to station sixty, port two. The
description is untouched and says what it always said; the numbers in
the program are the numbers that description means *here*. Nothing
that already existed is renumbered, and the invariant that an index
means what it meant is never bent — because no existing index changes.

This is what reading a file already does, with the base fixed at zero.

### The offset is the whole mechanism

A description's stations are numbered from zero in the order they are
declared. Instantiating one asks the table for a place per station,
and the first place it is given is the **base**. From there:

- station *n* of the description is station *base + n* of the program
- every arrow inside the description is drawn from *base + from* to
  *base + to*
- every arrow *into* or *out of* the instance is drawn by the parent,
  in the parent's own numbers, to the instance's doors

The reader is already written this way, which was not true a moment
ago and is the step this issue stands on. Its two passes take a base
and add it to everything; today every caller passes zero.

### What the parent wires to

**The doors, and nothing else.** A description declares which of its
stations is the entrance and which are the results, and those are the
only two things a parent has any business naming. A parent wiring into
an interior station of an instance is reaching inside, and the point
of the declaration is that reaching inside stops happening by
accident.

**And the choice has a second axis, which is where the hardware
comes in.** A station table belongs to one processor, and so does the
pool that runs it
([090](../docs/implementation-notes/090-one-table-per-processor.md)).
So things composed into one table run on one processor: composing is
the right shape for a subgraph you want *close*, and the wrong shape
for work you want spread across sockets — for which the answer is a
second program with a second table, fed through its entrance. The
choice was about isolation, that a composed program shares a fate; it
is also about locality, that a composed program shares a processor's
memory.

That is the difference between this and starting beside, and it is
worth stating as one sentence: **an instance shares the parent's
station table, so it can be wired to; a program started beside has its
own, so it cannot.** Which one somebody wants follows from whether
they want isolation. A program that runs *other people's* programs
wants the second, and can be as broken as it likes. A program that
uses a subgraph it wrote wants the first, and pays nothing at run time
for it: after instantiation there is no seam, no boundary to check, no
per-instance bookkeeping, and no dispatch asking which program a
station belongs to. There are stations with indices, the way there
always were.

### Station names are labels, and this is where that starts to matter

A station name is arbitrary text. **The engine never reads one** —
every wire is an index — and nothing mechanical anywhere depends on
one. Two stations in one program may be called the same thing and
nothing about the program is worse for it.

The one place a name does any work is inside a **description**,
because text has no indices: an arrow written down has to say
something, and what it says is a name. So names are resolved *within
the description being read* and never across it. The parser already
refuses two stations with one name in a single file, which is the rule
that makes that resolution total.

Instantiating one description twice therefore produces two `gate`
stations in one program, and that is correct and expected rather than
a collision to prevent.

**What it costs is the dump**, and that is the open question below.

## Suggested implementation steps

1. An operation that instantiates a description into an existing
   program at a base, returning the base and the count so the caller
   can find the doors. The reader's two passes already take a base;
   this is the caller that passes something other than zero.
2. Locating the instance's doors, so a parent can wire to them without
   knowing anything else about it. The instance knows which of its
   stations were declared as doors, and the parent needs those as
   indices in its own numbering.
3. Instantiating the same description twice into one program, with
   nothing shared between the two — separate stations, separate
   buffers, separate constants. This is the test that says "template"
   rather than "move".
4. A parent wiring into an instance's entrance and out of its results,
   running it, and being unable to tell that the thing behind the port
   is a graph rather than a C function — which is
   [209](completed/209-map-output-collection.md)'s step 7 and
   [213](213-the-input-station.md)'s step 6, both of which have been
   waiting for this.
5. Instantiating into a program that is already running, since every
   operation it is made of is already legal at any moment.
6. The operation as a box, so that a program can bring a program into
   a program — the same way the other construction operations became
   boxes in [212](completed/212-one-way-to-build-a-program.md).

## Open questions

**Open: what does the dump write when two stations share a name?**

The file format spells an arrow as a destination *name* and a port.
Two stations called `gate` in one program cannot both be written down
that way — a file with two `gate` lines is refused by the parser,
which is right, because an arrow to `gate` in such a file means
nothing.

So a program that instantiates one description twice runs perfectly
and cannot currently be written out and read back. Three ways out, and
the first looks right:

- **The dump makes the names unique on the way out.** A name is an
  arbitrary label carrying no meaning, so nothing is lost by writing
  the second `gate` as something else — the file needs labels it can
  tell apart, and that is a property of the *file* rather than of the
  program. Costs one pass over the names at dump time and no change
  to the format, the parser, or the engine.
- **The instance's names gain a prefix when it is built.** Turns
  `gate` into `sub.gate` in the program itself. The format needs no
  change — an arrow destination is split on its *last* dot, so
  `sub.gate.0` already reads as station `sub.gate`, port 0 — but it
  makes a name mechanical again, which is the thing this issue says
  it is not.
- **The format stops spelling arrows by name.** Honest, and it throws
  away the reason a map file is readable by a person.

**Open: what does a description arrive as?**

Reading a file is one way. The others are worth naming before the
operation's shape is fixed, because they decide whether it takes a
path, a parsed description, or something else: a description built by
calling the parser on text held in memory; a description produced by
another program; and — the case
[311d](311d-the-map-becomes-code.md) is heading for — a description
the generator has already turned into code, where instantiating is
calling a generated function rather than walking a parse tree.

## Related

- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md),
  where this was described and from which it is split
- [209 — The output station](completed/209-map-output-collection.md), whose last
  step is a program used as a box
- [213 — The input station](213-the-input-station.md), whose last step
  is the same thing from the other side
- [211 — Growing the station table](completed/211-growing-the-station-table.md),
  which makes adding a run of stations ordinary
- [210g — One way to build a station](completed/210g-one-way-to-build-a-station.md),
  which made reading a file a sequence of ordinary calls
- [311d — The map becomes code](311d-the-map-becomes-code.md), where a
  description stops being parsed at run time
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  whatever answers the dump question
