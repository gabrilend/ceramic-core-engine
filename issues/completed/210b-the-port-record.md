# 210b — The port record

Second child of [210](210-input-port-record.md). The shape every other
child stands on: what a port *is*, once there are two live kinds and a
third state meaning nobody has said yet.

**The statics block is cleared.** This issue was blocked in half on
[401](401-static-ports.md) and [405](405-statics-mutation.md): a port
could not be given room for a value that lived in a global register,
and the map file's `$n` form named a table the design had decided to
delete.

**The cycle resolved the way this file argued it should.** 401's first
step had read *take the port's own storage from 210b, which provides
it*, so both issues were waiting on each other. 401 went first and
built the storage it spends, because whoever deletes the table is the
one who has to know what replaces it. That work is done, and the
static-facing steps below unblocked with it.

What remains unbuilt here is one thing and it is small: the map file
form for an unconfigured port. See the open question.

## Current behavior

**The tag has three values, and the third is built.** *None* means a
port nobody has given a source to. The readiness dispatch answers no
for it forever, so a station holding one can never run no matter what
arrives at its other ports, and becomes runnable the moment that port
is given a source — proven on one station, both ways, without being
rebuilt. Nothing outside the engine can produce one yet, because the
map file has no word for it; the way in is the conversion call.

**Slots are allocated at instantiation for every port and are never
freed until the map is.** Ten slots deep, from one named constant
beside the record, sized from the registry. A port that is a static
for the whole life of a program carries slots it never uses.

**Converting no longer destroys.** Binding a static used to free the
slot array and null the pointer; it now writes the tag and nothing
else. Values a producer had already handed over and nobody had claimed
survive the port becoming something else, and are served if it becomes
a ring buffer again.

**A port can be told its own starting depth**, and a port never told
gets ten. This landed as its own call rather than as an argument on
the call that creates the station — that array would have been null at
roughly eighty existing call sites across the tests and the phase
demos, and [210g](210g-one-way-to-build-a-station.md) is about to make
configuring a port a single operation naming a station, a port, and
what it becomes, which is the shape this already has.

The first thing the depth call was used for was fixing a measurement:
see [210c](210c-a-state-on-every-slot.md), where the delivery baseline
turned out to be measuring buffer growth until the ports were sized
past what the run could fill.

**Both storages are real, and no dispatch row is an absence.** A
static's bytes live on the port beside the slots, allocated at the same
moment for the same reason, so exactly one is in effect and the other
sits idle — which is what makes changing what a port is a field write
in both directions rather than only one. The claim table's static row
was a null meaning "resolved later, outside the mutex", and it is an
ordinary function now; the caller has stopped testing a function
pointer for truth. [401](401-static-ports.md) did both.

**The map file gained a form for a value, which was half of what was
owed.** `in 1 = 5` carries a constant on the line, matching the statics
section's own `N = value`, and it is what the dump writes — a dump has
values on ports and no entry numbers to point back at.

**The map file can now write both of the things it owed.** A port with
no source at all is a bare dash, `in 2 -`, and a starting depth is
`x64` written **before** the source. The reader takes them, the dump
writes them, and a deliberately half-built program round-trips — which
it could not while the dump wrote a comment admitting the format had no
word for one.

**The depth moved from after the source to before it, during
implementation.** The `= value` form runs to the end of the line,
because a struct value has spaces in it, so nothing can follow it: a
depth written after a value would be part of the value. Before the
source is one rule that holds for all three forms, and one rule beats a
rule with an exception in it. The spelling was chosen the other way
round and corrected here; the documents say the corrected thing.

**The dump writes a depth only when it is not the default**, because
this format writes exceptions and a port at ten slots is not one. It
also means a port whose only exception *is* its depth now gets a real
line rather than a comment — `in 1 x64 -` where a buffered port at the
default still gets a comment saying what it derived.

**Still ahead, and not this issue's:** a program where *nothing* can
start is still refused at load by the seed sweep, so the round-trip
test builds a program that is partly wired rather than wholly unwired.
That is what a program mid-assembly actually looks like, and the
wholly-unwired case belongs to
[212](212-one-way-to-build-a-program.md), where the seed sweep stops
being a phase.

## Intended behavior

**Both storages live on the port; the tag says which is in effect.**
Ring slots and a static's bytes both have room, exactly one is
current, and the other sits idle. This is the whole trick that makes
[210f](210f-changing-what-a-port-is.md) a field write.

**Slots are allocated when the station is instantiated, for every port
regardless of what that port is currently for.** The element size is
known from the registry at placement, so the space is exactly right,
and a buffer standing ready is what removes allocation from every
later conversion. A port that is a static for the whole life of a
program carries slots it never uses, and that is the price: it is
paid once, at startup, in the cheapest moment a program has.

**Every port's ring buffer starts at ten values.** Ten slots of that
port's element size, so a port carrying four-byte integers starts at
forty bytes and one carrying a two-hundred-byte struct starts at two
thousand. Ten is a magic number and is meant to be one: it lives as a
single named constant, and it barely matters, because a buffer that
starts too small grows to whatever depth the program actually demands
and then stops. The cost of guessing low is a slower startup, which is
the cheapest time in a program's life to be slow.

