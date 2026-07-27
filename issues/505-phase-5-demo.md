# 505 — Phase 5 demo: a map that decides

## Current behavior

Phase 4's demo shows values that are current rather than merely
correct. Every map so far has been a pipeline — it can transform, but
it cannot choose.

## Intended behavior

The first demo where the map has behaviour rather than only shape. It
should make one thing obvious: **the branching is visible in the
wiring, not buried in a function.**

**What it should show, in order of how convincing it is:**

**A sorting network.** Values entering, comparators splitting them,
buckets filling. Report the final distribution against what it should
be. This is the clearest possible demonstration that routing decisions
are a structural property of the map — and it looks like something.

**The same comparison, six ways.** One comparator, its three ports
rewired six times to produce every comparison operator, with the
routing counts reported for each. This is the argument for having no
operator setting, made in numbers: six behaviours from one station and
three arrows.

**Something no operator can name.** Equal routed somewhere different
from greater. Report where each value went. This is the part that
cannot be expressed with an operator field at all.

**Even spreading under uneven load.** An iterator feeding several
consumers with deliberately different durations. Report each
destination's count and the order values arrived in. The counts should
be even; the order should not be. Both halves are the point — it is a
spreader, not a funnel.

**Concurrent cursor safety.** Saturate an iterator and report whether
any two concurrent tasks were assigned the same port in one cycle. The
answer should be no, and it is the property the mutex placement exists
to guarantee.

**A comparison that would have been wrong.** Route on negative
floating-point numbers, and alongside the correct result, show what
comparing the raw bytes would have produced. The two disagree, and the
disagreement is why the compare function exists.

**Reuse from earlier phases.** Same pool, same stations, same
generator, same gatherers. Report the occupancy figure again so all
five demos read against each other.

## Suggested implementation steps

1. Build the maps with the construction calls from issue 207, extended
   by 502 and 504.
2. Draw the sorting network as text with live bucket counts, redrawn as
   it runs — this is the phase where a visual earns its place.
3. Report every number by measuring it.
4. Write results to `tmp/shared-memory/` alongside the screen.
5. Confirm the root launcher finds it.

## Related

- [005 — Routing](../docs/005-routing.md)
- Issues 501 through 504 — everything being demonstrated
- Issue 406 — the phase 4 demo this builds on
