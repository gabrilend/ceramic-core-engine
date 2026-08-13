# 002 — Stations and slots

A station is one placement of a box in a map. It is the only thing in
the engine that persists and is written to by many threads at once, so
its shape is worth knowing exactly.

## The station table

Every station in a loaded map lives in one flat array, allocated once
when the map loads and never resized while the program runs. A station
is addressed by its position in that array — a 32-bit index, not a
pointer.

**The "never resized" half is going.** Under one construction surface,
adding a station is the only way one ever comes into existence, so the
table starts empty and grows as a program is read. It grows by adding a
**shelf** — another allocation holding a fixed number of stations, with
a short list of where the shelves are — rather than by reallocating,
because a station holds its own mutex and a mutex is identified by
where it lives. Move one and every thread parked on it waits forever at
an address nobody will unlock. Issue 211.

Indices are used rather than addresses for three reasons. They are half
the size. They survive being written to a log or dumped to the screen,
where a pointer is noise. And they leave the door open for the array to
be reallocated later, when runtime map editing becomes real, without
every wire in the program becoming a dangling reference.

The array holds fixed-size structs. Everything about a station that
varies in size — how many input slots it has, how many output ports,
how big each buffer is — hangs off a pointer, so the struct itself is
uniform and the array stays indexable.

**A station is made of:**

| Field | Type | What it is |
|---|---|---|
| mutex | `pthread_mutex_t` | Guards the input slots. Held during delivery and during the readiness check. |
| call | function pointer | The generated shim for this station's box. See [007](007-datapath-build.md). |
| kind | small integer | Plain, comparator, or iterator. Only consulted on the way out. |
| slots | pointer to an array | The input slots, in the order the box function's parameters appear. |
| n_slots | `int` | How many. Equals the box's parameter count, plus one if it is a comparator. |
| ports | pointer to a linked list | The output ports. One node for a plain box, three for a comparator, however many an iterator has. |
| cursor | `int` | Which output port an iterator sends to next. Unused by the other kinds. |

The station never moves. Its buffers can grow, but they grow by
reallocating the buffer storage the slot points at, not by reallocating
the station. This is why a wire can hold a station index forever and
never need fixing up.

## The kinds of input port

An input port is where one of a box's arguments comes from. There are
two ways a value can be there, plus the state of not having been
configured at all, and the port carries a one-byte tag saying which.
The tag is stored, never inferred — deciding it by walking upstream on
every readiness check would mean chasing indices into other stations to
answer a question that cannot change.

| tag | how it is read | how it is written | gates readiness |
|---|---|---|---|
| **ring** | **consumed** — one value taken per invocation | queued behind whatever is waiting | **yes** |
| **static** | **peeked** — every invocation reads the same value | replaces what was there | no, always full |
| **none** | never — a station holding one cannot run | by being given a source | **yes**, permanently negative |

A ring port is a **stream**; a static port is a **cell**. That is the
whole distinction, and [004](004-datapath-statics.md) is what follows
from it.

**A slot is made of:**

| Field | Type | What it is |
|---|---|---|
| kind | `unsigned char` | Ring buffer or static. There was a third, a gatherer, and [056](implementation-notes/056-no-pull-path.md) is where it went. |
| elem_size | `int` | Bytes per value. Copied from the registry at load; equals `sizeof` the box function's parameter type. |
| storage | `void *` | For a ring buffer, the cells. For a static, unused. |
| capacity | `int` | Ring buffer only — how many cells. |
| head, tail | `int` | Ring buffer only — where the oldest value sits and where the next one goes. |
| static_id | `int` | Static only — which entry in the statics table. |

There used to be a `source` field here, naming the station a gatherer
pulled from. It is gone from the record rather than left sitting
unused, because a field nobody writes is a question every reader has
to answer for themselves.

Two things about that table are changing and are worth reading beside
it. **A static's value is moving onto the port itself**, so `static_id`
becomes the bytes rather than an index into a shared table — which is
what lets a process hold more than one program at a time, and what
makes claiming a static happen under the same lock as the ring pop
instead of a second one. And **head and tail are going**, because each
cell will carry its own state and a reader will scan for a usable one
rather than compute where it must be; that is what lets a buffer grow
by adding a page instead of copying. Issue 210b carries the first;
210c and 210e carry the second between them.