**A port may be told its own starting capacity instead**, written in
the map file or handed to the call that creates the station, with any
port not given one getting ten. It is a hint rather than a setting:
growth covers being wrong, so nobody has to be right. This is what
the map format document means when it says its no-capacity rule is
becoming *no capacity is required* — what the rule was keeping out was
a tuning number an author had to get right, not an optional one they
may supply.

**The tag has three values: ring, static, and none.**

**None means unconfigured, and a station holding one can never be
ready.** It is a state, not a value — no null is invented and nothing
is ever handed to a box — and it is what lets a program be assembled
from nothing, a station coming into existence with every port unset
and becoming runnable as its ports are given sources one at a time.

**An unconfigured port is written into a dump.** The dump's whole
value is that it says what is actually there, walking the live station
table rather than any remembered file text — so a half-built program
dumps to a faithful record of a half-built program, and reloading that
file gives the same one back. Omitting the port would be the dump
quietly lying, producing a file that loads into something different
from what was dumped; refusing to dump at all would make the tool
useless precisely when somebody is mid-construction and most wants to
see what they have. That the result cannot run is not a problem the
dump has to solve: a station with an unconfigured port simply never
becomes ready, which is the same ordinary state an unwired station is
already in.

**So the map file format needs a form for it**, since today a port is
a ring buffer unless a line says otherwise and the only exception
written is a static.

**Both dispatch tables gain a row per tag, and no row anywhere is an
absence.** The *none* row answers "not filled" and is never claimable.
The static row keeps meaning "resolved during task construction,
outside the mutex" — but as a named function that says so rather than
as a null the caller tests for.

## Suggested implementation steps

**401 is done, so nothing here is blocked any more.** Where a step
below says a piece waited on it, that wait is over; the notes are kept
because they say which pieces are built and which are not.

1. The record itself: both storages, the three-value tag, and
   accessors that read the live one so nothing outside reaches past
   the tag to a field that may not be in effect. **Built** — the ring
   storage and the tag by this issue, the static storage by
   [401](401-static-ports.md), which built the room it spends
   rather than waiting to be handed it.
2. Slots allocated at station instantiation for every port, ten deep
   from one named constant, sized from the registry.
3. The per-port starting capacity: a form in the map file, an argument
   on the creation call, and ten when neither says otherwise.
   **Built** — `x64` before the source, in the reader and the dump.
4. Both dispatch tables gain their rows. The claim table's null becomes
   a named function; the caller stops testing a function pointer for
   truth. **Built, both of them** — the *none* rows here, and the
   static row by 401, which moved the claim inside the station's mutex
   and so changed what that row says from "resolved outside the lock"
   to an ordinary copy.
5. The *none* tag as a readiness answer, with a test that a station
   holding one never becomes ready no matter what arrives at its other
   ports.
6. The map file form for an unconfigured port — a bare dash — in the
   reader and the dump together, with a round-trip test on a
   deliberately half-built program. **Built**, and it landed with step
   3 as expected, since they share a line.

## Open questions

**Answered:**

- *What does an unconfigured port look like in the file?* **A bare
  dash.** `in 2 -` is a port whose source has not been given yet.

  The three candidates were a bare dash, the word *none*, and a
  question mark. The dash wins on what the format already says
  elsewhere: no value in this format is ever a lone dash, so it cannot
  be read as one, and the dash already means *wire* on an out line — so
  `in 2 -` reads as a wire that is not there yet rather than as a value
  nobody can name. The word *none* would have been unmistakable to a
  stranger and was the runner-up; it lost because it can collide with a
  future type or box called `none`, and because every other exception
  this format writes is punctuation rather than a word.

  Omitting the line was considered and refused for the reason the dump
  exists. A port with no line would be indistinguishable from a port
  nobody had thought about, and the dump's whole value is that it says
  what is actually there.

- *And a starting depth?* **`x64`, written before the source.** So
  `in 0 x64 $0` is a port reading statics entry zero whose ring starts
  with room for sixty-four values instead of ten. It reads as *sixty-
  four of them*, the way a parts list writes a quantity.

  **It was chosen as "after the source" and corrected to "before" when
  it was built**, which is the useful part of this answer. The
  `= value` form runs to the end of the line, because a struct value
  has spaces in it — so nothing can follow it, and a depth written
  after a value would simply be part of the value. Putting it first
  gives one rule that holds for all three forms. A rule with an
  exception in it would have been the alternative, and the exception
  would have been the form people write most.

  **A depth may appear beside any of the three source forms, including
  the dash**, because depth and source are independent — every port
  owns ring slots whatever its tag currently says, which is the
  standing buffer this issue is built on. `in 1 x256 -` is a port with
  no source yet and room for two hundred and fifty-six values when it
  gets one.

  A star was briefly accepted as a second spelling and then withdrawn.
  One spelling, read and written, matching the engine's habit of having
  exactly one way to say a thing; the reader refuses `*64` rather than
  quietly taking it.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210a — The pull path removed](210a-the-pull-path-removed.md),
  which leaves the record with one fewer kind to carry
- [210f — Changing what a port is](210f-changing-what-a-port-is.md),
  which is only cheap because of the standing buffer decided here
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which needs the *none* tag to assemble a program from nothing
- [202 — Ring buffer slots](202-ring-buffer-ports.md), the
  storage this keeps
- [008 — Map file format](../../docs/008-map-file-format.md), which names
  both the unconfigured form and the optional capacity as owed
- [703 — The map dump](703-map-dump.md), which must write a
  half-built program faithfully
