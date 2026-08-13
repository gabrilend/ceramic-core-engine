# 210b — The port record

Second child of [210](210-input-port-record.md). The shape every other
child stands on: what a port *is*, once there are two live kinds and a
third state meaning nobody has said yet.

## Current behavior

An input port carries a kind tag and the fields that kind needs, with
the other kind's fields sitting unused: storage, capacity, and two
indices for a ring buffer; a table entry number for a static.

**Converting between kinds destroys and rebuilds rather than
switching.** Both conversion paths in the source do the same three
things: free the cell array, null the pointer, flip the tag. So making
a ring port into a static throws away a buffer that is already exactly
the right size for the type it holds, and making it a ring port again
has to allocate a new one.

**A dispatch table answers with an absence.** Readiness asks each port
whether it holds a value; the claim table then asks each for one, and
its non-ring row is **null**, with the caller guarding it by testing
the function pointer. The meaning is "resolved later, outside the
mutex," which is a real and correct decision — but it is a decision
written as a hole, and a reader has to already know which hole means
what.

**There is no way to say a port has no source.** A port is a ring
buffer unless something converts it, so a station cannot exist in a
half-wired state. That is what stops a program being assembled from
nothing, one instruction at a time.

**A ring buffer's cells are allocated at placement** at a fixed
starting capacity, which is nearly right already — what is missing is
that the allocation belongs to *every* port rather than to ports that
happen to be rings at the time.

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
   the tag to a field that may not be in effect.
2. Cells allocated at station instantiation for every port, ten deep
   from one named constant, sized from the registry.
3. The per-port starting capacity: a form in the map file, an argument
   on the creation call, and ten when neither says otherwise.
4. Both dispatch tables gain their rows. The claim table's null
   becomes a named function; the caller stops testing a function
   pointer for truth.
5. The *none* tag as a readiness answer, with a test that a station
   holding one never becomes ready no matter what arrives at its other
   ports.
6. The map file form for an unconfigured port, in the reader and the
   dump together, with a round-trip test on a deliberately half-built
   program.

## Open questions

- What does an unconfigured port look like in the file? It has to be a
  line, because the format writes only exceptions and *unconfigured*
  is one — but the natural spellings all read like a value rather than
  like an absence, and the reader should not have to guess whether
  somebody meant it.

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