**Ring buffer.** The ordinary case. Values arrive by being written into
it and wait their turn. It is a real ring: two indices, wrapping at the
end. The cells are exactly `elem_size` bytes each, allocated once when
the map loads, so a write is a `memcpy` into a fixed offset with no
allocation anywhere on the path.

**Gatherer — removed.** A third kind used to hold the index of an
upstream station and produce its value on demand, by running that
station's box inline while a task was being assembled. Nothing is
pulled any more; a value that used to be gathered is written into a
static port by an ordinary push, and writing a static runs the
readiness check on the station holding it. See
[056](implementation-notes/056-no-pull-path.md) for what that was for
and what ending it cost.

**Static.** The port holds an index into the statics table. Its value
never arrives — it is simply always there, which means a static port is
always full and never affects whether a station is ready. Thresholds,
file paths, and configuration live here.

Reading one is a **peek**: the value is not consumed, so a station
driven by its ring side reads the same static on every one of its runs.
That is what makes a threshold a threshold. And **writing one is an
event** — it runs the ordinary readiness check on the station holding
it, which is how a chain of stations wired through statics recalculates
and how a program starts at all. [004](004-datapath-statics.md).

**None.** The port has been given no source yet. It can never hold a
value, so a station with one can never become ready — which is what
lets a program be assembled a piece at a time, with stations existing
before they are wired. It is a state, not a value: nothing is ever
handed to a box.

## Ring buffer growth

A ring buffer should never be full. If the writing index would land on
the reading index, the buffer grows: still holding the station's mutex,
the storage is reallocated to twice the size, the wrapped-around
portion is copied up so the contents read contiguously again, and the
two indices are corrected.

**The copy is the part that is going.** It exists because the two
indices are positions taken modulo the capacity — change the capacity
and every existing value is suddenly at a different index, so they have
to be physically moved back into order. Once cells carry their own
state and a reader scans instead of computing, nothing derives a
location from the capacity, and a buffer can grow by **adding a page**
of cells to a short list. No copy, no existing cell moves, and the
ordering hazard the copy has to be careful about — publish before
copying and readers see an empty buffer; copy before publishing and a
value taken during the copy is delivered twice — stops existing rather
than being handled.

This is safe without any further care because the growth reallocates
the *storage* the slot points at, not the station. Every wire in the
program refers to the station by index, and every value in flight is a
copy inside a task struct. Nothing holds a pointer into the buffer.

Growth is O(number of values held) but amortized to nothing, and it
happens at most a couple dozen times in a process lifetime. A buffer
that keeps growing is a signal worth logging: it means one input side
of a station is being fed faster than its sibling slots, and memory
is absorbing the imbalance while values wait for their partners.

A correction from the first build pass: a *single-input* station can
never accumulate a backlog in its slot, because every write completes
its input set and is claimed immediately. A slow single-input consumer
backs up the pool's task ring instead. Slot growth is specifically the
signature of a multi-input station fed unevenly; queue growth is the
signature of consumers slower than producers overall. Phase 7 reports
both, and reading them together is what locates a bottleneck.

## Output ports

A port is one exit from a station. It holds a linked list of
destinations, each a pair of 32-bit numbers: which station, and which
slot on it.

Both numbers are needed. The delivery path takes the destination
station's mutex and then examines *all* of its slots to decide
readiness, so it has to be able to name the station, not merely land
somewhere inside it.

A plain box has one port, which may fan out to any number of
destinations. A comparator has exactly three. An iterator has as many
as the map gives it. What the ports mean and how one is chosen is
[005](005-routing.md).

## Related

- [003 — Delivery](003-datapath-delivery.md), the path that writes into these slots.
- [004 — Statics and recalculation](004-datapath-statics.md), the input that is not a queue.
- [007 — The build path](007-datapath-build.md), where `elem_size` and the shim pointer come from.
- [009 — Loading](009-datapath-load.md), where the table is built.
