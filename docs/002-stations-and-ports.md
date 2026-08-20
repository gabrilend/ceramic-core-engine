# 002 — Stations and ports

A station is one placement of a box in a map. It is the only thing in
the engine that persists and is written to by many threads at once, so
its shape is worth knowing exactly.

## The station table

Every station in a map lives in a **table of shelves**: a short array
of pointers, each to a fixed run of station records. Station number
*n* sits at position *n* within shelf *n* divided by the shelf size —
one shift and one mask, because the shelf size is a power of two. A
station is addressed by its number forever after, and **nothing
already placed ever moves**.

That shape exists for one reason: a station holds its own mutex, and a
mutex is identified by where it lives. A flat array grows by
reallocating, which would move every record and leave any thread
parked on a lock waiting at an address nobody unlocks. Growing by
adding a shelf copies nothing but the short array of addresses.

The table grows one station at a time, and a station removed frees its
place for the next one — so a program that adds and removes forever
reaches a steady size rather than climbing. Reading a map file is that
same growth, one station per line, rather than a separate act that
counts lines and allocates once.

Indices are used rather than addresses for three reasons. They are half
the size. They survive being written to a log or dumped to the screen,
where a pointer is noise. And they leave the door open for the array to
be reallocated later, when runtime map editing becomes real, without
every wire in the program becoming a dangling reference.

The array holds fixed-size structs. Everything about a station that
varies in size — how many input ports it has, how many output ports,
how big each buffer is — hangs off a pointer, so the struct itself is
uniform and the array stays indexable.

**A station is made of:**

| Field | Type | What it is |
|---|---|---|
| mutex | `pthread_mutex_t` | Guards the input ports. Held during delivery and during the readiness check. |
| call | function pointer | The generated shim for this station's box. See [007](007-datapath-build.md). |
| kind | small integer | Plain, comparator, or iterator. Only consulted on the way out. |
| in_ports | pointer to an array | The input ports, in the order the box function's parameters appear. |
| n_in_ports | `int` | How many. Equals the box's parameter count, plus one if it is a comparator. |
| out_ports | pointer to a linked list | The output ports. One node for a plain box, three for a comparator, however many an iterator has. |
| cursor | `int` | Which output port an iterator sends to next. Unused by the other kinds. |

The station never moves. Its buffers can grow, but they grow by adding
a page of slots to the port, not by touching the station — and not by
moving any slot that already exists. This is why a wire can hold a
station index forever and never need fixing up, and why a worker
copying a value out of a slot it has claimed can do so holding no lock
at all.

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

A ring port is a **stream**; a static port is a **slot**. That is the
whole distinction, and [004](004-datapath-statics.md) is what follows
from it.

**A port is made of:**

| Field | Type | What it is |
|---|---|---|
| kind | `unsigned char` | Ring buffer, static, or no source yet. There was another, a gatherer, and [056](implementation-notes/056-no-pull-path.md) is where it went. |
| elem_size | `int` | Bytes per value. Copied from the registry at load; equals `sizeof` the box function's parameter type. |
| pages | pointer to a list | The slots, in equal-sized pages. Allocated whatever the kind and never freed until the map is, so changing what a port is costs no allocation and loses nothing that was waiting. Growth appends a page; nothing already there moves. |
| page_slots | `int` | Slots per page, the same for every page of this port. It is the starting depth, so asking for a deep buffer gives large pages rather than many small ones. |
| capacity | `int` | How many slots in total, across every page. Ten unless the port was told otherwise. |
| stride | `int` | Bytes from one slot to the next: a value, its state, and enough padding to keep the next value aligned. |
| read_hint, write_hint | `int` | Where a reader and a writer each start looking. Hints, not positions — a stale one costs a longer search and nothing else. |
| held | `int`, atomic | How many slots are ready right now. Maintained rather than counted, because readiness asks on every delivery. |
| constant | `void *` | The static's value, `elem_size` bytes, allocated at placement like the slots. Kept when the port is converted away, so a port that goes static, buffer, static reads the value it read before. |
| constant_string | `char *` | Where a string constant's characters live, since the value for such a port is a pointer and it has to point at something the port owns. |
| constant_set | `int` | Whether anybody has written one. A port can be turned back into a static, but not into one for the first time — the tag would be in effect over storage nobody wrote. |

There used to be a `source` field here, naming the station a gatherer
pulled from. It is gone from the record rather than left sitting
unused, because a field nobody writes is a question every reader has
to answer for themselves.

