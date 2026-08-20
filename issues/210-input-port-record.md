# 210 — What an input port is

Supersedes the port half of issues 202, 401, and 403, which each
designed one kind of input in isolation. This is the record all three
share, designed once — and one of the three no longer exists.

**This is a parent issue.** It was one ticket with thirteen
implementation steps, which is not a ticket but a phase wearing one.
The steps divided along seams that were already there: a removal, a
record, a state machine, a claim, a growth strategy, a conversion, a
construction surface, and a declaration. Each is separately
buildable, separately testable, and separately wrong-able. What stays
here is what all of them share — the vocabulary, the shared design
that no single child owns, and the questions answered once for the
whole family.

## The children

| issue | what it builds | depends on |
|---|---|---|
| [210a — The pull path removed](completed/210a-the-pull-path-removed.md) | the gatherer kind and everything reading it, taken out | — |
| [210b — The port record](completed/210b-the-port-record.md) | both storages, the three-value tag, cells allocated at instantiation | 210a |
| [210c — A state on every cell](210c-a-state-on-every-cell.md) | the four-state per-cell machine, then the copies moved out of the lock | 210b |
| [210d — The copies leave the lock](210d-the-copies-leave-the-lock.md) | the mutex narrowed to the cell states, the value copies moved outside it | 210c |
| [210e — Growth adds a page](210e-growth-adds-a-page.md) | a ring buffer that grows by appending, copying nothing | 210d |
| [210f — Changing what a port is](210f-changing-what-a-port-is.md) | conversion between tags as one operation, cells left alone | 210b |
| [210g — One way to build a station](210g-one-way-to-build-a-station.md) | a single construction and configuration surface | 210b, 210f |
| [210h — Optional parameters](completed/210h-optional-parameters.md) | refused — the record of why a parameter cannot be optional | 210g |

The order is real rather than tidy. **210c through 210e must land in
that sequence**, because each removes the reason the next one was
hard: per-cell states are what let the value copies leave the lock, and
a claim that scans rather than computes a position is what makes
growth-by-appending safe. **210f and 210g branch off 210b** and can
be built while the concurrency line is in progress.

## The statics blocker, which is cleared

**[401](completed/401-static-slots.md) is done and this family is no
longer cut in two.** The record below is kept because the deadlock it
describes was real and the way out of it is the useful part.

Half of this family waited on 401 and
[405](405-statics-mutation.md), which are not children of this issue
and were expected to stand on it rather than the reverse.

The reason was one sentence: **a port cannot be given room for a static
while a global register still owns the bytes.** 210b would have been
building a place for a value that lived somewhere else, and the map
file's `$n` form — the only way anything was bound — named a numbered
table the design had already decided to delete. Every step in this
family that touches a static inherited that: 210b's static storage and
both map-file forms, 210f's conversion to and from static, 210g's
construction surface, and the dump's account of any of it.

**And it ran against 401's own first step**, which read *take the
port's own storage from 210b, which provides it*. Both could not be
second.

**The way out was to notice who has to know.** Whoever deletes the
table is the one who has to know what replaces it, so 401 built the
port-side storage it spends — a byte buffer beside the cells, allocated
at placement, plus the characters a string constant points at — rather
than waiting to be handed one. 210b's static half unblocked the moment
that existed.

So the seam this section described is gone: the ring-buffer line and
the static line are one line again, and everything left in this family
is unblocked.

## Vocabulary, since this issue is about the record itself

- **A port** is the standing interface for one input of one station:
  where its value comes from, what type it is, how many bytes one value
  occupies, and the storage it keeps. It lives on the station for the
  life of the program.
- **A slot** is one place where one value physically sits — a cell
  inside a port's ring buffer, or the bytes reserved for one argument
  inside a task. **The port decides how a value is stored; the slot is
  where it lands.**

The source calls the port a slot, which is a naming debt this family
does not pay off — renaming reaches the station header, delivery,
statics, the loader, the dump, and
[002](../docs/002-stations-and-slots.md), and is worth doing
deliberately rather than as a side effect of any of these.

## Current behavior

An input port carries a kind tag and the fields that kind needs, with
the fields for the other kind sitting unused: storage, capacity, and
two indices for a ring buffer; a table entry number for a static.

**210a is done: the gatherer is gone**, along with the pull module,
the inline execution of a box during task assembly, the cycle walk,
the runtime gather-repoint operation, the gather timing charged to
the puller, and the loader's three gather-shaped validations. What
remains is a ring buffer, a static, and — still to be built — the
state of not being configured at all.

Everything else described below is still ahead.

## The design all the children share

### Three tags, one live, and switching is a field write

The tag has three values: **ring**, **static**, and **none**. Exactly
one is in effect; the storage belonging to the other sits idle. A
port's ring cells are allocated when the station is instantiated, for
every port regardless of what that port is currently for — the element
size is known from the registry at placement, so the space is exactly
right, and a buffer standing ready is what makes changing a port's
source **a field write** rather than an allocation dance.

