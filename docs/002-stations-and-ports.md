# 002 — Stations and ports

A station is one placement of a box in a map. It is the only thing in the
engine that persists and is written to by many threads at once.

## The station table

Every station lives in a **table of shelves**: a short array of pointers,
each to a fixed run of station records. Station *n* sits at position *n*
within shelf *n* divided by the shelf size — one shift and one mask,
because the shelf size is a power of two. **Nothing already placed ever
moves.**

That shape exists for one reason: a station holds its own mutex, and a
mutex is identified by where it lives. A flat array grows by
reallocating, which would move every record and leave any thread parked
on a lock waiting at an address nobody unlocks. Growing by adding a shelf
copies nothing but the short array of addresses.

The table grows one station at a time, and a station removed frees its
place for the next one, so a program that adds and removes forever
reaches a steady size. Wires hold indices rather than addresses: half the
size, and readable in a log where a pointer is noise.

The records are fixed-size structs. Everything that varies in size — how
many ports, how big each buffer — hangs off a pointer.

| Field | Type | What it is |
|---|---|---|
| mutex | `pthread_mutex_t` | Guards the input ports. Held during delivery and the readiness check. |
| call | function pointer | The generated shim for this station's box. See [007](007-datapath-build.md). |
| kind | small integer | Plain, comparator, or iterator. Only consulted on the way out. |
| in_ports | pointer to an array | The input ports, in the order the box function's parameters appear. |
| n_in_ports | `int` | The box's parameter count, plus one if it is a comparator. |
| out_ports | pointer to a linked list | One node for a plain box, three for a comparator, however many an iterator has. |
| cursor | `int` | Which output port an iterator sends to next. |

Buffers grow by adding a page of slots to the port, not by touching the
station and not by moving any slot that already exists. So a wire can
hold a station index forever, and a worker copying a value out of a slot
it has claimed can do so holding no lock at all.

## The kinds of input port

An input port is where one of a box's arguments comes from. The port
carries a one-byte tag. It is stored, never inferred — deciding it by
walking upstream on every readiness check would mean chasing indices into
other stations to answer a question that cannot change.

| tag | how it is read | how it is written | gates readiness |
|---|---|---|---|
| **ring** | **consumed** — one value taken per invocation | queued behind whatever is waiting | **yes** |
| **static** | **peeked** — every invocation reads the same value | replaces what was there | no, always full |
| **none** | never — a station holding one cannot run | by being given a source | **yes**, permanently negative |

A ring port is a **stream**; a static port is a **slot**. That is the
whole distinction, and [004](004-datapath-statics.md) is what follows
from it.

**Ring.** Values arrive by being written in and wait their turn; each
invocation consumes one.

**Static.** The port holds the value itself — always there, so it never
affects whether a station is ready. Thresholds, file paths, and
configuration live here. Reading one is a **peek**, which is what makes a
threshold a threshold, and **writing one is an event**: it runs the
ordinary readiness check on the station holding it, which is how a chain
wired through statics recalculates and how a program starts at all.

**None.** No source yet. The station can never become ready, which is
what lets a program be assembled a piece at a time. It is a state, not a
value: nothing is ever handed to a box.

| Field | Type | What it is |
|---|---|---|
| kind | `unsigned char` | Ring buffer, static, or no source yet. |
| elem_size | `int` | Bytes per value. `sizeof` the box function's parameter type. |
| pages | pointer to a list | The slots, in equal-sized pages. Allocated whatever the kind and never freed until the map is, so changing what a port is costs no allocation and loses nothing waiting. Growth appends; nothing already there moves. |
| page_slots | `int` | Slots per page, the same for every page. It is the starting depth, so a deep buffer means large pages rather than many small ones. |
| capacity | `int` | Slots in total. Ten unless the port was told otherwise. |
| stride | `int` | Bytes from one slot to the next: a value, its state, and padding to keep the next value aligned. |
| read_hint, write_hint | `int` | Where a reader and a writer each start looking. Hints, not positions — a stale one costs a longer search and nothing else. |
| held | `int`, atomic | How many slots are ready. Maintained rather than counted, because readiness asks on every delivery. |
| constant | `void *` | The static's value, allocated at placement like the slots. Kept when the port is converted away, so a port that goes static, buffer, static reads the value it read before. |
| constant_string | `char *` | Where a string constant's characters live, since the value for such a port is a pointer. |
| constant_set | `int` | Whether anybody has written one. A port can be turned back into a static, but not into one for the first time — the tag would be in effect over storage nobody wrote. |

**A slot carries its own state, and a reader looks for a usable one
rather than calculating where it must be.** Everything else about the
port follows: **a position must be exact and is therefore computed; a
hint may be wrong and therefore is not.** That is what lets the lock come
off the copying, and what lets a buffer grow by adding a page, since
nothing computes a location from the capacity.

**A static's value lives on the port** rather than in a shared table. So
a process may hold more than one running program; a static is claimed
under the station's own mutex beside the ring pop, which is why an
invocation's inputs are all taken in one window; and two ports of
different types cannot name one entry and read the same bytes each their
own way.

## Ring buffer growth

When a writer looks for an empty slot and nothing answers, the buffer
grows: still holding the station's mutex, **one more page of slots is
added to the end of a short list.** Nothing is copied and no slot that
already exists moves.

