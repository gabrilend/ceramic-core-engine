# 702 — Per-station statistics

## Current behavior

**Built, losing one column and gaining one warning.**

The gather column goes with the pull path
([056](../../docs/implementation-notes/056-no-pull-path.md)) — there is
nothing to charge and nobody to charge it to. What replaces it is
smaller and louder: an **output buffer growing** is reported from the
first doubling, because unlike the two piles this phase already
distinguishes, it does not mean a rate mismatch. It means nobody is
collecting the program's results at all
([209](../209-map-output-collection.md)).

The reasoning that produced the gather column outlives it, and it is
the best sentence in this issue: **charge a cost where it is actually
paid, not where it is nominally incurred.** That is why gather time
went to the pulling station rather than the gatherer, and it is what
made the phase 4 demo's first measurement visibly wrong. It applies
unchanged to whatever gets measured next.

Two decisions here are untouched and worth restating because
everything since has leaned on them: **counts are atomics updated where
the work already is, always on, costing a fetch-add**, and **timing
compiles out entirely** when it is not asked for, so the apparatus can
be removed rather than merely disabled. And the report offers its
orderings as a dispatch table — by time, by contention, by count —
because the interesting station is a different one under each.

The remainder describes it as built.

Built. Run counts and produced-for-others ride the delivery path as
atomics where the work already is, always on, costing a fetch-add.
Timing — box time from inside the generated shims, mutex wait around
the delivery lock, gather time charged to the pulling station at the
exact call — exists only when SORA_STATS is compiled in; without the
define every clock read compiles out of the shims and the delivery
path both, so the apparatus can be removed entirely, and the demo
measures its cost with it on and off rather than assuming. The
report offers the three orderings as a dispatch table of
comparators: by time, by contention, by count, because the
interesting station is a different one under each. Counts proven
exact on a loaded chain; the timing columns and the on/off overhead
are the phase demo's scenes.

## Intended behavior

Per station: how many times it ran, total and mean time inside its box
function, time spent waiting on its mutex, and how many tasks it
produced for others.

**Mutex wait time is the interesting one.** It is the direct measure of
contention on a station, and contention on a station is the shape of
the bottleneck. A station whose box is fast but whose mutex is
contended is a station too many things point at.

**Time inside the box separates the engine's cost from the program's.**
If the boxes account for nearly all the wall-clock time, the engine is
out of the way, which is the goal. If they do not, the difference is
delivery, gathering, and lock contention, and knowing the split is what
makes optimising anything possible.

**Gather time is charged to the station that pulled**, not to the
gatherer, because that is where it is actually paid — on the delivery
path of whoever needed the value. A gatherer that appears cheap while
the station using it appears slow is exactly the confusion this
prevents.

**Counting must be nearly free or it changes what it measures.**
Per-station counters written under the mutex that is already held cost
nothing extra. Timing is more delicate — a clock read on every box
invocation is real overhead on short boxes. Build it so the whole
apparatus compiles out.

## Suggested implementation steps

1. Add the counters to the station struct, updated where a lock is
   already held.
2. Timing around the shim call and around the mutex acquisition, behind
   a compile-time switch that removes it entirely when off.
3. Charge gather time to the pulling station.
4. A report sorted by total time, by contention, and by count — three
   orderings of the same data, because the interesting station is a
   different one under each.
5. Emit to `tmp/shared-memory/` and on demand.
6. A test that a map with a known-slow box attributes the time to the
   right station.
7. Measure and report the overhead of the instrumentation itself, with
   it on and off, so the number is known rather than assumed.

## Related

- Issue 701 — the other half of the picture
- [002 — Stations and ports](../../docs/002-stations-and-ports.md)
