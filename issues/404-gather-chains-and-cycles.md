# 404 — Gather chains and the cycle check

## Current behavior

A gatherer's slots may be static. If one is itself gathered, the
gathering recurses — which works, and which will recurse forever if two
gatherers point at each other.

## Intended behavior

**Gatherers may chain.** A gatherer's slot may itself be gathered, and
the chain is walked inline during task construction. Depth is a static
property of a map, so the cost is bounded and knowable rather than
open-ended.

**A cycle in a gather chain is fatal**, and it is fatal in the worst
possible way. Gathering is a function call that has not returned yet,
so a loop recurses until the stack dies — which surfaces as a segfault
with no message, no line number, and no indication that two boxes in a
map point at each other. It is the one failure mode that tells the
programmer nothing at all.

So cycles are caught when a connection is made, not when it is
traversed.

## Why the check is cheap

**If the graph is acyclic before an edge is added, any cycle that edge
creates must run through it.** There is never a reason to scan the
graph. Start at the new wire's destination, walk forward along gather
links, and see whether you arrive back at its source.

Start from an empty map, refuse every edge that closes a loop, and the
map is acyclic forever by induction. The cost is the length of one
chain, paid once per connection — a few hundred at load, occasionally
at runtime once phase 7 allows rewiring.

The same walk yields the chain's depth for free, which is the
worst-case inline work a worker will do while assembling a task. Worth
recording while it is in hand.

## The check applies only to gather wires

**A cycle in the push direction is legal and necessary.** Since a box
cannot remember anything, a loop through a ring buffer is the only way
to build a counter: the value goes out, comes back around, and arrives
as the next run's input. A blanket cycle check would forbid the sole
mechanism the engine has for carrying state.

The difference is that a push cycle passes through a buffer and the
call ends. A gather cycle is a call that never returns.

## Suggested implementation steps

1. The forward walk along gather links from a proposed edge's
   destination, looking for its source.
2. Call it from the connection path, refusing the edge and naming both
   stations when it closes a loop.
3. Record the deepest chain found, for phase 7 to report.
4. Make sure the check is reachable from the runtime rewiring path in
   phase 7, and note there that the check and the insertion must happen
   under one lock — two threads each adding an individually legal edge
   can produce an illegal pair.
5. A test of a legal chain several deep, asserting the value arrives
   and the recorded depth is right.
6. A test that a two-station cycle is refused, and one that a longer
   cycle is refused.
7. A test that a push cycle — a counter looping through a ring buffer —
   is allowed and counts correctly.

## Related

- [004 — Gathering](../docs/004-datapath-gather.md)
- Issue 403 — the gathering this bounds
- Issue 704 — runtime rewiring, which reuses this check
