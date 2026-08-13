# 710 — The demos after the pull path

**The window this issue was written to wait for is now open.**
[210a](completed/210a-the-pull-path-removed.md) removed the pull path,
so the three demos that call it no longer compile — phase 4 fails on
the slot-conversion call, phase 6 on the gatherer tag, phase 7 on the
runtime repoint. Each says so plainly and names the missing call,
which is the honest failure, but the project's front door currently
offers three entries that will not build.

That was foreseen and is the reason this issue exists. It could not be
worked earlier — the demos are compiled against the engine, so
rewriting them ahead of the removal produces programs that do not
compile either — and it should not wait long, because a launcher
demonstrating a machine that is gone is worse than one demonstrating
a machine that is unfinished.

**What it still waits on is narrower than it was.** Phase 4's rebuilt
scenes turn on writing a static and watching a chain recalculate,
which needs the statics work ([401](401-static-slots.md),
[405](405-statics-mutation.md)) standing on the port record
([210b](210b-the-port-record.md)). Phase 6's substitution needs the
same. Phase 7's replacement scene needs only
[309](309-types-by-width.md), and could go first.

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

1. Phase 7's scene first, since it needs only
   [309](309-types-by-width.md) and restores one of the three broken
   demos immediately. Then wait for the statics work — phases 4 and 6
   demonstrate a mechanism that does not exist yet.
2. Phase 4 end to end, since it is effectively a new demo rather than
   an edited one, and it establishes the voice for the other two.
3. Phase 6's substitution and phase 7's, which are single scenes.
4. Rename phase 4's source and runner to say config rather than pull,
   keeping the index, and chase every reference to the old names —
   the launcher, the phase demo issue, the docs site's generated
   pages, and anything in the transcripts index that points at them.
5. Replace the launcher's filename glob with a read of an ordering
   file in the demos directory, and write the new rule into the
   conventions, since the first-pass report asked for it in writing
   rather than only in code.
6. Run every demo from the launcher, which is the standing rule after
   any demo change.
7. Update each phase demo issue's current behavior to describe the
   scenes as they then read — and remove the marks this issue's
   existence made redundant.

## Open questions

**Answered:**

- *Does phase 4 keep its number and name?* It is renamed and keeps its
  index. The source becomes `037-phase-4-config-demo.c` and the runner
  becomes a config demo, matching the roadmap, which already calls the
  phase Configuration and leaves the demo as the last place the word
  "pull" survives. The index stays 037 because **an index is a
  position in the reading order, not an identity** — it says where
  this work sits in the story, between the generator and routing, and
  that has not moved. What changed is what the demo demonstrates, not
  when it arrived. Re-indexing it to the end would have said the
  opposite, and would have put phase 4's demo after phase 7's work in
  the one ordering the project uses to be read.

- *How is the launcher's discovery pattern reconciled with the file
  index rule?* **The launcher stops discovering and reads a list.** A
  file in the demos directory names the runners in the order they
  should be offered, and the launcher reads it instead of globbing for
  a filename shape.

  This costs the property the launcher's own header advertises — a new
  demo appearing merely by being dropped in place — and buys something
  worth more: **run order and reading order stop having to be the same
  thing.** The filename says where a file sits in the project's story;
  the list says where a demo sits in the menu. Neither has to bend to
  the other, and the collision the first-pass report recorded stops
  being a collision because the two conventions no longer share a
  channel. The list also becomes the place a demo can be temporarily
  withheld or reordered without renaming anything, which globbing
  never allowed.

- *What index do the runner scripts get, now that the list allows them
  one?* **Their source's index with a letter appended** —
  `037-phase-4-config-demo.c` is run by `037a-phase-4-config`. The
  numbers around each demo source are spent on engine files, so a
  runner cannot have a number of its own near the thing it runs; a
  letter gives it one anyway. The project already uses exactly this
  shape for issues that split into parts, where 522 becomes 522a and
  522b, and the relationship is the same one: **a runner is not a step
  in the story, it is the second half of the step its source is.**

  Taken as a judgment call rather than a preference, on the grounds
  that the alternative — seven fresh numbers at the end of the
  counter — puts phase 1's runner after phase 7's engine work, which
  is the one thing an index is supposed to prevent.

**Open:**

- **The index convention does not visibly do what it is for.** The
  numbers are supposed to encode a reading order for the whole
  project, and nothing in the project makes that apparent — a reader
  meeting `033-statics.c` beside `034-gather.c` has no way to learn
  that the numbers mean "read these in this order" rather than
  "these were created in this order" or "these are related." The
  convention is followed carefully and communicates nothing to anyone
  who was not told about it. Whether the fix is a document that states
  the rule, a generated reading index that lists every file in order
  with a line about each, or something the docs site renders, is
  undecided. Raised while numbering the runners, and larger than they
  are.

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
