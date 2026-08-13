# 210c — A state on every cell

Third child of [210](210-input-port-record.md). The first of three
that must land in order, and the one that buys the other two: once a
cell can say what is happening to it, the lock stops being the thing
that says so.

## Current behavior

**The station's mutex is held for the whole of an arrival.** A
delivery takes it, copies the value into a cell, walks every port
asking whether it holds a value, claims one from each, and only then
lets go. A station that three arrows fan into, carrying two-hundred-byte
structs, holds its lock for six hundred bytes of copying while every
other deliverer waits.

The copying is the part that does not belong there. Deciding whether
an input set is complete is a handful of index comparisons; moving the
bytes is unbounded in the size of the type and has nothing to do with
any other port.

A cell's occupancy is implied by two indices — head and tail — which
is why the lock must cover the copy: the indices say a cell is
occupied before its bytes have finished landing, so nothing but
exclusion can stop a reader from arriving early.

## Intended behavior

**Every cell carries its own state, and that state is the lock.**

| state | meaning | who may touch it |
|---|---|---|
| **empty** | nothing here | a writer, by taking it |
| **reserved** | a writer owns it and is copying in | that writer only |
| **ready** | the bytes have landed | a reader, by taking it |
| **claimed** | a reader owns it and is copying out | that reader only |

Every transition is a single atomic compare-and-swap, so two threads
can never own one cell. That is the whole of the mutual exclusion: a
writer must not write while anyone reads or writes, a reader must not
read while anyone writes, and the state machine says so **per cell**
rather than per port. No lock is involved.

**The state lives on the cell, not in a parallel array.** A parallel
array would put every cell's state in one cache line, so a writer at
one end and a reader at the other would invalidate each other's copy
on every flip — a hardware cost no lock can remove, because it is not
a race. Carried on the cell, a writer working at cell three and a
reader working at cell zero touch different lines entirely whenever
the value is large, which is exactly when the copying being moved out
of the lock was worth moving. Small values share a line and do not
care, because their copies were never the problem.

**Cells are not cleared when released.** Every write is a memory copy
of the port's full element size, so a stale value is always completely
covered and there is no such thing as a partial write into a cell. The
guarantee is not that a cell was cleaned but that its bytes are never
read unless its state says ready, which is the state machine's entire
job. Zeroing on release would cost a full erase per claim and buy
nothing.

**The copies move out of the lock in two separate steps, and the state
machine is proven correct before either.** This is the whole shape of
the work: build the states while the old locking is still in force, so
that a bug in the state machine cannot yet corrupt anything, and only
then start removing the thing that was covering for it.

## Suggested implementation steps

1. The per-cell state, carried on the cell, every transition an atomic
   compare-and-swap — **but with the copies still inside the station's
   mutex**. The state machine is exercised for real and the old
   locking still guarantees it cannot matter. A bug here shows up as a
   failed transition rather than as a torn value.
2. A test that hammers the transitions directly, without delivery
   around them: many threads competing for the same cell, exactly one
   winning each transition, no state ever reached from a state that
   cannot reach it.
3. Move the **write** copy out of the lock. A deliverer takes a cell
   to reserved, copies outside, then publishes as ready.
4. Move the **read** copy out of the lock. A claimer takes a cell to
   claimed, copies outside, then releases as empty.
5. Measure the delivery path at each of those two steps against a wide
   fan-in with large values — which is where the win is supposed to be
   and the only place it will show. A measurement on small values will
   show nothing and would be evidence of nothing.

## What this issue does not do

The claim still walks under the station's mutex here; only the copying
leaves. Making the claim itself lockless is
[210d](210d-the-claim-takes-no-lock.md), and it is separate because
the rollback and the ordering rule that make it safe are their own
argument.

## Open questions

- Step 5 needs a number to compare against, and there is no recorded
  baseline for the delivery path under wide fan-in with large values.
  Taking that measurement belongs *before* step 1 rather than at step
  5, or the improvement is a claim rather than a finding — and phase
  4 already learned once that a cost measured in the wrong place
  measures nothing.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210b — The port record](210b-the-port-record.md), which this needs
  for the cells to live on
- [210d — The claim takes no lock](210d-the-claim-takes-no-lock.md),
  which is only expressible once cells carry their own states
- [210e — Growth adds a page](210e-growth-adds-a-page.md), which needs
  occupancy to stop being implied by indices
- [202 — Ring buffer slots](completed/202-ring-buffer-slots.md), whose
  head-and-tail exclusivity this replaces
- [205 — The delivery walk](completed/205-delivery-walk.md), the path
  being measured
