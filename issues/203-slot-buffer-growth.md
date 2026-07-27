# 203 — Slot buffer growth

## Current behavior

A ring-buffer slot whose tail would collide with its head fails loudly
rather than overwriting. Correct, but it means a producer faster than
its consumer stops the program.

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

- [002 — Stations and slots](../docs/002-stations-and-slots.md)
- Issue 201 — why the station is fixed-size
- Issue 202 — the buffer being grown
- Issue 701 — reporting the growth count