Every page holds the same number of slots, so turning a slot's ordinal
into a page and an offset is a divide and a remainder. Pages that each
doubled the last would make that a walk down the list comparing ranges,
and the scan does it on every step while growth happens rarely. The page
size is the port's starting depth, so a program that needs deep buffers
gets large pages rather than a long chain of small ones.

Nothing else needs care: every wire refers to a station by index, every
value in flight is a copy inside a task struct, and a port's pages are
never reordered or freed while the map lives.

**A buffer that keeps growing is a signal worth logging**, and it says
something specific: one input side of a station is being fed faster than
its siblings, and memory is absorbing the imbalance while values wait for
their partners.

A *single-input* station can never accumulate a backlog, because every
write completes its input set and is claimed immediately; a slow
single-input consumer backs up the pool's task ring instead. So port
growth is the signature of a multi-input station fed unevenly, and queue
growth is the signature of consumers slower than producers overall.
Reading the two together locates a bottleneck.

## A depth travels downstream

Growth is cheap, but it happens **on the delivery path, holding the
station's own mutex**, which is the lock the readiness check wants. A
burst of a hundred values into a ten-slot buffer takes that lock about
nine extra times. So when the engine already knows a backlog is coming,
it makes the buffers below deep enough up front.

**Two moments know it**, and they are the only two: a depth declared on a
port (`in 0 x64`), and a wire being drawn, where the new destination
inherits whatever backlog the source can hold. Both are construction-time
operations under the rewiring lock; none of this runs while values are
being delivered.

### How far a station can fall behind

The **minimum** over the ports that gate it, because a station runs when
*every* input port holds a value. Fed a hundred from one side and ten
from the other, it runs ten times. Taking the maximum would size
everything below an uneven join for a backlog that cannot arrive.

Static ports are skipped, because a static is always full and gates
nothing. A port with **no source at all** gates everything, so the answer
is zero and the walk stops. A station with no gating ports is a station
of constants, seeded once, and answers one.

### What each kind passes on

The three kinds differ in exactly one way — where a returned value goes —
so this is the one place that has to know about them:

| kind | what it passes down |
|---|---|
| **plain** | the whole backlog to every wire on port zero, because fan-out duplicates a value rather than dividing it |
| **iterator** | each exit gets its share, since the exits are taken in turn; the remainder rides on the ones the cursor reaches first |
| **comparator** | **nothing. The walk stops here.** |
| **void** | nothing to send |

**The comparator is exempt on purpose.** One of three exits fires per run
and the data chooses which, so any one could take everything. Sizing all
three costs three times the memory for a guess; sizing none costs a
page-at-a-time growth on whichever exit turns out to be busy. **The
cheaper mistake is the one that only costs time.**

### Why a cycle is safe without a visited set

The stopping condition is **"already deep enough"**, checked at each port
before touching it. An accumulator wires its own output back into its own
input, so the walk arrives back where it started and the second visit
finds the port at the size the first gave it. That is a fixed point: no
visited set to allocate, no depth limit to pick, and nothing to keep in
step as the graph changes while the program runs.

### Reading a port's depth

Two different questions. **How many values are waiting** is what the
depth call reports. **How many slots the port has room for** is its
capacity, which is the number this walk raises. A buffer sized sixty-four
deep and holding three answers sixty-four for one and three for the
other.

## Output ports

A port is one exit from a station. The ports of a station are a linked
list; each holds a pointer to its **destinations**, an array of pairs of
32-bit numbers: which station, and which port on it. Both are needed,
because the delivery path takes the destination station's mutex and then
examines *all* of its ports to decide readiness.

**The destination array is immutable and is swapped whole.** Drawing or
cutting a wire builds a new array and swaps the port's pointer in a
single atomic write, so a walker reads the pointer once and then walks
something nobody will ever modify. That is what takes the station's mutex
off the delivery walk: no lock, no copy onto the walker's stack, and no
way to see a half-edited set. The replaced array is filed in the
scrapyard and freed when nothing can still be walking it.

A null pointer means a port wired nowhere, and delivering to it
**discards** — which is what an unwired comparator branch should do.

A plain box has one port, which may fan out to any number of
destinations. A comparator has exactly three. An iterator has as many as
the map gives it. [005](005-routing.md) is how one is chosen.

### An output port may be one of the map's results

A port carries the number of the result it is, or a sentinel saying it is
neither.

**A mark is not a bucket.** A marked port with nowhere to put its values
discards them like any other unwired output, so a program nobody is
collecting from grows nothing. What makes values arrive somewhere is a
caller **registering an array**: a pointer, how many fit, and how wide
one is. The memory belongs to the caller.

Slots are claimed with one atomic add. **The bound is the reservation,
never the winding down** — a worker handed a slot at or past the end
writes nothing, because workers are still inside boxes at the moment the
array fills.

There is no per-slot state machine here, which is the difference from a
ring buffer's slots. A ring slot needs one because it is reused; one of
these is written once and read by nobody until the caller comes to look.

## Related

- [003 — Delivery](003-datapath-delivery.md), the path that writes into these ports
- [004 — Statics and recalculation](004-datapath-statics.md), the input that is not a queue
- [007 — The build path](007-datapath-build.md), where `elem_size` and the shim pointer come from
- [009 — Loading](009-datapath-load.md), where the table is built
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md), where a third kind of input port went
