# 002 — Stations and slots

A station is one placement of a box in a map. It is the only thing in
the engine that persists and is written to by many threads at once, so
its shape is worth knowing exactly.

## The station table

Every station in a loaded map lives in one flat array, allocated once
when the map loads and never resized while the program runs. A station
is addressed by its position in that array — a 32-bit index, not a
pointer.

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

## The three kinds of slot

An input slot is a place a value arrives from. There are three ways
that can happen, and the slot carries a one-byte tag saying which. The
tag is stored, never inferred — asking "is the box upstream of me an
input-less box?" on every readiness check would mean chasing an index
into another station to answer a question that cannot change.

**A slot is made of:**

| Field | Type | What it is |
|---|---|---|
| kind | `unsigned char` | Ring buffer, gatherer, or static. |
| elem_size | `int` | Bytes per value. Copied from the registry at load; equals `sizeof` the box function's parameter type. |
| storage | `void *` | For a ring buffer, the cells. For a static, unused. |
| capacity | `int` | Ring buffer only — how many cells. |
| head, tail | `int` | Ring buffer only — where the oldest value sits and where the next one goes. |
| source | `int` | Gatherer only — which station supplies this slot on demand. |
| static_id | `int` | Static only — which entry in the statics table. |

**Ring buffer.** The ordinary case. Values arrive by being written into
it and wait their turn. It is a real ring: two indices, wrapping at the
end. The cells are exactly `elem_size` bytes each, allocated once when
the map loads, so a write is a `memcpy` into a fixed offset with no
allocation anywhere on the path.

**Gatherer.** The slot holds no buffer at all. It holds the index of an
upstream station, and the value is produced on demand by running that
station's box inline at the moment a task is being assembled. See
[004](004-datapath-gather.md).

**Static.** The slot holds an index into the statics table. Its value
never arrives — it is simply always there, which means a static slot is
always full and never affects whether a station is ready. Thresholds,
file paths, and configuration live here.

## Ring buffer growth

A ring buffer should never be full. If the writing index would land on
the reading index, the buffer grows: still holding the station's mutex,
the storage is reallocated to twice the size, the wrapped-around
portion is copied up so the contents read contiguously again, and the
two indices are corrected.

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
- [004 — Gathering](004-datapath-gather.md), the pull path.
- [007 — The build path](007-datapath-build.md), where `elem_size` and the shim pointer come from.
- [009 — Loading](009-datapath-load.md), where the table is built.
