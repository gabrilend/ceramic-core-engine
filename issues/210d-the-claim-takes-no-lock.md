# 210d — The claim takes no lock

Fourth child of [210](210-input-port-record.md). The station's mutex
leaves the hot path entirely, and a real guarantee is given up on
purpose to let it.

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
mutex**, and both copies are still inside it. Nothing about the locking
has changed yet — deliberately, so that the search could be proven
while the old lock still covered for it.

### The scan is not free, and the payoff has not landed

Measured on the fan-in apparatus (`tests/063-test-fan-in-cost.c`),
small values got **meaningfully slower** — roughly half again as long
per delivery — while large values did not move outside the noise.

That is the expected shape of this moment and is worth writing down
rather than explaining away. The search replaced index arithmetic with
a compare-and-swap per candidate cell, and the count of ready cells
replaced a subtraction with an atomic read-modify-write that the
delivering threads share. Both costs are paid now; the thing they were
paid for — taking the mutex off the copying, and then off the walk — is
still ahead. A delivery currently does the new work *and* holds the old
lock.

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

This number is the one to beat when the copies move out. If the small
figure does not come back down, something other than the lock is
holding it up.

**The scan below is needed earlier than this issue's place in the
order suggests.** [210c](210c-a-state-on-every-cell.md) built the
per-cell states and then found it could not move either copy out of
the mutex without it: a cell that a writer has reserved and not yet
published is, to anything that computes a position, indistinguishable
from a cell holding a value. A claimer that arrives there is refused
and has nowhere else to look, because a computed position is the only
one it has. The distinction this issue draws — a position must be
exact, a hint may be wrong — is what makes that window survivable, and
nothing smaller does.

So the scan is the next thing built, and 210c's two copy moves follow
it rather than precede it. Nothing about the design below changes;
only when it happens does.

**A value's position is computed.** A reader takes the cell at the
head index, which is exact and must be, because head and tail are what
say which cells are occupied.

That is the property standing in the way. A computed position has to
be right, which means it has to be maintained under exclusion, which
means the walk needs the lock.

## Intended behavior

**Walk the ports in ascending index order.** At each ring port, find a
ready cell and flip it to claimed. Statics are skipped — they are
peeked, never consumed, so there is nothing to take. Reach the end
having claimed one from every ring port and the invocation is real.
Meet a ring port with nothing ready and walk back, flipping what you
claimed to ready again, and give up.

**The fixed order is what prevents livelock**, and it is the only
subtle part. Without it, two threads at a two-input station can claim
one port each, each fail on the other's port, each roll back, and
retry into the same interleaving forever — nobody blocked, nobody
progressing, and a complete input set sitting there the whole time.
Lowest index first means both reach for the same port, one wins
outright, and the loser fails at the first step having claimed
nothing.

**Finding a ready cell is a scan, with a bookmark.** There are no
positions any more, so a reader starts at a hint and looks forward.
The hint advances as cells are used and is allowed to be wrong — a
stale one costs a slightly longer scan and nothing else. That is the
distinction that makes everything here work: **a position must be
exact and is therefore computed from the capacity; a hint may be wrong
and therefore is not.**

**The bookmark is one shared number per port, and it is an index.**
Not a pointer, and not a copy per worker. An index because the port's
storage is the one thing in this design that gets reallocated: growth
adds a page, and a pointer saved into a page is a pointer that means
something else afterwards, while an ordinal position into the port's
cells keeps meaning what it meant. This is the same reason a wire is a
pair of integers rather than an address, and the reason has now been
paid for twice.

**The scan reads the bookmark once, sweeps forward, wraps, and stops
where it started.** Reading it once is what bounds the work: the sweep
visits at most every cell the port has, exactly one time each, and
then gives up. Re-reading a bookmark that other workers keep pushing
forward would let a reader chase it, and a scan that can be outrun is
a scan with no bound.

**Other workers move the bookmark while a sweep is in progress, and
that is fine.** It is a hint, so a reader that started from a value
now stale is not wrong, only slightly less lucky — it pays a few extra
cells of walking. Nothing about correctness rests on the number being
current; what rests on it is only how quickly a reader finds work.

**Values may leave a port in a different order than they arrived.**
This is a real loss and it is chosen deliberately. Values reaching one
port from two upstream stations were already in whatever order the
threads produced them, so arrival order was arbitrary to begin with;
and rollback releases cells wherever they sit, so gaps open and the
oldest occupied cell stops being findable without a search nobody
wants to pay for.

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
   reason. It was already written there — ahead of the code, and
   claiming the test below had already gone.
2. **Done.** The in-order delivery test narrowed rather than deleted.
   It held three things at once: that values arrive untorn, that none
   is lost or doubled, and that the fifth value on one side meets the
   fifth on the other. Only the third was the retired promise. Tearing
   and loss are the failures worth catching and neither of them was
   ever the ordering, so what is left is the part that still means
   something.
3. **Done.** The scan: a bookmark per port, read once, sweep forward,
   wrap, stop where it started. Built ahead of its place in the order,
   for the reason under *Current behavior* — 210c's copy moves wait on
   it rather than the other way round — and landed while the walk is
   still under the mutex, which keeps 210c's staging principle intact:
   build the mechanism while the old lock still covers for it, prove
   it, then remove the cover.
4. The claim walk in ascending port order, with roll-back on the first
   port that has nothing ready. The claim already reports whether it
   found anything, which is the answer the roll-back needs; today a no
   is treated as an engine bug, because under the mutex it is one.
5. A livelock test that would fail without the ordering rule: many
   threads, a multi-input station, values arriving on all sides, run
   long enough that a repeating interleaving would show as a stall
   rather than as a slowdown.
6. Remove the station's mutex from the readiness-and-claim path
   entirely, leaving it to the four rare structural operations the
   parent names.

## Open questions

- A livelock test that passes proves nothing on its own — the
  interleaving it is meant to catch may simply not have occurred. It
  needs to be run once with the ordering rule deliberately broken, and
  seen to fail, or it is decoration. Whether that inverted run stays
  in the tree as a disabled test or exists only as a note recording
  that it was done is undecided.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210c — A state on every cell](210c-a-state-on-every-cell.md), which
  must land first — a lockless claim is not expressible while
  occupancy is implied by indices
- [210e — Growth adds a page](210e-growth-adds-a-page.md), which is
  only safe because nothing here computes a location from the capacity
- [204 — The readiness check](completed/204-readiness-check.md), the
  walk this makes lockless
- [214 — Destinations without a lock](214-destinations-without-a-lock.md),
  the same removal on the output side
- [058 — Guarantees](../docs/058-guarantees.md), where the lost arrival
  order is recorded before the test goes
