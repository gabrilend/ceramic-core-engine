# 710 — The demos after the pull path

**This cannot be built before the engine changes it describes.** The
demos are runnable programs compiled against the engine as it exists,
and they call the pull path directly. Rewriting them first produces
programs that do not compile. Rewriting them last means the launcher
demonstrates a machine that is gone. So this issue exists to be *ready*
when the implementation lands, not to be worked ahead of it.

## Current behavior

Three of the seven demos have scenes that demonstrate a capability
being removed ([056](../docs/implementation-notes/056-no-pull-path.md)).

**Phase 4 is worst: four of its five scenes.** Frozen versus fresh, the
gather tax, a chain three deep, and the refused cycle. Its source
places a gatherable station, wires a slot to gather from it, allocates
a statics table, and times the difference — every one of those calls is
going. What survives is the knob turned mid-run, which was the least of
the five scenes and is about to be the most important.

**Phase 6 loses three pieces.** The pushed-plus-gathered pair that
leaves 14, the gathered addend in the everything-map, and the refused
gather cycle among its six deliberate failure modes. Its explanation of
the seed — station by station, "with the vacuously-ready gatherer
correctly skipped" — describes a sweep that no longer happens.

**Phase 7 loses one scene and keeps its shape.** The refused rewire
attempts an edit that would create a gather cycle.

The other four demos are untouched: nothing in the pool, the graph, the
generator, or routing depended on anything being pulled.

## Intended behavior

**Every demo demonstrates the engine that exists, and the phase 4 demo
demonstrates what replaced the pull path rather than apologising for
its absence.**

### Phase 4, rebuilt around the write

The phase's mechanism is now one sentence: **writing a static runs the
ordinary readiness check on the station holding it.** Everything worth
showing follows from that.

- **A value written into a static port propagates.** Change one value
  at the top of a chain and watch every station below it recalculate,
  once, with each consumer reading the same result. This is the scene
  that replaces frozen-versus-fresh, and it makes a stronger point: not
  that a value can be current, but that a *program* can be.
- **Computed once, read by many.** Two consumers of one upstream chain.
  Under the pull path each would have pulled it and it would have run
  twice; now it runs once. A count, measured, beside what the old
  number would have been.
- **A write cannot make something run that could not run anyway.** A
  station with a ring port that is empty, its static written ten times,
  running zero times — then one value arrives and it runs. The
  readiness check is the same check reached from a different direction,
  and showing it refusing is how a reader learns it is not a special
  case.
- **What was given up, stated plainly.** A value is now as fresh as the
  last write, not as fresh as its use. Show the gap. A demo that only
  shows what a change bought is an advertisement.
- **And the knob turned mid-run survives**, promoted from a footnote to
  the thing the phase is about.

### Phase 6, one substitution

Replace the removed pieces with a **static written while the program
runs**, recalculating everything downstream without a restart. It is a
text edit like the demo's other two, it demonstrates the mechanism that
replaced the pull path, and it makes the phase's claim more strongly
than the scenes it replaces: the shape of a program can change while it
runs, not only between runs.

The everything-map should still exercise every phase at once — it just
exercises statics-with-writes where it exercised gatherers.

### Phase 7, a different illegal edit

Keep the scene; change what is refused. A wire whose types do not match
is the natural candidate, and under
[309](309-types-by-width.md) it reads better than a cycle ever did: the
message can name the first field where two layouts diverge rather than
only reporting that two names differ.

### What must not be lost

**The measuring lesson from phase 4.** Its gather-tax number was wrong
the first time, because gathering happened at task assembly and seeded
values paid on the seeding thread, so the timing wrapped only the
running and measured nothing. *Where a cost lands is not where you
expect to find it* is a lesson about measurement, not about gathering,
and it cost a real mistake to learn. Whatever replaces that scene
should carry it.

**The story contract from [707](completed/707-demos-as-word-problems.md).** Every
new scene opens with the problem in the engine's own terms, offers one
analogy never reused inside that demo, prints the mapping, justifies
every row of it, measures in the story's units beside the engine's, and
ends with a finding. A scene that cannot be given a story that makes a
reader care is evidence about the mechanic, not about the story.

## Suggested implementation steps

1. Wait. Land [210](210-input-port-record.md) and the statics work
   first; nothing here compiles before they do.
2. Phase 4 end to end, since it is effectively a new demo rather than
   an edited one, and it establishes the voice for the other two.
3. Phase 6's substitution and phase 7's, which are single scenes.
4. Run every demo from the launcher, which is the standing rule after
   any demo change.
5. Update each phase demo issue's current behavior to describe the
   scenes as they then read — and remove the marks this issue's
   existence made redundant.

## Open questions

- Does phase 4 keep its number and name? It is called the pull demo,
  the phase is no longer called the pull path, and the file carries an
  index that means something in the reading order. Renaming it is the
  same class of act as the two documents renamed alongside this work.
- The launcher discovers demos by a `phase-*` filename while the source
  carries a file index — a naming collision the first-pass report
  already recorded. Rewriting a demo is the cheapest moment to resolve
  it, or the worst moment to try.

## Related

- [056 — Why there is no pull path](../docs/implementation-notes/056-no-pull-path.md),
  the change that forces this
- [406](completed/406-phase-4-demo.md),
  [606](completed/606-phase-6-demo.md), and
  [706](completed/706-phase-7-demo.md), each of which names what it
  needs
- [707 — The demos told as word problems](completed/707-demos-as-word-problems.md),
  the contract every new scene is held to
- [004 — Statics and recalculation](../docs/004-datapath-statics.md),
  the mechanism phase 4 now demonstrates
- [309 — Types compared by width](309-types-by-width.md), which gives
  phase 7's refusal a better message than the one it replaces
