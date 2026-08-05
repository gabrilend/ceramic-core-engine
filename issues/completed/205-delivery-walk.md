# 205 — The delivery walk

## Current behavior

**Built, and losing the two things it does that are not delivering.**

**The gather step goes.** Task construction used to resolve pulled
values by running upstream boxes inline on the delivering thread; there
is no pull path
([056](../../docs/implementation-notes/056-no-pull-path.md)), so a task
is complete the moment its values are copied in. With it goes the one
exception in the engine to *a box only runs when a worker picks it up*.

**The destination snapshot goes.** The walk takes the station's mutex,
copies the destination list into a stack array, and releases — a lock
and a copy proportional to fan-out on every value the engine moves,
there because a rewire can free a list node under a walker.
[214](../214-destinations-without-a-lock.md) makes the list an
immutable array published by a single atomic write, so the walk reads
one pointer and takes no lock at all.

**And it gains one case**: a destination that is a *boundary* rather
than a slot. An output station runs no box
([209](../209-map-output-collection.md)), so arriving at one means the
value has left the program — or, if nothing is wired beyond it, is held
where it landed.

Everything else here is untouched, including the decision that reads
best in hindsight: **a port wired to nothing discards.** That is right
for an unwired comparator branch, which is the normal case, and it
stays right — the output station's held values are the one exception,
and they are an exception because discarding a program's results means
the program did nothing.

The remainder describes it as built.

Built, riding the pool's finish hook: after a worker runs a task, the
walk chooses a port through the routing dispatch (plain filled,
comparator and iterator rows failing loudly until phase 5), then
delivers the output value to each destination on it — lock, write,
readiness, claim, unlock, build, push, next. A void box's task skips
the walk entirely; both dedicated read and write box types from the
original vision stayed dissolved, with the write box falling out as a
plain sink. A port wired to nothing discards, which a comparator
outcome will later want. Proven by a three-station chain, a
twenty-way fan-out, and structs crossing two hops byte-identical
into a void sink.

## Intended behavior

The path from "a box function just returned" to "some other box is
queued to run because of it." This is the central path of the engine;
everything else exists to support it or set it up.

A worker has just executed a task. It holds the return value in the
task struct's output field, and the task records which station produced
it.

1. **Choose a port.** In this phase every station is plain and has
   exactly one. The choice becomes a three-entry dispatch in phase 5,
   so it should be a function call from the start rather than an
   assumption inlined here.
2. **Walk that port's destination list.** For each `{station, slot}`
   pair, deliver independently.
3. **Take the destination station's mutex.**
4. **Write the value into the slot** — `memcpy` of `elem_size` bytes at
   the tail, advance, grow if needed.
5. **Run the readiness check** (issue 204), still holding the mutex.
6. **Release**, and if the station was ready, build a task (issue 206)
   and push it.
7. Continue to the next destination.

When every destination is handled, the worker frees its own task struct
and returns to the pool.

**Fan-out is not a kind of box.** It is what one port with several
destinations already does. A port wired to a hundred places means a
hundred deliveries by this one worker before it goes back for more
work — which is acceptable, because each delivery may unblock a
station, so the worker is spending its time manufacturing parallelism
for everyone else.

**A box that returns nothing is a sink.** Delivery is skipped entirely
and the worker goes straight to freeing its task. File writes, prints,
and effects need no special support; they fall out of a function
declared `void`. The original design called for dedicated read and
write box types, and both dissolved — a write box is a sink, and a read
box is a gatherer in phase 4. Neither needs a line of engine code.

## Suggested implementation steps

1. The delivery function, taking a finished task and walking its
   station's port.
2. Port selection as a separate call with only the plain case
   implemented, so phase 5 adds cases rather than restructuring.
3. Wire delivery into the worker run loop between running a task and
   freeing it.
4. A test of a three-station chain, asserting the value arrives at the
   end and the middle station ran once.
5. A test of one station fanning out to many, asserting every
   destination received a copy and each downstream station ran.
6. A test that a `void` box's task is freed and nothing is delivered.

## Related

- [003 — Delivery](../docs/003-datapath-delivery.md)
- Issue 204 — the check this runs
- Issue 206 — the task this builds
- Issue 501 — where port selection grows two more cases
