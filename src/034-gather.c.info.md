# 034-gather.c — the pull path, from outside

The one place the engine runs backwards: a gatherer slot's value is
produced by running its upstream station inline at the moment a task
is assembled — fresh when used, not when produced.

## Functions

**map_slot_gather(map, station, slot, source station)** — convert a
ring slot to a gatherer pulling from the named station. Checks: the
source exists, is placed, and returns exactly the slot's size; and
the new edge closes no gather cycle — checked by one forward walk
from the source, because a graph acyclic before an edge can only
gain a cycle through it. A refused cycle names both stations and
stops; a cycle let through would recurse until the stack dies with
no message at all. The deepest chain is recorded on the map for
phase 7.

**gather_claim(map, slot, out)** — internal: run the upstream box on
the calling thread's own stack, its arguments statics or themselves
gathered (bounded recursion), its value landing straight in the task
being built. No pool, no mutex on the source — it has no buffers to
guard.

## The exception, named

This is the one place a box runs without a worker taking it from the
pool. Two workers can be inside the same gathered box at the same
instant, so a gathered box must touch nothing but its arguments and
the world it reads — open, read, close; never a kept handle.
