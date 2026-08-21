# 217 — A program inside another

Split out of [212](completed/212-one-way-to-build-a-program.md), whose
ten implementation steps are built and whose intended behaviour ends
with a sentence it never got to: **a map is a box.** This is that
sentence.

## Current behavior

**Built.** A description can be brought inside a program that already
exists, as many times as anybody likes, and a parent wires to what it
brought in by its doors and nothing else.

**It is instantiating a template, not merging two programs**, and
saying it that way is what made it small. There is no second running
program picked up and carried, no handle that becomes invalid, no
table stitched onto another. There is a *description* and a table with
some number of stations in it; new stations are built for the
description's stations and wired the way it says. One description
instantiated twice is two of everything — separate stations, separate
buffers, separate constants — with nothing shared, which is the test
that says template rather than move.

**And no wire is rewritten**, so the invariant everything here rests
on is never approached.

**The offset was wrong, and the correction is the interesting part.**
This issue said a description's numbers are translated by an offset:
its third station becomes *base plus three*. That is true only while
nothing has ever been removed. Adding a station hands back a **freed
place** before it grows the table, so a program that has had removals
gets whatever holes exist, in whatever order — and the offset story
reads perfectly right up until it does not. What the reader keeps is a
small **translation table** saying where each of the description's
stations landed, and the offset is what that table degenerates to on a
program nothing has been taken out of.

**A parent is entitled to the doors and nothing else.** The handle
could reach any of an instance's stations and doing so would be
reaching inside something whose author may rename or restructure
anything that is not a door, so what the surface offers is *the nth
entrance* and *the nth way out*. The test wires two instances in
series and reads twenty-eight out of them without naming anything
inside either.

**Instantiating into a running program needed nothing added**, because
every operation it is made of was already legal at any moment.

**The operation exists as a box, in a shape chosen against a more
general one.** A box returns one value, so a handle carrying an
entrance *and* a way out is a struct of two numbers — and getting the
numbers out means a box that takes the struct and returns one field,
which is **a function written to fit the engine**, the one cost this
design refuses. So the box says what a composing map actually wants:
*put this part between here and there*. No handle escapes and nothing
needs unpacking. What it does not reach is a part with several
entrances or several ways out, which is below.

**The dump makes names unique on the way out**, which the open
question below predicted and instantiating twice made real. Two copies
of one description give two stations with one name; a *file* cannot
have that, because an arrow is written as a destination name and could
not say which one it meant. Nothing is lost by spelling a label
differently — a name is arbitrary text the engine never reads — and
the round trip is proven on a program holding two of everything.

**A station cannot be called `in`, `out` or `statics`.** The reader
dispatches on the first word of a line and those three already mean
something there. Found by naming a door `in` and being told there was
an input line before any station.

### What stood before


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
([213](completed/213-the-input-station.md),
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

*It is not, quite, and the correction is under Current behavior: a
description's stations land wherever the table had room, which is not
`base + n` on a program anything has been removed from. What the
reader keeps is a translation table, and the offset is what that table
degenerates to when nothing has been removed. Everything else this
section says holds.*

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

1. **Done**, and not at a base. The reader keeps a translation table
   saying where each of the description's stations landed, because
   adding a station hands back a freed place before it grows the
   table — so an offset is right only on a program nothing has been
   removed from.
2. **Done.** The nth entrance and the nth way out, found by asking
   which way a station faces. A parent gets those and nothing else.
3. **Done.** Two instances of one description share no station, and
   their doors are different stations.
4. **Done.** A parent wires its own source into the first instance's
   entrance, that instance's way out into the second's entrance, and
   reads twenty-eight from its own way out — naming nothing inside
   either. This is
   [209](completed/209-map-output-collection.md)'s step 7 and
   [213](completed/213-the-input-station.md)'s step 6, both of which had been
   waiting for it.
5. **Done**, and it needed nothing added.
6. **Done**, as *put this part between here and there* rather than as
   an operation returning a handle — see the reasoning above.
7. **Done, and it was the open question below arriving.** The dump
   makes names unique on the way out, so a program holding two copies
   of one description can still be written down and read back.

## Open questions

**Answered: what does the dump write when two stations share a name?**

**The first way, and it cost one pass.** A name is an arbitrary label
carrying no meaning, so nothing is lost by writing the second `gate`
as something else — the file needs labels it can tell apart, and that
is a property of the *file* rather than of the program. The names on
the program are left exactly as they are.

The two rejected answers are kept because the reasoning is the design.
Giving an instance's names a prefix when it is built would turn `gate`
into `sub.gate` in the program itself, and the format would need no
change — an arrow destination is split on its *last* dot, so
`sub.gate.0` already reads as station `sub.gate`, port 0. It was
refused for making a name mechanical again, which is the thing this
issue says it is not. Dropping names from arrows altogether would
throw away the reason a map file is readable by a person.

**Open: a part with several entrances or several ways out, from a
box.**

The C call handles it: the surface offers the *nth* door, and a
caller asks for as many as it likes. The **box** does not, because a
box returns one value and a handle carrying two station numbers is a
struct somebody downstream has to take apart — which is a function
written to fit the engine, and [209](completed/209-map-output-collection.md)
refused exactly that when a station with several output ports was
proposed.

Three shapes are worth weighing before one is built, and none is
obviously right:

- **A box per door**, taking the description and a door number and
  returning that station's index. It would have to instantiate to
  answer, so two calls would build two copies — unless the instance
  is remembered somewhere, which is state the engine does not keep.
- **A part placed with a list of wires**, given as text the way a
  constant is. Keeps one call and one value, and moves the wiring
  description into a string the engine parses, which is a second
  little language.
- **Accept the struct and the unpackers**, on the grounds that a
  handle is a value like any other and the objection was about faking
  *several returns*, not about returning a small record. This is the
  one that most deserves re-reading: `program` is already a
  one-field struct travelling on a wire, and nobody called that a
  function written to fit the engine.

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
- [213 — The input station](completed/213-the-input-station.md), whose last step
  is the same thing from the other side
- [211 — Growing the station table](completed/211-growing-the-station-table.md),
  which makes adding a run of stations ordinary
- [210g — One way to build a station](completed/210g-one-way-to-build-a-station.md),
  which made reading a file a sequence of ordinary calls
- [311d — The map becomes code](311d-the-map-becomes-code.md), where a
  description stops being parsed at run time
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  whatever answers the dump question
