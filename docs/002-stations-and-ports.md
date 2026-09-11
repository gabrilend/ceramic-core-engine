# 002 — Stations and ports

A station is one placement of a box in a map. It is the only thing in
the engine that persists and is written to by many threads at once, so
its shape is worth knowing exactly.

## The station table

Every station in a map lives in a **table of shelves**: a short array of
pointers, each to a fixed run of station records. Station number *n* sits
at position *n* within shelf *n* divided by the shelf size — one shift
and one mask, because the shelf size is a power of two. A station is
addressed by its number forever after, and **nothing already placed ever
moves**.

That shape exists for one reason: a station holds its own mutex, and a
mutex is identified by where it lives. A flat array grows by
reallocating, which would move every record and leave any thread parked
on a lock waiting at an address nobody unlocks. Growing by adding a shelf
copies nothing but the short array of addresses.

The table grows one station at a time, and a station removed frees its
place for the next one — so a program that adds and removes forever
reaches a steady size rather than climbing. Reading a map file is that
same growth, one station per line.

Wires hold indices rather than addresses: half the size, and readable in
a log or a dump where a pointer is noise.

The array holds fixed-size structs. Everything about a station that
varies in size — how many input ports it has, how many output ports, how
big each buffer is — hangs off a pointer, so the struct itself is uniform
and the array stays indexable.

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

The station never moves. Its buffers grow by adding a page of slots to
the port, not by touching the station and not by moving any slot that
already exists. This is why a wire can hold a station index forever and
never need fixing up, and why a worker copying a value out of a slot it
has claimed can do so holding no lock at all.

## The kinds of input port

An input port is where one of a box's arguments comes from. There are two
ways a value can be there, plus the state of not having been configured
at all, and the port carries a one-byte tag saying which. The tag is
stored, never inferred — deciding it by walking upstream on every
readiness check would mean chasing indices into other stations to answer
a question that cannot change.

| tag | how it is read | how it is written | gates readiness |
|---|---|---|---|
| **ring** | **consumed** — one value taken per invocation | queued behind whatever is waiting | **yes** |
| **static** | **peeked** — every invocation reads the same value | replaces what was there | no, always full |
| **none** | never — a station holding one cannot run | by being given a source | **yes**, permanently negative |

A ring port is a **stream**; a static port is a **slot**. That is the
whole distinction, and [004](004-datapath-statics.md) is what follows
from it.

**Ring.** The ordinary case. Values arrive by being written in and wait
their turn; each invocation consumes one. The slots live in pages, each
slot holding a value, its state, and enough padding to keep the next
value aligned.

**Static.** The port holds the value itself. It never arrives — it is
simply always there, which means a static port is always full and never
affects whether a station is ready. Thresholds, file paths, and
configuration live here. Reading one is a **peek**: the value is not
consumed, so a station driven by its ring side reads the same static on
every one of its runs. That is what makes a threshold a threshold. And
**writing one is an event** — it runs the ordinary readiness check on the
station holding it, which is how a chain of stations wired through
statics recalculates and how a program starts at all.
[004](004-datapath-statics.md).

**None.** The port has been given no source yet. It can never hold a
value, so a station with one can never become ready — which is what lets
a program be assembled a piece at a time, with stations existing before
they are wired. It is a state, not a value: nothing is ever handed to a
box.

**A port is made of:**

