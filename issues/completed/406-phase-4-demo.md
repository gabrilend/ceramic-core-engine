# 406 — Phase 4 demo: values that are current, not merely correct

## Current behavior

**Built, and needing rewriting — four of its five scenes demonstrate a
capability that is being removed.**

Frozen versus fresh, the gather tax, the chain three deep, and the
refused cycle are all about the pull path, which is gone
([056](../../docs/implementation-notes/056-no-pull-path.md)). The knob
turned mid-run survives, and it survives as the *most* interesting
scene rather than the least, because writing a static is no longer only
configuration — it runs the readiness check on the station holding it,
which is what starts a program and what makes a chain of statics
recalculate.

**What the phase needs to demonstrate instead** is that: a value
written into a static port propagating down a chain, computed once and
read by every consumer, with the recalculation visible. And its honest
counterpart — the same program before and after, showing that a value
is now as fresh as the last write rather than as fresh as the moment it
is used, which is the thing given up.

**One finding here outlives its scene and should be kept in whatever
replaces it.** The gather tax measurement was wrong the first time, and
the reason was instructive: gathering happened at task assembly, so
seeded values paid on the seeding thread, and the timing wrapped only
the running and measured nothing. *Where a cost lands is not where you
expect to find it* is a lesson about measuring, not about gathering,
and it cost a real mistake to learn.

The remainder describes it as built.

Built and discoverable from the root launcher. The file read both
ways sits first: the frozen side keeps reporting 111 after the file
turns to 999 mid-run while the gathered side follows — the whole
reason the pull path exists, shown by contrast. The gather tax is
measured honestly (a static second input against a deliberately
expensive gathered one, roughly sixty times the per-task cost), with
a subtlety the measuring itself surfaced: gathering happens at task
assembly, so seeded values pay on the seeding thread — the first
timing wrapped only the running and measured nothing. The chain
scene shows depth three recorded at wiring and the walk's per-task
cost. The refusal scene captures the engine's cycle message verbatim
and then, in a second child, the alternative fate: killed by signal
eleven saying nothing at all. The knob scene bends the output stream
from 1000 to 5000 at the moment of a mid-run write. Mirrored to the
shared-memory tier.

Each scene opens with a story and reports in that story's units beside
the engine's own, under the standard issue 707 sets: a printed
timetable beside a departures board, a rate card beside a telephone to
head office, a question passed along a line of people whose length is
drawn on a chart, two dictionaries that define each other, and a dial
on the wall of a working factory. Values are drawn per run from a
printed seed, except in the byte-comparison scene, where the mechanic
depends on the exact pair and the demo says so.

## Intended behavior

A demo about the direction of flow. It should make one distinction
obvious: **a pushed value was true once; a gathered value is true
now.**

**What it should show, in order of how convincing it is:**

**The same file read both ways, side by side.** Two identical
sub-graphs, one fed by a pushed read at startup, one by a gatherer.
Change the file while both run. The pushed side keeps reporting what it
saw at the beginning; the gathered side follows the file. This is the
entire reason the pull path exists and it is best shown by contrast.

**Gather cost as a tax on delivery.** A gatherer that takes a
measurable amount of time, wired into a station that runs many
thousands of times. Report the delivering worker's time split between
its own work and gathering. This is what "once per task assembled"
costs, and seeing it is what stops someone putting a network call
behind a gatherer.

**Chain depth.** A chain several gatherers deep. Report the depth
recorded at load and the measured inline time per task, so the two can
be compared.

**A refused cycle.** Attempt to wire two gatherers into each other and
show the connection refused, naming both stations. Then show what the
same map would have done without the check — a stack overflow with no
message — so the reader understands what was traded for the walk.

**A threshold turned while it runs.** Alter a statics entry mid-run and
show the routing change from that moment. Best paired with a running
count of which way values went, so the change is visible as a bend in
the numbers rather than a claim.

**Reuse from earlier phases.** Same pool, same stations, same
generator. Report the occupancy figure again so all four demos can be
read against each other.

## Suggested implementation steps

1. Build the demo maps with the construction calls from issue 207,
   extended by 401 and 403.
2. Have the file-changing and statics-turning happen visibly as the
   demo runs, not before it starts.
3. Report every number by measuring it.
4. Write results to `tmp/shared-memory/` alongside the screen.
5. Confirm the root launcher finds it.

## Related

- [004 — Statics and recalculation](../docs/004-datapath-statics.md),
  which describes what this demo now has to show instead
- Issues 401 through 405 — everything being demonstrated
- Issue 307 — the phase 3 demo this builds on
