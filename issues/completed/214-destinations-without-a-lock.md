# 214 — Destinations without a lock

The other half of getting the station's mutex off the hot path.
[210](210-input-port-record.md) does the input side, where a claim
stops taking a lock. This does the output side, where a delivery stops
taking one to find out where it is going.

## Current behavior

**Done, and the delivery walk takes no lock at all.**

A port's destinations are one immutable array behind an atomic
pointer. Drawing or removing a wire builds a whole new array and swaps
the pointer in a single write; nothing ever edits one in place. A
walker reads the pointer once and walks something nobody will ever
modify — no lock, no copy onto its own stack, and no way to see a
half-edited set.

What it replaced: a linked list whose nodes a rewire could unlink and
free under a walker's feet, which is why the walk used to take the
station's mutex, count the destinations, copy every pair onto its own
stack, and release the lock before visiting any of them. That cost a
lock acquisition and a copy proportional to fan-out **on every value
the engine moved**, and it was the last thing holding the station's
mutex on the hot path.

**The scrapyard reclaims rather than leaks.** Each worker keeps an
epoch on its own cache line, bumped at the start and end of a whole
task, so it is odd while inside one and even while not. A replaced set
is filed with a snapshot of every epoch, and a later retire sweeps
first: a worker that is now even, or whose epoch differs from the
snapshot, cannot still be in the task it was in and therefore cannot
be holding that set. When all of them pass, it is freed.

Nothing waits and nothing spins. An idle worker is asleep and
therefore even, so it passes without ever having to move — which is
what would otherwise deadlock a sweep against a quiet pool.

**One counter for the whole task, not one per window**, exactly as
this issue asked. A worker is inside a *box* earlier in a task than it
is inside a *delivery walk*, so a counter spanning the whole thing
answers both questions, and the same mechanism now serves
[310](310-boxes-compiled-at-runtime.md)'s box unloading and
[216](216-removing-a-station.md)'s station removal. Destination sets
become freeable slightly later than they strictly must, which costs
nothing anybody measures.

**Proven**, in `tests/076-test-destinations.c`:

- a wire removed mid-run leaves the old array untouched, so a walker
  inside it sees what it always saw
- four hundred rewires against a working pool: the scrapyard never
  held one set per rewire — it drained as it went — and one sweep
  after the workers went idle freed every last one
- a teardown with sets genuinely still filed, which is the double-free
  the scrapyard's own lock exists to prevent
- a rebuild keeps wiring order, so the dump still round-trips

Under a leak checker the whole test is **1,761 allocations and 1,761
frees, no leaks and no errors**, which is what actually judges this
one — the mid-run rewiring test comes out the same way.

**One bug worth recording, because it was mine and it was instructive.**
The epoch array was first sized from the worker count the *caller*
asked for. A pool asked for zero decides its own count, so the array
was one slot while several workers wrote into it — and the symptom was
a segmentation fault inside `pthread_join`, freeing a thread control
block, hundreds of lines from the cause. Sizing it from what the pool
settled on rather than from what it was asked for is the fix.

## Intended behavior

**A port's destinations are one immutable array, and the port holds a
pointer to it.**

Rewiring never edits that array. It builds a whole new one and swaps
the pointer in a single atomic write. A walker reads the pointer once
and walks whatever it got, which nobody will ever modify — so there is
no lock, no copy, and no possibility of seeing a half-edited set.

Three things fall out. The walk gets faster by exactly the copy it no
longer makes. Old and new readers each see a coherent set rather than a
mixture. And it fits the decision that wires are attached in batches
([212](212-one-way-to-build-a-program.md)) — a batch was going to
rebuild the list anyway, so building an array instead is no extra work.

An array is also better on its own terms than a linked list here: the
destinations are visited in order, immediately, one after another, and
a contiguous run of pairs is what a processor wants for that.

### Reclaiming the old array is the real question

A walker may still be inside the old array when rewiring swaps the
pointer. Freeing it then is a use-after-free; never freeing it leaks
one array per rewire.

**Do the simple thing first: retire, don't free.** The old array goes
on a list and stays there until the program is torn down. A program
that rewires a few dozen times leaks a few kilobytes and nobody
notices. This is correct, it is three lines, and it is enough for
everything that exists today.

**Then, for a program that rewires continuously** — a control loop
turning knobs forever — the retired arrays have to come back. The
mechanism:

Each worker keeps a **private counter** of delivery walks, on its own
cache line, written by nobody else. It is incremented when a walk
begins and again when it ends, so the number is **odd while walking and
even while not**. That costs one uncontended write per delivery and no
coordination at all.

When rewiring retires an array, it files it together with a snapshot of
every worker's counter. Any later rewiring sweeps the retire list
first: for each filed array, check each worker, and if that worker's
counter is now **even** — it is not walking — **or differs from the
snapshot** — it has finished the walk it was in — that worker cannot be
holding it. When every worker passes, free it. When they do not, leave
it filed and check again next time.

Nothing waits and nothing spins. A worker with no work is asleep and
therefore even, so it passes without ever having to move, which is what
would otherwise deadlock the sweep against an idle pool. Counters are
64 bits, so a counter cannot wrap all the way back to its snapshot in a
long run and read as unchanged when it is not.

