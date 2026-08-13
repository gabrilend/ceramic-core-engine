# 210c — A state on every cell

Third child of [210](210-input-port-record.md). The first of three
that must land in order, and the one that buys the other two: once a
cell can say what is happening to it, the lock stops being the thing
that says so.

## Current behavior

**Every cell carries its own state, and the machine is proven.** A
cell is a value followed by the state of that value, and the four
transitions are compare-and-swaps that name the state they start
from — so a move out of a state a cell is not in simply fails, and an
illegal transition is impossible rather than discouraged. Delivery
runs through it: a write reserves an empty cell, copies, and publishes
it ready; a claim takes a ready cell, copies out, and releases it
empty.

**The copies are still inside the station's mutex, and that is the
plan.** The machine is exercised for real while the old locking still
guarantees a bug in it cannot matter — a wrong transition shows up as
a refused move that stops the program, rather than as a torn value
that does not. Every refusal in the delivery path is therefore fatal
and says so: under the mutex nobody else is touching the port, so a
refusal is not a lost race but a disagreement between the indices and
the states.

Proven three ways with no delivery around it: sixteen threads over
four thousand cells win every cell exactly once; every move from a
state a cell is not in is refused, walked over the whole table rather
than spot-checked; and a hundred thousand values pass through eight
cells between four writers and four readers with no lock anywhere,
each coming out exactly once.

**The cost of the state is in the stride.** A cell is now its value,
its state, and enough padding to keep the next value aligned. The
engine does not know any type's alignment — the registry carries sizes
and not alignments — so the stride rounds up to the largest power of
two dividing the element size, which is guaranteed to be at least the
alignment because a type's alignment always divides its size. A
four-byte integer port goes from four bytes per cell to eight; a
two-hundred-byte struct port goes to two hundred and eight. The
overhead is proportionally large exactly where it is absolutely small.

**What is left.** Both copies are still under the mutex.

## What steps 3 and 4 turned out to need

**The write copy cannot leave the lock without the reader learning to
search**, and the plan above did not see it. It is worth writing down
because it is the same fact this issue is built on, arriving one step
earlier than expected.

Occupancy is still implied by the head and tail indices. Move the
write copy outside the lock and the sequence becomes: take a cell to
reserved, advance the tail, release the lock, copy, publish. Between
the tail advancing and the publish landing, **the indices say a value
is waiting and the cell says it is still being written.** A claimer
arriving in that window computes the position of the value it wants,
finds a reserved cell there, and is refused — correctly, because the
bytes have not landed, but it had nowhere else to look, because a
computed position is the only one it has.

So the reader has to stop computing a position and start looking for a
ready cell. That is exactly
[210d](210d-the-claim-takes-no-lock.md)'s scan, and it is needed
*before* the lock comes off the copy rather than after. The
distinction that issue draws — a position must be exact and is
therefore computed, a hint may be wrong and therefore is not — is what
makes the reserved-but-unpublished window survivable, and there is no
smaller thing that does.

**The two issues are therefore one piece of work in the middle, and
they should be resequenced rather than forced.** The scan belongs at
the head of the remaining work, with both copy moves and the claim
walk's rollback following it. What does not change is the staging
principle that made this issue safe: build the mechanism while the old
lock still covers for it, prove it, and only then remove the cover.

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

1. **Done.** The per-cell state, carried on the cell, every transition
   an atomic compare-and-swap — **but with the copies still inside the
   station's mutex**. The state machine is exercised for real and the
   old locking still guarantees it cannot matter. A bug here shows up
   as a failed transition rather than as a torn value.
2. **Done.** A test that hammers the transitions directly, without
   delivery around them: many threads competing for the same cells,
   exactly one winning each, no state ever reached from a state that
   cannot reach it.

   Contention there comes from breadth rather than from a barrier —
   every thread walks the same long run of cells at once — because the
   one-cell-per-round shape needs a barrier between rounds, and
   building a correct barrier to test a mechanism whose purpose is to
   avoid needing one is the wrong way round. The scaffolding's own
   race was the first thing that shape produced.
3. **The scan comes first**, from
   [210d](210d-the-claim-takes-no-lock.md), for the reason set out
   above: a reserved-but-unpublished cell is indistinguishable from a
   full one to anything that computes a position, so the reader has to
   look rather than calculate before either copy can leave the lock.
4. Move the **write** copy out of the lock. A deliverer takes a cell
   to reserved, copies outside, then publishes as ready.
5. Move the **read** copy out of the lock. A claimer takes a cell to
   claimed, copies outside, then releases as empty.
6. Measure the delivery path at each of those two steps against a wide
   fan-in with large values — which is where the win is supposed to be
   and the only place it will show. A measurement on small values will
   show nothing and would be evidence of nothing. The apparatus and
   the baseline both exist; see above.

## What this issue does not do

The claim still walks under the station's mutex here; only the copying
leaves. Making the claim itself lockless is
[210d](210d-the-claim-takes-no-lock.md), and it is separate because
the rollback and the ordering rule that make it safe are their own
argument.

## The baseline, taken

Step 5 needed a number to compare against and there was none. There is
now: `tests/063-test-fan-in-cost.c` runs four ports fed by four
contending threads and reports nanoseconds per delivery, once with
two-hundred-byte values and once with four-byte ones. It runs in the
ordinary test suite, so the number is re-taken on every build and
nobody has to remember to look.

Read the current figures by running it rather than from here; what is
worth writing down is the **shape** of the result, because that is
what the comparison rests on. A large value costs roughly three to
four times what a small one does. That gap is the copy's share of a
delivery, and it is the entire thing this issue is trying to move —
so a successful 210c shrinks the large number toward the small one and
leaves the small one alone. If a later run shows both falling
together, whatever improved was not this.

**The measurement had to be corrected before it was worth keeping, and
the correction is the interesting part.** Taken first with the default
buffer depth, the ports grew thirteen times each during the run. Every
growth allocates a new array and copies every value across *under the
same mutex being measured*, and that copy is proportional to the
element size — so the large-value figure was substantially a
measurement of growth. Growth is [210e](210e-growth-adds-a-page.md)'s
problem. A baseline containing it would have made this issue look like
it achieved less than it did, because moving the delivery copy out of
the lock leaves the growth copy inside it. The ports are now sized
past what the run can fill, and what remains is the cost this issue
is aiming at.

The stray reading is worth keeping in mind for step 5: the same
apparatus, pointed at a port that *is* allowed to grow, is most of the
measurement 210e will want.

## Open questions

None outstanding. The baseline question above is answered.

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
