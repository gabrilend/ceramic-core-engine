# 214 — Destinations without a lock

The other half of getting the station's mutex off the hot path.
[210](210-input-port-record.md) does the input side, where a claim
stops taking a lock. This does the output side, where a delivery stops
taking one to find out where it is going.

## Current behavior

An output port holds its destinations as a **linked list**, and every
delivery reads that list under the owning station's mutex.

It does not merely read it — it **copies it out**. The walk takes the
lock, counts the destinations, copies each `{station, port}` pair into
an array on its own stack, releases the lock, and then visits them. The
copy exists because runtime rewiring can unlink and free a node, and a
walker holding a pointer into the list would follow a freed one. Taking
the values out means that after the unlock, nothing points at anything
that can be freed.

That is correct, and it was the one retrofit the rewiring work
discovered late rather than planning for. It costs a lock acquisition
and a copy proportional to fan-out **on every value the engine moves**,
and it is now the last thing holding the station's mutex on the hot
path.

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
   with the retire list observed emptying.
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
  [058](../docs/058-guarantees.md), where ordering is one of the things
  sold to keep every core busy. An order nothing depends on is not an
  invariant, so there is nothing to demote from structure to
  discipline, and nothing for a test to stand guard over.

  **The round trip survives without anyone arranging it.** Dump, load,
  dump contains no rewire: loading appends destinations in file order,
  dumping writes them in array order, so the second text matches the
  first. [C5](../docs/058-guarantees.md) holds by construction rather
  than by care.

  **What is knowingly passed up:** the dump is not a canonical form.
  Two programs of identical shape, wired in different orders, dump to
  different text, so two save-files cannot be compared to answer "are
  these the same program." Sorting destinations at dump time would buy
  that, and it can be added later by whoever wants it, since it
  requires nothing of the rebuild.
- Does the retire list need its own lock? Only rewiring touches it, and
  rewiring is already serialized by the rewiring lock, so probably not
  — but that is an argument from a lock existing elsewhere, which is
  the kind of reasoning that stops being true quietly.

## Related

- [210 — What an input port is](210-input-port-record.md), the input
  side of the same removal
- [704 — Rewiring while it runs](completed/704-runtime-rewiring.md),
  which added the snapshot this replaces, and recorded it as the
  retrofit its own plan had missed
- [205 — The delivery walk](completed/205-delivery-walk.md), the walk
  this changes
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  where wires are attached in batches, which is what makes rebuilding
  an array per change reasonable
- [703 — The map dump](completed/703-map-dump.md), whose round trip
  depends on destination order
