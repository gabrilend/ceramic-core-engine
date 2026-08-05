# 210 — What an input port is

Supersedes the port half of issues 202, 401, and 403, which each
designed one kind of input in isolation. This is the record all three
share, designed once — and one of the three no longer exists.

## Vocabulary, since this issue is about the record itself

- **A port** is the standing interface for one input of one station:
  where its value comes from, what type it is, how many bytes one value
  occupies, and the storage it keeps. It lives on the station for the
  life of the program.
- **A slot** is one place where one value physically sits — a cell
  inside a port's ring buffer, or the bytes reserved for one argument
  inside a task. **The port decides how a value is stored; the slot is
  where it lands.**

The source calls the port a slot, which is a naming debt this issue
does not pay off — renaming reaches the station header, delivery,
statics, the loader, the dump, and
[002](../docs/002-stations-and-slots.md), and is worth doing
deliberately rather than as a side effect.

## Current behavior

An input port carries a kind tag and the fields that kind needs, with
the fields for the other kinds sitting unused: storage, capacity, and
two indices for a ring buffer; an upstream station index for a
gatherer; a table entry number for a static.

**One of those kinds is being removed entirely.** Nothing is pulled any
more — see
[056](../docs/implementation-notes/056-no-pull-path.md) — so the
gatherer tag, the upstream index, and everything that read them are
going. What remains is a ring buffer, a static, and the state of not
being configured at all.

**Converting a port between kinds destroys and rebuilds rather than
switching.** Both conversion paths in the source do the same three
things: free the cell array, null the pointer, flip the tag. So making
a ring port into a static throws away a buffer that is already the
right size for the type it holds, and making it a ring port again would
have to allocate a new one.

**The station's mutex is held for the whole of an arrival.** A delivery
takes it, copies the value into a cell, walks every port asking whether
it holds a value, claims one from each, and only then lets go. A
station that three arrows fan into, carrying two-hundred-byte structs,
holds its lock for six hundred bytes of copying while every other
deliverer waits.

**A dispatch table answers with an absence.** Readiness asks each port
whether it holds a value; the claim table then asks each for one, and
its non-ring rows are **null**, with the caller guarding them by
testing the function pointer. The meaning is "resolved later, outside
the mutex," which was a real and correct decision — but it is a
decision written as a hole, and a reader has to already know which hole
means what.