**Head and tail are gone, and what replaced them is the interesting
part.** They were exact positions: the head said where the oldest value
sat, and it had to be right, because it was also what said which slots
were occupied at all. A number that must be right has to be maintained
under exclusion, and that is why the station's mutex had to be held
across the copying — the indices called a slot occupied the moment it
was spoken for, which is before its bytes had landed.

Each slot now carries its own state, and a reader looks for a usable
one instead of calculating where it must be. **A position must be
exact and is therefore computed; a hint may be wrong and therefore is
not.** That one sentence is what lets the lock come off the copying,
and it is also what will let a buffer grow by adding a page instead of
copying, since nothing computes a location from the capacity any more.
Issues 210c and 210d carry it between them; 210e collects.

**A static's value lives on the port**, rather than in a numbered table
every port shared. Three things came out of the table with it. A
process may hold more than one running program, because the table was
the map-level state that forced a process-wide pointer to "the" map.
Claiming a static happens under the station's own mutex beside the ring
pop, instead of taking a second lock that could not be nested inside
the first — so an invocation's inputs are now all taken in one window.
And two ports of different types can no longer name one entry and read
the same bytes each their own way, which stops being a rule to
document and becomes a thing that cannot be said. Issue 401 did that.

**Ring buffer.** The ordinary case. Values arrive by being written into
it and wait their turn. It is a real ring: two indices, wrapping at the
end. The slots are exactly `elem_size` bytes each, allocated once when
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

A ring buffer should never be full. When a writer looks for an empty
slot and nothing answers, the buffer grows: still holding the station's
mutex, **one more page of slots is added to the end of a short list.**
Nothing is copied and no slot that already exists moves.

Every page holds the same number of slots, so turning a slot's ordinal
into a page and an offset is a divide and a remainder. Pages that each
doubled the last would have made that a walk down the list comparing
ranges, and the scan does it on every step while growth happens rarely
— so the cheap operation belongs on the side that repeats. The page
size is the port's starting depth, which means a program that knows it
needs deep buffers raises that number and gets large pages everywhere
rather than a long chain of small ones.

**It used to double and copy, and the copy could not be made safe.**
The two indices were positions taken modulo the capacity, so changing
the capacity put every existing value at a different index and they had
to be physically moved back into order. That copy had an ordering
hazard with no correct answer: publish before copying and readers see
an empty buffer while it fills; copy before publishing and a value
taken during the copy is delivered twice. What made it safe was that
the station's mutex was held for the whole of it, so nothing else could
happen at all.

That protection is exactly what the next step in this line spends.
Once a worker copies a claimed value out **without holding the lock** —
which it may, because a claimed slot belongs to it alone — "nothing
else is happening" stops being true, and moving that slot underneath
its owner is the one thing ownership does not protect. So the copy did
not need a better ordering. It needed to stop existing.

Nothing else needs care, because a page is added to the port rather
than to the station. Every wire refers to a station by index, every
value in flight is a copy inside a task struct, and a port's pages are
never reordered or freed while the map lives.

Growth costs one allocation and nothing else — there is no work
proportional to what the buffer holds any more. A buffer that keeps
growing is a signal worth logging: it means one input side
of a station is being fed faster than its sibling ports, and memory
is absorbing the imbalance while values wait for their partners.

A correction from the first build pass: a *single-input* station can
never accumulate a backlog in its port, because every write completes
its input set and is claimed immediately. A slow single-input consumer
backs up the pool's task ring instead. Port growth is specifically the
signature of a multi-input station fed unevenly; queue growth is the
signature of consumers slower than producers overall. Phase 7 reports
both, and reading them together is what locates a bottleneck.

## Output ports

A port is one exit from a station. It holds a linked list of
destinations, each a pair of 32-bit numbers: which station, and which
port on it.

Both numbers are needed. The delivery path takes the destination
station's mutex and then examines *all* of its ports to decide
readiness, so it has to be able to name the station, not merely land
somewhere inside it.

A plain box has one port, which may fan out to any number of
destinations. A comparator has exactly three. An iterator has as many
as the map gives it. What the ports mean and how one is chosen is
[005](005-routing.md).

## Related

- [003 — Delivery](003-datapath-delivery.md), the path that writes into these ports.
- [004 — Statics and recalculation](004-datapath-statics.md), the input that is not a queue.
- [007 — The build path](007-datapath-build.md), where `elem_size` and the shim pointer come from.
- [009 — Loading](009-datapath-load.md), where the table is built.
