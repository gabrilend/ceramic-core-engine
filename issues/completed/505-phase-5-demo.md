# 505 — Phase 5 demo: a map that decides

## Current behavior

Built and discoverable from the root launcher. The sorting network
leads: three comparator stations and nine arrows split four hundred
values into four buckets drawn live as they fill, landing exactly on
the expected distribution with no bucket-choosing code anywhere. One
comparator rewired six ways produces every comparison operator in
numbers — twenty or forty arrivals per sixty values, exact — and
then the seventh shape no operator can name: equals one way,
greaters another, lessers discarded. The iterator deals ninety
values across consumers burning ten, two hundred, and eight hundred
units into exact thirty-thirty-thirty, with the arrival order
printed to show the disorder fairness permits. The byte-lie scene
routes minus two below one half and prints both doubles as unsigned
words beside the verdict — the sign bit reading as magnitude — with
a note that memcmp on little-endian lies differently, a nuance found
while building it. Mirrored to the shared-memory tier.

Each scene opens with a story and reports in that story's units beside
the engine's own, under the standard issue 707 sets: a coin sorter
that holds no opinions, a doorman with one height stick and three
doors of which only some are unlocked, a dealer who is scrupulously
fair at a table that never looks it, and a ledger where debts are
written in red. The byte-comparison scene keeps its fixed pair rather
than drawing one, because a drawn pair could fail to demonstrate the
lie at all; the demo says so where it does it.

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
