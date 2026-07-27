# 701 — Reporting buffer growth

## Current behavior

Every ring buffer carries a growth count (issue 203) and nothing ever
reads it. A slot that has doubled twenty times looks exactly like one
that has never grown.

## Intended behavior

**A growing buffer is a diagnosis, not an event.** It means a consumer
is slower than its producer and memory is quietly absorbing the
difference. Nothing is broken — the engine handles it — which is
precisely why it needs saying out loud. A map that works but leaks
memory into one buffer forever is a map with a design problem that
nothing currently surfaces.

**Report per slot**: how many times it has grown, its current capacity,
its high-water occupancy, and the station and slot it belongs to.

**Report while running, not only at the end.** A buffer that grows
steadily over an hour is a different problem from one that spiked once
during startup, and only a time series distinguishes them.

**High-water occupancy matters more than capacity.** Capacity is what
the engine allocated; occupancy is how deep the backlog actually got. A
buffer at a thousand cells holding two values is a buffer that had one
bad moment. One holding nine hundred is a bottleneck.

Per the project's standing rule, a warning is an error nobody has
decided about yet. Growth past some threshold should be loud enough
that a person decides.

## Suggested implementation steps

1. Add high-water occupancy alongside the growth count, updated on
   write while the mutex is already held.
2. A walk over every station's slots producing the report.
3. Periodic emission to `tmp/shared-memory/`, at an interval that can
   be turned off entirely — an engine that writes diagnostics nobody
   reads is an engine with a background thread doing nothing useful.
4. A loud report at shutdown for any slot that grew beyond a
   threshold, naming the station and slot.
5. A test with a deliberately mismatched producer and consumer,
   asserting the report identifies the right slot.

## Related

- Issue 203 — where the count comes from
- [002 — Stations and slots](../docs/002-stations-and-slots.md)