It is a scrapyard rather than a leak: everything in it is accounted
for, and anything still filed at teardown is freed then.

**This mechanism has a second customer, so build it to be shared.**
[310](310-boxes-compiled-at-runtime.md) needs to unload the compiled
code of a box while workers may be inside it, which is the same
lifetime problem in different clothes. It asks about a wider window,
though: a worker is inside a *box* earlier in a task than it is inside
a *delivery walk*. Rather than two counters, **bump one per worker at
the start and end of the whole task** — call, finish hook, free — so it
is odd across both windows and answers both questions. Destinations
then become freeable slightly later than they strictly must, which
costs nothing anybody measures, and there is one mechanism instead of
two that drift.

**The scrapyard owns a mutex.** Not against tearing — nothing reads a
filed array's contents — but against two hands freeing the same one.
There are two touchers and only one of them is obvious: rewiring
sweeps, and teardown empties. Anything touching it takes the lock,
confirms the array is still filed, unfiles it, and frees it under that
same hold, so a second arrival simply does not find it. The lock is a
leaf; nothing is acquired while it is held.

This costs nothing that this issue is trying to save. The lock being
removed is the one on the delivery walk. The scrapyard is touched when
wiring changes and when the program ends, and never in between.

## Suggested implementation steps

1. Change the destination representation from a linked list to an
   array behind a pointer, with rewiring rebuilding rather than
   editing, still under the existing rewiring lock. Delivery keeps
   taking the station's mutex for now.
2. Take the mutex out of the walk: read the pointer, walk the array.
   Measure a wide fan-out before and after — a hundred-destination
   station is where the copy cost lives.
3. Retire-don't-free, and a test that a wire removed mid-run leaves
   in-flight walkers unharmed and delivers a value down the removed
   wire exactly as it would have a moment earlier.
4. The per-worker counters, odd while walking, and the sweep. Its own
   change with its own test: many rewires against a saturated pool,
   with the retire list observed emptying. **The scrapyard's mutex
   arrives here**, with a test that a teardown racing a sweep frees
   each array exactly once — the double-free this lock exists for, and
   the one an argument from another lock would have missed.
5. Confirm the dump still round-trips after a batch rewire. Not
   because destination order must be preserved — it need not be, and is
   not — but because the rewire is the one thing that rebuilds the
   array, and a round trip taken after one is the round trip most
   likely to reveal that something else got lost in the rebuild.

## Open questions

**Answered:**

- *Must a rebuilt array preserve wiring order?* **No, and the invariant
  is deleted rather than defended.** Nothing in the running engine
  reads destination order and means anything by it: a delivery visits
  all of them, and the order values arrive at different stations was
  never promised — that is part of the bargain in
  [058](../../docs/058-guarantees.md), where ordering is one of the things
  sold to keep every core busy. An order nothing depends on is not an
  invariant, so there is nothing to demote from structure to
  discipline, and nothing for a test to stand guard over.

  **The round trip survives without anyone arranging it.** Dump, load,
  dump contains no rewire: loading appends destinations in file order,
  dumping writes them in array order, so the second text matches the
  first. [C5](../../docs/058-guarantees.md) holds by construction rather
  than by care.

  **What is knowingly passed up:** the dump is not a canonical form.
  Two programs of identical shape, wired in different orders, dump to
  different text, so two save-files cannot be compared to answer "are
  these the same program." Sorting destinations at dump time would buy
  that, and it can be added later by whoever wants it, since it
  requires nothing of the rebuild.
- *Does the retire list need its own lock?* **Yes, it gets one, and the
  reason is double-freeing rather than tearing.** The old argument —
  only rewiring touches it, and rewiring is serialized elsewhere — was
  already false when written: **teardown frees whatever is still filed,
  and teardown is not rewiring.** That is two touchers, one of which
  nobody would think to look for, and the two could free the same array
  between them.

  **The protocol is take the lock, confirm the array is still filed,
  unfile it, free it — all under one hold.** Unfiling before freeing,
  under the lock, is the whole mechanism: a second toucher arriving
  afterwards does not find it, so there is nothing for it to free
  twice. Holding the lock across the free itself is fine and simpler
  than not; this list is touched only when wiring changes or the
  program ends.

  **It costs nothing, because delivery never touches this list.** The
  mutex this issue exists to remove is the one on the *walk* — read the
  pointer, visit the array, no lock. The retire list sits entirely off
  that path, so giving it a lock takes nothing back.

  **The lock is a leaf: nothing else may be acquired while holding
  it.** Stated as a rule here rather than left to be inferred, because
  a lock-ordering cycle is precisely the kind of thing that gets built
  later by somebody who had no way to know. Rewiring may take the
  rewiring lock and then this one; nothing goes the other way.

## Related

- [210 — What an input port is](210-input-port-record.md), the input
  side of the same removal
- [704 — Rewiring while it runs](704-runtime-rewiring.md),
  which added the snapshot this replaces, and recorded it as the
  retrofit its own plan had missed
- [205 — The delivery walk](205-delivery-walk.md), the walk
  this changes
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  where wires are attached in batches, which is what makes rebuilding
  an array per change reasonable
- [703 — The map dump](703-map-dump.md), whose round trip
  depends on destination order
