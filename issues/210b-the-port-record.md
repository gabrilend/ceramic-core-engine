# 210b — The port record

Second child of [210](210-input-port-record.md). The shape every other
child stands on: what a port *is*, once there are two live kinds and a
third state meaning nobody has said yet.

**BLOCKED, in half, on [401](401-static-slots.md) and
[405](405-statics-mutation.md).** The half of this record that faces a
static cannot be built while a global statics register still owns the
bytes: the port would be given room for a value that lives somewhere
else, and the map file's `$n` form names a table the design has already
decided to delete. The half that faces a ring buffer is unaffected and
is built.

**The block runs against 401's own first step**, which reads *take the
port's own storage from 210b, which provides it — this issue spends
that room rather than building it*. Both issues cannot be second. The
cycle is broken by having 401 build the port-side storage it spends,
which is a one-line change to that step and is where the knowledge
belongs anyway: whoever deletes the table is the one who has to know
what replaces it. Until that is decided, the static-facing steps below
stay unbuilt and are marked.

## Current behavior

**The tag has three values, and the third is built.** *None* means a
port nobody has given a source to. The readiness dispatch answers no
for it forever, so a station holding one can never run no matter what
arrives at its other ports, and becomes runnable the moment that port
is given a source — proven on one station, both ways, without being
rebuilt. Nothing outside the engine can produce one yet, because the
map file has no word for it; the way in is the conversion call.

**Cells are allocated at instantiation for every port and are never
freed until the map is.** Ten cells deep, from one named constant
beside the record, sized from the registry. A port that is a static
for the whole life of a program carries cells it never uses.

**Converting no longer destroys.** Binding a static used to free the
cell array and null the pointer; it now writes the tag and nothing
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
see [210c](210c-a-state-on-every-cell.md), where the delivery baseline
turned out to be measuring buffer growth until the ports were sized
past what the run could fill.

**Still ahead, and blocked:**

- A static's *bytes* live in the map's statics table rather than on
  the port. The record has room for one storage and an index to the
  other, which is the honest shape of a half-finished move.
- The claim dispatch table's static row is still a null meaning
  "resolved outside the mutex". 401 moves that claim inside the walk,
  so naming the hole now means naming it twice.
- The map file has no form for an unconfigured port and no form for a
  starting depth. The dump writes an unconfigured port as a comment
  saying the format cannot yet spell it, which keeps the dump honest
  at the cost of the round trip — a half-built program is currently
  one of the things a dump cannot promise to reload.

## Intended behavior

**Both storages live on the port; the tag says which is in effect.**
Ring cells and a static's bytes both have room, exactly one is
current, and the other sits idle. This is the whole trick that makes
[210f](210f-changing-what-a-port-is.md) a field write.

**Cells are allocated when the station is instantiated, for every port
regardless of what that port is currently for.** The element size is
known from the registry at placement, so the space is exactly right,
and a buffer standing ready is what removes allocation from every
later conversion. A port that is a static for the whole life of a
program carries cells it never uses, and that is the price: it is
paid once, at startup, in the cheapest moment a program has.

**Every port's ring buffer starts at ten values.** Ten cells of that
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

1. The record itself: both storages, the three-value tag, and
   accessors that read the live one so nothing outside reaches past
   the tag to a field that may not be in effect. *The ring storage and
   the tag are built; the static storage waits on 401.*
2. Cells allocated at station instantiation for every port, ten deep
   from one named constant, sized from the registry.
3. The per-port starting capacity: a form in the map file, an argument
   on the creation call, and ten when neither says otherwise. *The
   argument is built; the file form waits, because it shares a line
   with the static form 401 is redesigning.*
4. Both dispatch tables gain their rows. The claim table's null
   becomes a named function; the caller stops testing a function
   pointer for truth. *The none rows are built. The static row's null
   waits on 401, which changes what it would say: today it means
   "resolved outside the mutex," and 401 moves that claim inside.*
5. The *none* tag as a readiness answer, with a test that a station
   holding one never becomes ready no matter what arrives at its other
   ports.
6. The map file form for an unconfigured port, in the reader and the
   dump together, with a round-trip test on a deliberately half-built
   program. *Waits on 401 with step 3, and for the same reason.*

## Open questions

- What does an unconfigured port look like in the file? It has to be a
  line, because the format writes only exceptions and *unconfigured*
  is one — but the natural spellings all read like a value rather than
  like an absence, and the reader should not have to guess whether
  somebody meant it. **This cannot be settled alone.** The three
  spellings offered — a bare dash, the word *none*, a question mark —
  were all shown beside a `$n` static line, and the static line is the
  problem: it names a numbered global register that 401 deletes. The
  whole `in` line grammar is in flux, so the absent form and the
  static form should be chosen together, once, by whoever settles the
  latter.

- Where does a static's bytes actually live, and which issue puts them
  there? 401 says this issue provides the room and 401 spends it; this
  issue is now blocked on 401. Somebody has to go first, and the note
  at the top of this file argues it should be 401.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210a — The pull path removed](completed/210a-the-pull-path-removed.md),
  which leaves the record with one fewer kind to carry
- [210f — Changing what a port is](210f-changing-what-a-port-is.md),
  which is only cheap because of the standing buffer decided here
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which needs the *none* tag to assemble a program from nothing
- [202 — Ring buffer slots](completed/202-ring-buffer-slots.md), the
  storage this keeps
- [008 — Map file format](../docs/008-map-file-format.md), which names
  both the unconfigured form and the optional capacity as owed
- [703 — The map dump](completed/703-map-dump.md), which must write a
  half-built program faithfully