**None means unconfigured, and a station holding one can never be
ready.** It is a state, not a value — no null is invented and nothing
is ever handed to a box — and it is what lets a program be assembled
from nothing, a station coming into existence with every port unset and
becoming runnable as its ports are given sources one at a time.

### What the station's mutex covers

**The cell states of every port on that station, and nothing else.**
Not the value bytes, not the destinations, not the station record at
large. One lock per station rather than one per port, so anyone
claiming holds exactly one lock and there is no lock ordering anywhere
to get wrong.

It stays **inside the station record**, because shelves keep station
records still, so a mutex inside one never moves — the separate paged
array of locks that was considered would buy nothing and would pack
unrelated stations' locks into shared cache lines.

**What takes it:** the four rare structural operations — growing a ring
buffer, rewiring, writing a static, and changing a port's tag — and the
claim, which is not rare at all. The claim is on the hot path and that
is accepted, because it needs several cells at once and nothing
composes several atomic operations into one. What is *not* under it is
the expensive part: both value copies happen outside, protected by the
fact that a cell in *reserved* or *claimed* belongs to exactly one
worker. See [210d](210d-the-copies-leave-the-lock.md), which also
records why the lock-free claim that was planned here was abandoned.

**A delivering writer takes it not at all.** Writing a value touches
one cell, and one cell is already atomic with a single compare-and-swap.
So deliveries into a station never serialize; only task construction
does.

### A static lives on the port, and writing one is an event

There is no statics table. Binding a static copies the value into the
port that reads it, and from that moment the file's entry has done its
job. Claiming happens in the same window as the ring pop, so the static
half of an input set is as mutually consistent as the buffered half —
one lock instead of two.

**Writing a static runs the readiness check on its station.** That is
what replaced the pull path, and it is one addition rather than a
subsystem: a chain of stations wired through static ports becomes a
recalculation graph, and construction's own writes are what start a
program. A write cannot make something run that could not run anyway,
because the check it triggers is the ordinary one and an empty ring
port still answers no.

The statics work proper belongs to [401](completed/401-static-slots.md) and
[405](405-statics-mutation.md), which stand on 210b's record.

## Open questions

**Answered:**

- *The bookmark is touched by every reader, which is a shared write on
  the hot path — should it be per worker, or written back only
  occasionally?* Neither. It stays one shared number per port. A
  per-worker copy trades a contended hint for a cold one, and a hint
  that is always cold has stopped shortening the scan it exists to
  shorten — which is the whole of its job. The contention is real and
  is accepted with its eyes open: the per-cell states were arranged so
  that workers claiming *different* cells never write the same line,
  and that is the property worth protecting, because it scales with how
  much work is in flight. The bookmark does not — it is one line, its
  cost is fixed no matter how large the port grows, and it is only
  touched on the transitions of a claim rather than throughout one. If
  a measurement ever shows that fixed cost mattering, the cheap move is
  the second option, publishing progress every so often rather than
  every time, and nothing above has to change for it. Belongs to
  [210d](210d-the-copies-leave-the-lock.md).

- *A writer that dies mid-copy leaves a cell reserved forever — should
  that be detected, reclaimed, or reported?* None of the three, because
  nothing that can die is able to get into that window. A cell is
  reserved for the length of one copy whose size is the port's element
  size — a `sizeof` the compiler computed, carried on the box record —
  from one allocation the engine owns into another. No pointer is
  followed, nothing is allocated, and nothing is called. The claim in
  the other direction is the same shape: a reader copies the cell into
  the task's own value area and is finished with the cell there, and
  the box does not run until a worker picks that task up later, reading
  from the task and holding no cell at all. What remains is failing
  hardware, and software that stops when the memory under it stops is
  behaving correctly rather than lacking a feature.

  **This became true rather than being true, and now it is true.**
  There was one place where user code ran inside the claim walk: a
  gather slot invoked its upstream box inline, on the claiming thread,
  while cells were held, so a box that crashed there really did strand
  them. That was the pull path, and removing it is what closed the
  window. [210a](completed/210a-the-pull-path-removed.md) removed it,
  so the answer above rests on the engine as it is rather than on the
  engine as it is going to be.

  A box that crashes while it *runs* is still an event worth having an
  answer for, but it cannot strand a cell, because by then it holds
  none. It loses a task, which is a different problem.

## Related

- [202 — Ring buffer slots](completed/202-ring-buffer-slots.md), whose
  storage this keeps and whose exclusivity it drops
- [401 — Static input values](completed/401-static-slots.md), whose value moves
  onto the port here
- [405 — Changing a static while it runs](405-statics-mutation.md),
  which becomes one case of changing a port
- [403 — Gatherer slots](completed/403-gatherer-slots.md), the kind
  210a removed
- [211 — Growing the station table](211-growing-the-station-table.md),
  the same paging shape one level up
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  the surface that configures these
- [002 — Stations and slots](../docs/002-stations-and-slots.md), which
  this rewrites, including the word it uses for a port
- [056 — Why there is no pull path](../docs/implementation-notes/056-no-pull-path.md)
- [058 — Guarantees](../docs/058-guarantees.md), where the lost arrival
  order belongs
