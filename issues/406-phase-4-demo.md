# 406 — Phase 4 demo: values that are current, not merely correct

## Current behavior

Phase 3's demo shows that writing a box is writing a function. Every
value in the engine still travels forward, so a value is as old as the
moment it was produced.

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

- [004 — Gathering](../docs/004-datapath-gather.md)
- Issues 401 through 405 — everything being demonstrated
- Issue 307 — the phase 3 demo this builds on
