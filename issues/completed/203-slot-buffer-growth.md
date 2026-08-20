# 203 — Slot buffer growth

## Current behavior

**Built, and the copy at the centre of it is going.**

Doubling exists as it does because head and tail are *positions taken
modulo the capacity* — change the capacity and every value is suddenly
at a different index, so the wrapped portion has to be physically moved
back into order. That copy is the reason growth needs the mutex, the
reason it has an ordering hazard, and the reason a ring buffer was the
one thing in the engine that could not grow by simply adding room.

[210](../210-input-port-record.md) removes the premise rather than the
symptom: cells carry their own state, a reader scans from a bookmark
that is allowed to be wrong, and nothing computes a location from the
capacity. Then a buffer grows by **adding a page**, nothing is copied,
no cell moves, and the hazard stops existing — publish before copying
and readers see an empty buffer, copy before publishing and a value
taken during the copy is delivered twice, and neither can happen when
there is no copy.

**The invariant this issue proved is what makes all of it safe and it
is unchanged**: only the storage the port points at is ever
reallocated, never the port and never the station. Seven doublings with
every station address unchanged and every neighbour untouched.

Two things also survive: the **growth count and high-water mark** put
here a phase early so the phase 7 report would be a read rather than a
retrofit — still the right instinct, and now feeding a third kind of
warning, since an output buffer growing means nobody is collecting a
program's results. And the rule that **an input buffer is never full
when a value arrives**, which is what lets memory absorb any imbalance
between input sides instead of the engine having to decide what to drop.

The remainder describes it as built.

Built. When the tail would land on the head the storage doubles under
the station's mutex, the wrapped portion is copied so the contents
read contiguously from cell zero, and the indices are corrected. Only
the storage the slot points at is reallocated — never the slot, never
the station — which the station-table test proves by flooding one
slot through seven doublings and finding every address unchanged and
every neighbour untouched. The growth count and a high-water
occupancy mark (added here, a small step past the plan, so phase 7's
report is a read rather than a retrofit) live on the slot. The
wrapped-before-growth case is exactly what the slots test seeds.

## Intended behavior

**An input ring buffer should never be full.** When the tail would land
on the head, the buffer grows: still holding the station's mutex, the
storage is reallocated to twice its size, the wrapped-around portion is
copied up so the contents read contiguously again, and the two indices
are corrected.

**The safety argument matters more than the mechanism.** Growth
reallocates the *storage the slot points at*, not the station. Every
wire in the program refers to its destination by station index, and
every value in flight is already a copy inside a task struct. Nothing
anywhere holds a pointer into the buffer, so moving it cannot dangle
anything.

This is also the reason the station struct is fixed-size and its
variable parts hang off pointers — see issue 201. Had the cells been
inline, growing a buffer would grow the struct, and growing the struct
would move the station, and moving a station would invalidate every
wire pointing at it.

**Growth happens under the mutex that already guards the slot.** Every
write and every pop takes it, so a growing buffer has no other thread
inside it. No new lock, no new ordering to reason about.

**A growing buffer is a signal, not an event.** It means a consumer is
slower than its producer and memory is quietly absorbing the
difference. Phase 7 reports it. This issue should leave the hook in
place — a count of how many times each slot has grown — so that
reporting it later is a read rather than a retrofit.

## Suggested implementation steps

1. Add a growth count to the slot struct.
2. In write, replace the loud failure with: reallocate to twice
   capacity, copy the wrapped portion so the queue reads contiguously,
   set head to zero and tail to the number of values held, increment
   the growth count.
3. A test that fills a slot of small capacity, grows it several times,
   and asserts every value comes back in order. Seed it with a buffer
   whose contents have already wrapped before the growth — the unwrap
   step is the part that will be wrong first, and a buffer that has not
   wrapped will not catch it.
4. A test that grows a slot while other threads are writing to
   neighbouring slots on the same station, confirming the station's
   address and every other slot's contents are untouched.

## Related

- [002 — Stations and ports](../docs/002-stations-and-ports.md)
- Issue 201 — why the station is fixed-size
- Issue 202 — the buffer being grown
- Issue 701 — reporting the growth count
