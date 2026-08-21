# 701 — Reporting buffer growth

## Current behavior

**Built, and gaining a third diagnosis that is louder than the other
two.**

This issue's best decision was refusing to report "a buffer grew" as
one fact. A **port** piling up means uneven inputs — one side of a
station outpacing its siblings. The **task ring** piling up means slow
consumers. Two different diagnoses, written into the output so nobody
has to work out which they are looking at.

There is now a third: an **output buffer** piling up
([209](../209-map-output-collection.md)). It is not a rate mismatch at
all. It means the program's results are accumulating with nobody
collecting them — computing into somewhere nobody is looking. So unlike
the other two, which are performance signals summarised at teardown, it
fires **from the first doubling**, because by the time it is
summarised it is too late to be useful.

Everything else here stands, including the two decisions that read
best in hindsight. **High water leads and capacity follows**, because
capacity is what was allocated and occupancy is how deep the trouble
actually got. And **a non-positive interval is refused rather than
defaulted**, because unwanted diagnostics are a background thread doing
nothing useful — which is the same instinct that keeps every clock read
compiling out of the statistics when nobody asked for them.

The remainder describes it as built.

Built. The buffer report walks every port naming station and port
with doublings, current capacity, and high-water occupancy — high
water leading, since capacity is what was allocated and occupancy is
how deep the trouble actually got. The task ring reports beside the
ports, with the phase 2 lesson written into the output: port piles
mean uneven inputs, ring piles mean slow consumers, two different
diagnoses. Periodic emission runs from a small non-worker thread
that pushes nothing (termination stays sound), appending to a file
in the shared-memory tier; a non-positive interval is refused rather
than defaulted, since unwanted diagnostics are a background thread
doing nothing useful. At teardown any port grown past the shout
threshold is named on stderr, loud, per the standing rule that a
warning is an error nobody has decided about. Proven by a starved
pairing port the report names exactly.

## Intended behavior

**A growing buffer is a diagnosis, not an event.** It means a consumer
is slower than its producer and memory is quietly absorbing the
difference. Nothing is broken — the engine handles it — which is
precisely why it needs saying out loud. A map that works but leaks
memory into one buffer forever is a map with a design problem that
nothing currently surfaces.

**Report per port**: how many times it has grown, its current capacity,
its high-water occupancy, and the station and port it belongs to.

**Report while running, not only at the end.** A buffer that grows
steadily over an hour is a different problem from one that spiked once
during startup, and only a time series distinguishes them.

**High-water occupancy matters more than capacity.** Capacity is what
the engine allocated; occupancy is how deep the backlog actually got. A
buffer at a thousand slots holding two values is a buffer that had one
bad moment. One holding nine hundred is a bottleneck.

Per the project's standing rule, a warning is an error nobody has
decided about yet. Growth past some threshold should be loud enough
that a person decides.

## Suggested implementation steps

1. Add high-water occupancy alongside the growth count, updated on
   write while the mutex is already held.
2. A walk over every station's ports producing the report.
3. Periodic emission to `tmp/shared-memory/`, at an interval that can
   be turned off entirely — an engine that writes diagnostics nobody
   reads is an engine with a background thread doing nothing useful.
4. A loud report at shutdown for any port that grew beyond a
   threshold, naming the station and port.
5. A test with a deliberately mismatched producer and consumer,
   asserting the report identifies the right port.

## Related

- Issue 203 — where the count comes from
- [002 — Stations and ports](../../docs/002-stations-and-ports.md)
