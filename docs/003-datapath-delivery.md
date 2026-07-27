# 003 — Datapath: delivery

This is the central path of the engine. Everything else exists to
support it or to set it up.

It answers one question: a box function has just returned a value —
what happens between that moment and the moment some other box runs
because of it?

## The path

A worker thread has just executed a task. It holds the return value in
the task struct's output field, and the task struct records which
station produced it.

**1. Choose a port.** The station's kind decides which of its output
ports this value goes down. A plain box has one. A comparator compares
the value against its threshold slot and picks one of three. An
iterator uses the port its task was assigned when it was enqueued. This
is the whole of [005](005-routing.md).

**2. Walk that port's destination list.** For each `{station, slot}`
pair on it, the value is delivered independently. A port wired to a
hundred destinations means a hundred deliveries, done by this one
worker, before it goes back for more work. That is acceptable: each
delivery may unblock a station, so the worker is spending its time
manufacturing parallelism for everyone else.

**3. For each destination, take that station's mutex.**

**4. Write the value into the slot.** `memcpy` of `elem_size` bytes
into the cell at the tail index, then advance the tail. If advancing
the tail would land on the head, the buffer grows first — see
[002](002-stations-and-slots.md).

**5. Run the readiness check, still holding the mutex.** Walk every
slot on this station and ask whether it holds a value. A ring buffer
holds one if head and tail differ. A gatherer slot always does, because
its value is produced on demand. A static slot always does, because its
value is simply there.

If any slot is empty, release the mutex and move to the next
destination. Nothing more happens; the value sits in the buffer waiting
for its siblings.

**6. If every slot is occupied, claim one value from each.** Ring
buffer slots are popped — the value is copied out and the head advances,
so the value is now spoken for and no other thread can claim it. If the
station is an iterator, its cursor advances now and the port it landed
on is recorded, so that two tasks assembled moments apart go to
different ports.

**7. Release the mutex.** This is the end of the contended section, and
it is deliberately short: a few `memcpy`s and some index arithmetic. As
soon as it is released, other threads can deliver into this station
again.

**8. Build the task struct.** Allocated fresh, sized exactly for this
box — the generator knows it needs, say, two ints in and one int out.
It receives the values claimed in step 6, the station's shim pointer,
the station's index, and for an iterator the port chosen in step 6.

Any gatherer slots are resolved here, outside the mutex, by running
their upstream boxes inline. See [004](004-datapath-gather.md).

**9. Push the task onto the pool** and continue to the next
destination.

When every destination has been handled, the worker frees its own task
struct and goes back to the pool for another.

## Why the values are claimed before the mutex is released

Steps 6 and 7 are the reason two invocations of the same station can
run at once without interfering. By the time the mutex is released, the
values belonging to this invocation have been copied out of the station
entirely and into a task struct that nothing else can see. A second
thread arriving immediately afterward finds different values, builds a
different task, and the two never meet.

This is also why a box may not remember anything. The station is
guarded, but the box function runs long after the mutex was dropped,
on whichever worker eventually picks the task up. Two of them can be
inside the same box function at the same instant. The only state a
station keeps across invocations is the iterator's cursor, and that is
touched only in step 6, under the mutex, by the enqueuing thread — the
box function never sees it.

## Why nothing polls

Notice that no part of this path searches for work. The readiness check
runs on exactly one station: the one just written to. A station whose
inputs have not changed cannot have become ready, so there is nothing
to look at.

The consequence is that a station with no ring-buffer inputs can never
be discovered this way, because nothing is ever written into it. Those
stations are either seeded once at startup ([009](009-datapath-load.md))
or pulled on demand ([004](004-datapath-gather.md)).

## Boxes that return nothing

A box whose function has no return value is a sink — a file write, a
print, an effect. Delivery is simply skipped and the worker goes
straight to freeing its task. Sinks need no special support; they fall
out of a function declared `void`.

## Related

- [002 — Stations and slots](002-stations-and-slots.md)
- [005 — Routing](005-routing.md), which port step 1 chooses
- [006 — Scheduling](006-datapath-scheduling.md), what step 9 pushes into
