# 209a — Results go where the caller says

The exit half of moving a program's surface onto the port, and the end
of the holding queue. [213a](213a-a-door-is-a-port.md) is the other half.

## Current behavior

**A result is a marked station, and the mark changes one thing.** When a
value comes out an output port with no wire on it, an ordinary station
discards it and a marked station *holds* it. Held values queue on the
station and come out oldest-first when somebody outside takes one.

**The queue is unbounded, and the engine knows it.** The holding array
doubles whenever it fills, and from the first growth past eight slots it
prints:

> results are piling up at *name* — *n* waiting and nobody taking them;
> this program is computing into somewhere nobody is looking

That warning was written instead of a fix. A program whose results
nobody drains grows until memory runs out, having been told so on the
way.

**A program must declare a result or it is refused at bring-up**, even
when nothing is wired into it, so that a program's interface is total —
there is otherwise no way to tell a program that deliberately works by
side effect from one whose author forgot.

**A station cannot be both doors**, and the mark rides into a composed
program the same way the entrance mark does.

## Intended behavior

**The mark lives on an output port, carries a number, and means: this
value leaves the map.** Result zero, result one. No station is special.

**Nothing is held unless somebody asked for it.** An output marked as a
result behaves exactly like any other unwired output — the value is
discarded — until an embedding caller registers somewhere to put it.
Registering *is* the wire. Nothing grows that nobody asked to grow, and
the pile-up warning stops describing a state that can happen.

**The caller owns the memory.** Registering hands the engine an address,
a count, and an element size; the engine fills that array and never
allocates. Each output gets its own array **and its own counter**,
because each fills independently.

**The bound is enforced at the reservation, not by winding down.** A
worker takes the next index atomically; a worker handed an index at or
past the count writes nothing. Winding down when an array fills is a
separate and later thing — it stops the machine wasting effort on
results nobody will keep, and it is never what keeps the array in
bounds, because workers are still inside boxes when the array fills.

**There is no slot state machine.** A ring slot needs empty, reserved,
ready and claimed because it is reused and a reader has to know what it
is looking at. A collection slot is written once and read by nobody
until the caller looks, so one atomic increment is the whole mechanism.

**Wiring happens before starting.** A caller instantiates, registers its
inputs and its collection arrays, and only then releases the workers.
This is not new discipline — it is the rule the pool already enforces,
that a standing promise must be held from before the workers are
released until the last argument is in, and the engine already has a
message for somebody who got it backwards.

**The counter tells which of two endings happened.** Filled means the
map produced at least what was asked for and was stopped. Short of full,
with the pool finished, means the map ran dry — no task queued, nobody
inside a box, nobody promising more — so that was all there was.

**Results are not synchronised with each other, and this is stated
rather than left to be found.** Two outputs are two stations on two
threads at two unrelated moments. Entry three of one array and entry
three of another did not come from the same input and are not related.
Two arrays side by side look like columns of a table and are not. The
rule for anyone building on it: make the outputs fungible; if two values
must stay together they have to be one value.

**A program need no longer declare a result.** The requirement existed
so that an interface was total, and an interface made of marked ports is
total by being read: a map with no result mark produces nothing outward,
which is a complete statement and needs no separate declaration.

## Suggested implementation steps

1. Move the mark to the output port as a number, refusing gaps and
   duplicates at bring-up alongside the argument numbering.
2. Delete the holding array, its growth, and its warning. An unwired
   marked output discards like any other until registered.
3. Add registration: an address, a count, an element size, checked
   against the port's value size, stored on the output port.
4. In delivery, when a port carries a registration, take the next index
   atomically and write only if it is below the count.
5. Add a read of the count so a caller can tell how far it got, and wind
   the program down when an array fills.
6. Remove the bring-up requirement that a program declare a result.
7. Update the outside-collection callers: the tests, and the watch-a-map
   runner.
8. Tests: two outputs into two arrays with independent counters; an
   unregistered output discards and nothing grows; values arriving after
   an array fills are dropped rather than written past the end; a short
   array plus a finished pool reads as ran-dry.

## Related

- [209 — Map output collection](completed/209-map-output-collection.md),
  which this replaces the mechanism of
- [213a — A door is a port](213a-a-door-is-a-port.md), the other half
- [601b — The dollar sign means the boundary](601b-the-dollar-sign-means-the-boundary.md),
  which spells this in the map file
- [135 — A box and a map are one thing](../docs/implementation-notes/135-a-box-and-a-map-are-one-thing.md),
  which states the non-synchronisation as a design property
- [058 — Guarantees](../docs/058-guarantees.md), which gains the bound
  and the non-synchronisation
