# 210d — The copies leave the lock

Fourth child of [210](210-input-port-record.md). The station's mutex
stops guarding value bytes and guards only the bookkeeping, so the
expensive half of a claim runs with no lock held at all.

**This issue was called *The claim takes no lock* and aimed at a
lock-free claim.** It no longer does, and the reversal is recorded
below under *why the lock stayed*, because the reasoning is the design
and because a lock-free claim is a thing somebody will reach for again.

## Current behavior

**The scan is built and the positions are gone.** Head and tail have
been replaced by two hints — one for a reader, one for a writer — and
by a maintained count of ready cells. Each search starts where its hint
points, sweeps forward, wraps, and stops where it began, taking the
first cell that will make the transition it wants. The hint is read
once, which is what bounds the sweep; a hint another worker has moved
in the meantime is not wrong, only less lucky.

One function serves both directions, because a reader looking for a
ready cell and a writer looking for an empty one are the same search
with different arguments.

**A spare cell stopped being necessary and is gone.** One was always
held back so that head meeting tail could mean empty rather than full.
A buffer is now full when nothing answers to a search for an empty
cell, which is a question asked directly, so every cell is usable and
growth happens when there is genuinely nowhere to put a value rather
than one cell before.

**Readiness and claiming still happen together under the station's
mutex**, and both copies are still inside it. That is what this issue
changes.

### The measurement, and what it now means

Measured on the fan-in apparatus (`tests/063-test-fan-in-cost.c`),
small values got **meaningfully slower** — roughly half again as long
per delivery — while large values did not move outside the noise. The
search replaced index arithmetic with a compare-and-swap per candidate
cell, and the count of ready cells replaced a subtraction with an
atomic read-modify-write that the delivering threads share.

**The first suspect was wrong, and ruling it out is the useful part.**
A search over a nearly-full buffer walking past every ready cell to
find an empty one would be O(occupancy), and the apparatus pins its
ports at about ninety-five per cent full to keep growth out of the
measurement — a plausible pathology. Running it again with ports four
times roomier produced the same numbers, so the cost is the mechanism
itself rather than the buffer's fullness. The hints do their job: a
writer's next empty cell and a reader's next ready one are usually one
step away, because both hints advance in the same direction and the
region between them is exactly the occupied run.

**Under the design below, most of that cost goes away rather than being
repaid.** The per-candidate compare-and-swap existed to make a claim
safe without a lock. With the lock kept over the flips, the claim walks
under exclusion and the state transitions can be plain writes — the
atomic machinery is needed only where writers and claimers genuinely
meet, which is the single cell a delivering writer publishes. The
number to watch is still the small-value figure, and it should now come
down below the original rather than merely back to it.

## Why the lock stayed

The plan was to remove the station's mutex from the claim entirely.
Two facts, taken together, retired that plan.

**A claim needs several cells at once, and atomic operations do not
compose.** One cell flips from *ready* to *claimed* with a single
compare-and-swap, which is genuinely indivisible. A station with N
input ports needs N such cells, and there is no instruction that
compare-and-swaps N scattered addresses at once — double-width
compare-and-swap covers sixteen *contiguous* bytes, hardware
transactional memory is disabled on most parts and not broadly
shipping, and software multi-word schemes cost more atomic operations
than they save for small N. So a lock-free claim has to take its cells
one at a time and undo them when a later port comes up empty.

**That roll-back is where the difficulty lived.** Two workers at a
two-port station can each win a cell at one port, each find the other's
port empty, each release what they took, and retry into the same
interleaving — nobody blocked, nobody progressing, and a complete input
set sitting there the whole time. Claiming in ascending port order
fixes it, and the fix is provable rather than testable: a worker that
rolls back at port *k* was beaten there by a worker standing at a
strictly higher port index, the chain is bounded by the port count, and
the worker at its end must have found a genuinely empty port — so if a
complete input set exists, somebody completes.

**None of that is needed once the lock is kept.** With the whole walk
under one hold, a claimer checks every port for a ready cell *first*
and flips them only if all of them answer — because nothing can take a
ready cell away in between. Writers need not be excluded: a writer only
moves a cell empty → reserved → ready, so it can *add* availability
during the check and never remove it, and only claimers consume. So
either every port has a ready cell and the claim succeeds outright, or
one does not and nothing was ever claimed.

**The roll-back path does not exist, and livelock is not merely
prevented but unreachable.** Ascending port order survives as a habit,
because walking the ports in one direction keeps the scan's memory
access predictable, but nothing about safety rests on it any more.

**And the guarantee the lock-free direction would have weakened stays
intact.** [T3](../docs/058-guarantees.md) — *one invocation's inputs
are claimed atomically with respect to each other, all of them, under a
single hold* — is exactly what one hold across the walk means.

## Intended behavior

**The station's mutex covers the cell states of all its ports, and
nothing else.** Not the value bytes, not the destinations, not the
station record at large. One lock per station rather than one per port,
so a claimer holds exactly one lock and there is no lock ordering
anywhere to get wrong.

**A claim is check-all, then flip-all, under one hold.**

1. Take the station's claim mutex.
2. Scan each ring port for a cell in *ready*. Statics are skipped —
   they are peeked, never consumed. A port with nothing ready ends the
   attempt here, having changed nothing.