| Field | Type | What it is |
|---|---|---|
| kind | `unsigned char` | Ring buffer, static, or no source yet. |
| elem_size | `int` | Bytes per value. Written by the placement function; equals `sizeof` the box function's parameter type. |
| pages | pointer to a list | The slots, in equal-sized pages. Allocated whatever the kind and never freed until the map is, so changing what a port is costs no allocation and loses nothing that was waiting. Growth appends a page; nothing already there moves. |
| page_slots | `int` | Slots per page, the same for every page of this port. It is the starting depth, so asking for a deep buffer gives large pages rather than many small ones. |
| capacity | `int` | How many slots in total, across every page. Ten unless the port was told otherwise. |
| stride | `int` | Bytes from one slot to the next: a value, its state, and enough padding to keep the next value aligned. |
| read_hint, write_hint | `int` | Where a reader and a writer each start looking. Hints, not positions — a stale one costs a longer search and nothing else. |
| held | `int`, atomic | How many slots are ready right now. Maintained rather than counted, because readiness asks on every delivery. |
| constant | `void *` | The static's value, `elem_size` bytes, allocated at placement like the slots. Kept when the port is converted away, so a port that goes static, buffer, static reads the value it read before. |
| constant_string | `char *` | Where a string constant's characters live, since the value for such a port is a pointer and it has to point at something the port owns. |
| constant_set | `int` | Whether anybody has written one. A port can be turned back into a static, but not into one for the first time — the tag would be in effect over storage nobody wrote. |

**A slot carries its own state, and a reader looks for a usable one
rather than calculating where it must be.** That is the sentence the rest
of the port's behaviour falls out of: **a position must be exact and is
therefore computed; a hint may be wrong and therefore is not.** It is
what lets the lock come off the copying, and what lets a buffer grow by
adding a page, since nothing computes a location from the capacity.

**A static's value lives on the port** rather than in a table every port
shares. So a process may hold more than one running program; claiming a
static happens under the station's own mutex beside the ring pop, which
is why an invocation's inputs are all taken in one window; and two ports
of different types cannot name one entry and read the same bytes each
their own way.

## Ring buffer growth

A ring buffer should never be full. When a writer looks for an empty slot
and nothing answers, the buffer grows: still holding the station's mutex,
**one more page of slots is added to the end of a short list.** Nothing
is copied and no slot that already exists moves.

Every page holds the same number of slots, so turning a slot's ordinal
into a page and an offset is a divide and a remainder. Pages that each
doubled the last would have made that a walk down the list comparing
ranges, and the scan does it on every step while growth happens rarely —
so the cheap operation belongs on the side that repeats. The page size is
the port's starting depth, which means a program that knows it needs deep
buffers raises that number and gets large pages everywhere rather than a
long chain of small ones.

Nothing else needs care, because a page is added to the port rather than
to the station. Every wire refers to a station by index, every value in
flight is a copy inside a task struct, and a port's pages are never
reordered or freed while the map lives.

Growth costs one allocation and nothing else. **A buffer that keeps
growing is a signal worth logging**, and it says something specific: one
input side of a station is being fed faster than its sibling ports, and
memory is absorbing the imbalance while values wait for their partners.

A *single-input* station can never accumulate a backlog in its port,
because every write completes its input set and is claimed immediately. A
slow single-input consumer backs up the pool's task ring instead. So port
growth is the signature of a multi-input station fed unevenly, and queue
growth is the signature of consumers slower than producers overall.
Reading the two together is what locates a bottleneck.

## A depth travels downstream

Growing a buffer is cheap, but it happens **on the delivery path, holding
the station's own mutex**, which is the same lock the readiness check
wants. A burst of a hundred values into a ten-slot buffer takes that lock
about nine extra times for no reason a reader of the map could have
predicted. So when the engine already knows a backlog is coming, it makes
the buffers below deep enough up front instead.

**Two moments know it**, and they are the only two:

- **A depth is declared on a port.** Writing `in 0 x64` says this buffer
  starts sixty-four deep, which says something about every station
  downstream of it too.
- **A wire is drawn.** The new destination inherits whatever backlog the
  source can already hold.

Both are construction-time operations under the rewiring lock. Nothing
about this ever runs while values are being delivered.

### How far a station can fall behind

The **minimum** over the ports that gate it, because a station runs when
*every* input port holds a value. Fed a hundred from one side and ten
from the other, it runs ten times and produces ten. Taking the maximum
would size everything below an uneven join for a backlog that cannot
arrive.