**Two ways exist to create a station, and they can bind different
things.** Placement by name looks the box up in the registry and copies
each parameter's type name onto the corresponding port; hand placement
takes an array of element sizes and no type names at all. Binding a
static needs the type, because a static in a box file is *text* and
turning `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into bytes means
knowing the field layout. So a hand-placed station cannot bind a
static, and the reason is not a design limit — it is that phase 2's
scaffolding was never given the argument.

## Intended behavior

### Three tags, one live, and switching is a field write

The tag has three values: **ring**, **static**, and **none**. Exactly
one is in effect; the storage belonging to the other sits idle. A
port's ring cells are allocated when the station is instantiated, for
every port regardless of what that port is currently for — the element
size is known from the registry at placement, so the space is exactly
right, and a buffer standing ready is what makes changing a port's
source **a field write** rather than an allocation dance.

**Every port's ring buffer starts at ten values.** Ten cells of that
port's element size, so a port carrying four-byte integers starts at
forty bytes and one carrying a two-hundred-byte struct starts at two
thousand. Ten is a magic number and is meant to be one: it lives as a
single named constant, and it barely matters, because a buffer that
starts too small grows to whatever depth the program actually demands
and then stops. The cost of guessing low is a slower startup, which is
the cheapest time in a program's life to be slow.

**A port may be told its own starting capacity instead**, written in
the box file or handed to the call that creates the station, with any
port not given one getting ten. It is a hint rather than a setting:
growth covers being wrong, so nobody has to be right.

**None means unconfigured, and a station holding one can never be
ready.** It is a state, not a value — no null is invented and nothing
is ever handed to a box — and it is what lets a program be assembled
from nothing, a station coming into existence with every port unset and
becoming runnable as its ports are given sources one at a time.

**An unconfigured port is written into a dump.** The dump's whole value
is that it says what is actually there, walking the live station table
rather than any remembered file text — so a half-built program dumps to
a faithful record of a half-built program, and reloading that file
gives the same one back. Omitting the port would be the dump quietly
lying, producing a file that loads into something different from what
was dumped; refusing to dump at all would make the tool useless
precisely when somebody is mid-construction and most wants to see what
they have. That the result cannot run is not a problem the dump has to
solve: a station with an unconfigured port simply never becomes ready,
which is the same ordinary state an unwired station is already in.

This means the file format needs a way to say it, since today a port is
a ring buffer unless a line says otherwise and the only exception
written is a static.

**Values survive a change of source.** Switching a port's tag away from
ring leaves its cells exactly as they are — not freed, not cleared, not
drained. They are waiting if the port becomes a ring again. Discarding
them would throw away values a producer already handed over,
invisibly, which is worse than serving them slightly late.

### Every cell carries its own state, and that state is the lock

| state | meaning | who may touch it |
|---|---|---|
| **empty** | nothing here | a writer, by taking it |
| **reserved** | a writer owns it and is copying in | that writer only |
| **ready** | the bytes have landed | a reader, by taking it |
| **claimed** | a reader owns it and is copying out | that reader only |

Every transition is a single atomic compare-and-swap, so two threads
can never own one cell. That is the whole of the mutual exclusion: a
writer must not write while anyone reads or writes, a reader must not
read while anyone writes, and the state machine says so **per cell**
rather than per port. No lock is involved.

**The state lives on the cell, not in a parallel array.** A parallel
array would put every cell's state in one cache line, so a writer at
one end and a reader at the other would invalidate each other's copy on
every flip — a hardware cost no lock can remove, because it is not a
race. Carried on the cell, a writer working at cell three and a reader
working at cell zero touch different lines entirely whenever the value
is large, which is exactly when the copying being moved out of the lock
was worth moving. Small values share a line and do not care, because
their copies were never the problem.

**Cells are not cleared when released.** Every write is a memory copy
of the port's full element size, so a stale value is always completely
covered and there is no such thing as a partial write into a cell. The
guarantee is not that a cell was cleaned but that its bytes are never
read unless its state says ready, which is the state machine's entire
job. Zeroing on release would cost a full erase per claim and buy
nothing.

### The claim takes no lock

**Walk the ports in ascending index order.** At each ring port, find a
ready cell and flip it to claimed. Statics are skipped — they are
peeked, never consumed, so there is nothing to take. Reach the end
having claimed one from every ring port and the invocation is real.
Meet a ring port with nothing ready and walk back, flipping what you
claimed to ready again, and give up.

**The fixed order is what prevents livelock**, and it is the only
subtle part. Without it, two threads at a two-input station can claim
one port each, each fail on the other's port, each roll back, and
retry into the same interleaving forever — nobody blocked, nobody
progressing, and a complete input set sitting there the whole time.
Lowest index first means both reach for the same port, one wins
outright, and the loser fails at the first step having claimed nothing.

**Finding a ready cell is a scan, with a bookmark.** There are no
positions any more, so a reader starts at a hint and looks forward.
The hint advances as cells are used and is allowed to be wrong — a
stale one costs a slightly longer scan and nothing else. That is the
distinction that makes everything below work: **a position must be
exact and is therefore computed from the capacity; a hint may be wrong
and therefore is not.**

**Values may leave a port in a different order than they arrived.**
This is a real loss and it is chosen deliberately. Values reaching one
port from two upstream stations were already in whatever order the
threads produced them, so arrival order was arbitrary to begin with;
and rollback releases cells wherever they sit, so gaps open and the
oldest occupied cell stops being findable without a search nobody wants
to pay for. It belongs in [058](../docs/058-guarantees.md) as a stated
non-guarantee, and there is a passing test asserting several hundred
in-order deliveries that has to be retired on purpose rather than
discovered.

### Growth adds a page

Because nothing computes a location from the capacity, changing the
capacity disturbs nothing. A ring buffer grows by allocating another
page of cells and adding it to a short list — the same shape the
station table uses, and for the same reason. Nothing is copied, no
existing cell moves, and there is no window to get right.

That removes the whole ordering problem growth used to have. Copy the
values across and then publish, and a value popped from the old storage
during the copy exists in both places and gets delivered twice; publish
first and then copy, and readers see an empty buffer while it fills.
Neither order is safe, because the real requirement was that nothing
else happen at all during the copy. With nothing copied, there is
nothing to protect.

### A static lives on the port, and writing one is an event

There is no statics table. Binding a static copies the value into the
port that reads it, and from that moment the file's entry has done its
job. Claiming happens in the same window as the ring pop, so the static
half of an input set is as mutually consistent as the buffered half —
one lock instead of two.

**Writing a static runs the readiness check on its station.** That is
what replaces the pull path, and it is one addition rather than a
subsystem: a chain of stations wired through static ports becomes a
recalculation graph, and construction's own writes are what start a
program. A write cannot make something run that could not run anyway,
because the check it triggers is the ordinary one and an empty ring
port still answers no.

### What is left for the station's mutex

Four things, all rare and all structural: growing a ring buffer,
rewiring, writing a static, and changing a port's tag. Nothing on the
hot path. It stays **inside the station record**, because shelves keep
station records still, so a mutex inside one never moves — the separate
paged array of locks that was considered would buy nothing and would
pack unrelated stations' locks into shared cache lines.

### One way to build a station

Configuring a port is a single operation naming a station, a port, a
source, and a value. The loader calls it while reading a file; a
debugger, a control socket, or a workbench calls the same one on a
running program. Hand placement stops being a second contract that can
bind fewer things than the first — it takes the type names the registry
already has, or it stops existing.

### Optional parameters

A box may declare a parameter optional. Because there is no spare value
inside an `int`'s range that could honestly mean "deliberately absent",
an optional parameter's type is a small generated wrapper carrying a
presence flag beside the value — so the C signature *looks* optional
rather than being an ordinary parameter that might secretly be a
sentinel. The check that a non-optional parameter is not left facing an
unconfigured port belongs at configuration time, which names the
station and the port while a person is still there to read it, rather
than on the first task built minutes into a run. The shim keeps a cheap
assertion as a backstop and never has to make a decision, so the hot
path carries no check that can only fail because of a configuration
error made much earlier.

## Suggested implementation steps

1. Remove the gatherer tag and everything reading it, following
   [056](../docs/implementation-notes/056-no-pull-path.md). This
   first, because every step below is smaller once it is gone.
2. The port record: both storages, the three-value tag, and accessors
   that read the live one. Ring cells allocated at station
   instantiation, ten deep from one named constant or as many as that
   port was told, sized from the registry.
3. Both dispatch tables gain a row per tag, with the *none* row
   answering "not filled" and never claimable, and no row anywhere
   being an absence.
4. The per-cell state, carried on the cell, every transition an atomic
   compare-and-swap — but with the copies still inside the station's
   mutex, so the state machine is proven correct while the old locking
   still guarantees it cannot matter.
5. Move the write copy out of the lock, then the read copy. Measure the
   delivery path at each step against a wide fan-in with large values,
   which is where the win is supposed to be and the only place it will
   show.
6. The lock-free claim: ascending port order, claim-or-roll-back,
   scanning from a hint. Retire the in-order delivery test in the same
   change, with the non-guarantee recorded first.
7. Paged growth, replacing the copy-and-unwrap path.
8. Conversion between tags as a single operation under the station's
   mutex, leaving the cells alone.
9. One station-construction and port-configuration surface, with the
   loader as its first caller and runtime editing as its second.
10. The configuration-time check for an unconfigured port feeding a
    non-optional parameter, then optional parameters themselves — the
    declaration, the generated wrapper type, and the generator's
    handling of both.
11. A test that a port cycles through all three tags while the program
    runs, with a station upstream delivering throughout, and nothing
    tears.
12. A test that a station with an unconfigured port never becomes
    ready, and becomes ready the moment that port is given a source.
13. A test that a program read from a file and one built by calling the
    configuration surface directly produce identical dumps.

## Open questions

- The bookmark is per port and touched by every reader. That is a
  shared write on the hot path — the thing the per-cell states were
  arranged to avoid — so it may want to be per worker, or to be updated
  only occasionally rather than on every claim.
- A writer that dies mid-copy leaves a cell reserved forever, and the
  port stalls behind it. That is the same class of failure as any lost
  value, but the state machine makes it a visible, nameable condition
  for the first time, so it could be reported rather than merely
  suffered.

## Related

- [202 — Ring buffer slots](completed/202-ring-buffer-slots.md), whose
  storage this keeps and whose exclusivity it drops
- [401 — Static input values](401-static-slots.md), whose value moves
  onto the port here
- [405 — Changing a static while it runs](405-statics-mutation.md),
  which becomes one case of changing a port
- [403 — Gatherer slots](completed/403-gatherer-slots.md), the kind
  this removes
- [211 — Growing the station table](211-growing-the-station-table.md),
  the same paging shape one level up
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  the surface that configures these
- [002 — Stations and slots](../docs/002-stations-and-slots.md), which
  this rewrites, including the word it uses for a port
- [056 — Why there is no pull path](../docs/implementation-notes/056-no-pull-path.md)
- [058 — Guarantees](../docs/058-guarantees.md), where the lost arrival
  order belongs