3. Every port answered, so flip each found cell from *ready* to
   *claimed*.
4. Release the mutex.

Steps 2 and 3 are the whole critical section: a scan per port and one
state write per port. No bytes move inside it.

**The copies happen outside the lock, and cell ownership is what makes
that safe.** *Reserved* and *claimed* each mean **exactly one worker
owns this cell and no other worker may touch its value** — which is
already written into the state table. A claimed cell is private
property, so copying its bytes into the task needs no exclusion from
anybody. The claimer copies its N values into the task's own storage
and then releases each cell to *empty*.

That is where the expense lives — a two-hundred-byte struct per port —
and it now runs fully parallel. Several workers copy out of one station
simultaneously while a different worker holds the lock doing its flips.

**A delivering writer takes no lock at all.** Writing a value is a
single-cell operation — reserve an empty cell, copy in, publish it
ready — and single-cell operations are already atomic with one
compare-and-swap. Only the claim needs several cells at once. So
deliveries into a station never serialize; only task construction does.

**Writing a static runs the readiness check on its station, and both
want this mutex.** That has to be one hold doing both rather than two
acquisitions, or a station recursively locks itself.

**Finding a ready cell is a scan, with a bookmark.** There are no
positions any more, so a reader starts at a hint and looks forward.
The hint advances as cells are used and is allowed to be wrong — a
stale one costs a slightly longer scan and nothing else. **A position
must be exact and is therefore computed from the capacity; a hint may
be wrong and therefore is not.**

**The bookmark is one shared number per port, and it is an index.**
Not a pointer, and not a copy per worker. An index because the port's
storage is the one thing in this design that gets reallocated: growth
adds a page, and a pointer saved into a page is a pointer that means
something else afterwards, while an ordinal position into the port's
cells keeps meaning what it meant. This is the same reason a wire is a
pair of integers rather than an address.

**The scan reads the bookmark once, sweeps forward, wraps, and stops
where it started.** Reading it once is what bounds the work: the sweep
visits at most every cell the port has, exactly one time each, and
then gives up. Re-reading a bookmark that other workers keep pushing
forward would let a reader chase it, and a scan that can be outrun is
a scan with no bound.

**The scan is the one unbounded thing inside the critical section**,
and that should be said plainly rather than discovered. The flips are N
writes; the scan is a walk whose length is bounded only by the port's
depth. A bookmark that is usually right keeps it to a step or two, and
a port deep enough for it to matter is one phase 7 already complains
about. But it is the term to watch if this ever measures badly.

**Values may leave a port in a different order than they arrived.**
This is a real loss and it is chosen deliberately. Values reaching one
port from two upstream stations were already in whatever order the
threads produced them, so arrival order was arbitrary to begin with;
and the scan takes the first ready cell from a hint rather than the
oldest, which nothing cheap could find once cells are released
individually.

**That non-guarantee is written down before the test that contradicts
it is retired.** There is a passing test asserting several hundred
in-order deliveries. It has to go, and the order matters: record the
non-guarantee in [058](../docs/058-guarantees.md) first, then retire
the test citing it. Retiring it first would leave a window in which
the project has silently stopped promising something it still appears
to promise.

## Suggested implementation steps

1. **Done.** The lost ordering recorded in
   [058](../docs/058-guarantees.md) as a stated non-guarantee, with its
   reason.
2. **Done.** The in-order delivery test narrowed rather than deleted.
   It held three things at once: that values arrive untorn, that none
   is lost or doubled, and that the fifth value on one side meets the
   fifth on the other. Only the third was the retired promise.
3. **Done.** The scan: a bookmark per port, read once, sweep forward,
   wrap, stop where it started.
4. The claim becomes check-all-then-flip-all under one hold, with no
   roll-back path built. The claim already reports whether it found
   anything, which is the answer the check needs.
5. The mutex's scope narrowed to the cell states, and both copies moved
   outside it — the write copy first, then the claim copy — with cell
   ownership as the only thing protecting the bytes.
6. The per-candidate compare-and-swap reduced to a plain write wherever
   the lock already excludes, keeping the atomic transition only where
   a writer and a claimer genuinely meet.
7. The static-write path and the readiness check made one hold rather
   than two acquisitions.
8. Re-measure the fan-in apparatus against the figure recorded above.

## Open questions

None outstanding. The one this issue carried — how to prove a livelock
test can catch anything, given that a passing concurrency test may
simply have missed the interleaving — dissolved with the roll-back
path. There is no livelock to test for, because no worker can hold a
partial claim while another interleaves.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210c — A state on every cell](210c-a-state-on-every-cell.md), which
  must land first — the ownership meaning of *reserved* and *claimed*
  is the whole reason the copies can leave the lock
- [210e — Growth adds a page](210e-growth-adds-a-page.md), which is
  only safe because nothing here computes a location from the capacity
- [204 — The readiness check](completed/204-readiness-check.md), the
  walk this changes
- [214 — Destinations without a lock](214-destinations-without-a-lock.md),
  the output side, which does remove its lock — the asymmetry is that a
  delivery walk reads one pointer and a claim needs N cells at once
- [058 — Guarantees](../docs/058-guarantees.md), where the lost arrival
  order is recorded and where T3 survives this unchanged