Static ports are skipped rather than counted, because a static is always
full and gates nothing. A port with **no source at all** gates everything
— the station never runs — so the answer is zero and the walk stops. A
station with no gating ports at all is a station of constants, seeded
once, and answers one.

### What each kind passes on

The three station kinds differ in exactly one way — where a returned
value goes — so this is the one place that has to know about them:

| kind | what it passes down |
|---|---|
| **plain** | the whole backlog to every wire on port zero, because fan-out duplicates a value rather than dividing it |
| **iterator** | each exit gets its share, since the exits are taken in turn; the remainder rides on the ones the cursor reaches first |
| **comparator** | **nothing. The walk stops here.** |
| **void** | nothing to send |

**The comparator is exempt on purpose.** One of three exits fires per run
and the data chooses which, so any one of them could take everything.
Sizing all three for the whole backlog costs three times the memory for a
guess; sizing none of them costs a page-at-a-time growth on whichever
exit turns out to be busy. **The cheaper mistake is the one that only
costs time.**

### Why a cycle is safe without a visited set

The walk's stopping condition is **"already deep enough"**, checked at
each port before touching it.

An accumulator wires its own output back into its own input, so this walk
arrives back where it started. The second visit finds the port at the
size the first visit gave it and stops. That is a fixed point, and it
means there is no visited set to allocate, no depth limit to pick, and
nothing that has to be kept in step as the graph changes — which matters,
because the graph changes while the program runs.

### Reading a port's depth

Two different questions, and the calls answer different ones. **How many
values are waiting in a port** is what the depth call reports. **How many
slots the port has room for** is its capacity, which is the number this
walk raises. A buffer sized sixty-four deep and holding three answers
sixty-four for one question and three for the other.

## Output ports

A port is one exit from a station. The ports of a station are a linked
list; each port holds a pointer to its **destinations**, which are an
array of pairs of 32-bit numbers: which station, and which port on it.

Both numbers are needed. The delivery path takes the destination
station's mutex and then examines *all* of its ports to decide readiness,
so it has to be able to name the station, not merely land somewhere
inside it.

**The destination array is immutable and is swapped whole.** Nothing ever
edits one. Drawing or cutting a wire builds a whole new array and swaps
the port's pointer in a single atomic write, so a walker reads the
pointer once and then walks something nobody will ever modify. That is
what takes the station's mutex off the delivery walk altogether: no lock,
no copy onto the walker's stack, and no way to see a half-edited set. The
array it replaced is filed in the scrapyard and freed when nothing can
still be walking it.

A null pointer means a port wired nowhere, and delivering to it
**discards** — which is exactly what an unwired comparator branch should
do.

A plain box has one port, which may fan out to any number of
destinations. A comparator has exactly three. An iterator has as many as
the map gives it. What the ports mean and how one is chosen is
[005](005-routing.md).

### An output port may be one of the map's results

A port carries the number of the result it is, or a sentinel saying it is
neither — which is the case for most ports.

**A mark is not a bucket.** A marked port with nowhere to put its values
discards them like any other unwired output, so a program nobody is
collecting from grows nothing. What makes the values arrive somewhere is
a caller **registering an array to put them in**: a pointer, how many
fit, and how wide one is. The memory belongs to the caller.

Slots are claimed with one atomic add on a counter. **The bound is the
reservation, never the winding down** — a worker handed a slot at or past
the end writes nothing, because workers are still inside boxes at the
moment the array fills and there is no way to ask them to stop having
started.

There is no per-slot state machine here, and that is the difference from
a ring buffer's slots. A ring slot needs one because it is reused and a
reader must know what it is looking at; one of these is written once and
read by nobody until the caller comes to look.

## Related

- [003 — Delivery](003-datapath-delivery.md), the path that writes into these ports.
- [004 — Statics and recalculation](004-datapath-statics.md), the input that is not a queue.
- [007 — The build path](007-datapath-build.md), where `elem_size` and the shim pointer come from.
- [009 — Loading](009-datapath-load.md), where the table is built.
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md), where a third kind of input port went.
