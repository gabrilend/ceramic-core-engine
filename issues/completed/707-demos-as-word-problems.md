# 707 — The demos told as word problems

> **The contract below was replaced, not withdrawn ([713](../713-demos-you-can-steer.md)).**
> The scene shape it defines went with the paged demos. Its finding did not:
> a control that does not say what it stands for teaches nothing, and the
> panels carry that rule per lever exactly as this carried it per scene.

## Current behavior

Done. All thirty-five scenes across the seven demos follow one shape:
state the problem in the engine's own vocabulary, offer an image to
hold it by, tabulate the correspondence between the two, justify every
row of that table, measure, and end with a finding. A hundred and
forty-two justified correspondences, one per row, none optional —
the interface has no way to name a correspondence without also saying
what makes it one.

The demos are paged rather than printed. Each scene fills the screen,
waits for a keypress, and then erases exactly the rows it wrote before
the next one starts, so the reader faces one scene rather than a
transcript. Four scenes are live panels the reader steers: the pool's
ring under fan-out, backpressure at a gated station, the statics dial
turned mid-run, and the map under load in phase 7. Colour carries one
meaning each — engine terms, story terms, measured figures, keys to
press — consistently across all seven.

Two presenters own the layout, one compiled and one shell, matched
column for column so the two families of demo are indistinguishable on
screen. Scenes measure into a record of facts and tell from it
afterwards, so a rewording cannot disturb a measurement. Values are
drawn per run from a seed printed in the banner, except where a
mechanic depends on the exact value, which the scene says where it
happens.

A run that is not a terminal — piped, redirected, or in a test — gets
no paging, no colour and no waiting, plays the live panels' scripted
version, and prints every scene at once. It says so in its banner
rather than pretending.

Three things were found while doing it, all fixed:

**The phase 7 overhead scene had been measuring the wrong thing.** It
reported the instrumented build as *faster* than the plain one, which
is impossible. Best-of-five made the claim more confident rather than
less wrong: the two builds were being run as two blocks, so whichever
ran second inherited a warm cache and a processor already scaled up.
The runs now alternate after a discarded warm-up and the cost lands
around two percent. Repeating a measurement carefully is not the same
as taking it fairly.

**The report file was optional.** All the compiled demos would print a
notice and carry on to the screen alone, then close by announcing
where the report had been mirrored. Opening it is now part of running.

**Wrapping was done twice, badly.** Stories are written across several
source lines for whoever reads the source; the shell presenter's first
version folded each of those lines separately, so paragraphs broke
wherever the source happened to break. Paragraphs are flattened before
they are folded.

**Done.** The last outstanding item was the HTML set, which had no page
for this issue and none of the updated current-behavior sections of the
seven demo issues. The set has been regenerated — ninety-eight pages,
no unresolved references — and the caution recorded here turned out to
be unnecessary: the generator was not mid-edit in any dangerous sense,
only carrying a complete eleven-line pass that gives implementation
notes their own heading in the sidebar. Waiting was still the right
call at the time, because the cost of being wrong was publishing
somebody's half-finished work and the cost of waiting was nothing.

**One thing this issue's scenes will outlive is the engine they
describe.** Four of the phase 4 demo's five scenes, three pieces of the
phase 6 demo, and one phase 7 scene demonstrate the pull path, which is
being removed. The story contract laid out below is what their
replacements are held to, and
[710](../710-demos-after-the-pull-path.md) says what each one needs. That
is a different issue rather than a reopening of this one: the shape of
a scene is settled and proven across thirty-five of them; what changes
is which mechanics there are to tell stories about.

The rest of this issue is the specification the work was done against,
kept because it is what the demos are now held to.

### What it was before

Every number was measured on the run and each scene ended with a line
saying what the number meant. What was missing was the beginning of
each scene: nothing set up what was about to be shown, in terms of
anything a reader already had intuition about.

The consequence is that the demos read as a report from someone who
already understands the engine. A scene announces "the gather tax",
prints two timings, and concludes that a network call does not belong
behind a gatherer. Every word of that is true and none of it lands
unless the reader already knows what a gatherer is.

Three specific gaps:

**No setup.** Scenes open on their measurements. The reader meets the
number before the question the number answers.

**Engine units only.** Times are microseconds per task, capacities are
slots, throughputs are tasks per second. These are the units the
machine thinks in, and they are the right units to *check* against —
but a reader with no model of a "task" cannot tell whether 72
microseconds is alarming or unremarkable.

**Measuring and telling are the same code.** Each demo carries its own
printing helper — the compiled ones a variadic wrapper around printf
that also writes the report file, the shell-driven ones a function
piping through tee. Scene functions interleave taking a measurement
with narrating it, so changing how a demo speaks means editing the
code that decides what it measures.

One presentation defect worth naming separately: the phase 7 map
provokes a load-time warning about its spare station before scene 1
begins, and nothing explains it. It is the loader being correct — the
station has buffered inputs no arrow feeds until the demo rewires it
mid-run — but an unexplained warning in the first five lines of the
final demo teaches a reader to ignore warnings.

And one fallback, found while reading the compiled demos: when the
report file cannot be opened they print a notice and carry on to the
screen alone. A demo that announces "report mirrored at ..." while
having failed to mirror it is lying about where its evidence went.
Opening the report is part of the run; failing to open it should stop
the run and name the path.

## Intended behavior

Every scene in every demo opens with a short story that has tangible
pieces, and reports its findings in the units that story asks for,
beside the engine's own.

Phase demos are the deliverable — the thing someone runs to decide
whether this project is worth their attention. A demo that can only be
read by someone who already understands the engine is a demo for
nobody.

### The shape of a scene

Six blocks, in this order, every time. The order is the argument: a
reader who already knows the engine can stop after the first block, and
a reader who does not gets the mechanism before the picture rather than
instead of it.

1. **The problem, in idiomatic terms.** What the engine does here and
   what question the scene exists to answer. No analogy at all.
2. **The bridge.** One sentence, worded by the presenter rather than by
   each demo, so the hinge between mechanism and picture is identical
   on all thirty-five pages.
3. **The table.** Two columns, engine term against story thing,
   aligned, for pattern-matching at a glance and referring back to.
4. **The justifications.** One sentence per row: "X is like Y because
   …", where everything after *because* is the part that was missing.
   An analogy nobody justified is a claim the reader has to take on
   trust, and these demos are supposed to be arguing rather than
   asserting.
5. **The measurements**, in the story's units beside the engine's own —
   or a live panel, for a mechanic that is a process rather than a
   total.
6. **The finding.**

### The story contract

Six rules. A scene either satisfies all of them or it is cut.

**One analogy per scene, never reused inside a demo.** The point of
several analogies per phase is that each one illuminates a different
piece of the machinery. Reusing an analogy across scenes collapses two
mechanics into one picture and hides the difference between them.

**The mapping is printed, not implied.** Each story names which of its
tangible pieces stands for which part of the engine. A reader who
distrusts the analogy can check it; a reader who trusts it gets a
vocabulary for the rest of the scene.

**Story units are relabelled measurements, never invented ones.** If a
story counts letters through a sorting office, the letter count is the
task count and nothing else. The engine's own number is printed beside
it on the same line. Nothing is computed for the story alone, and no
figure appears that the run did not produce.

**Plausibility is not required; valid mechanics are.** A scenario may
be absurd so long as every relationship inside it holds. Treat them as
math word problems: the trains do not have to be real trains.

**Randomized where the mechanic does not depend on the value.** Counts,
payloads and orderings vary per run, with the seed printed so a run can
be repeated. Determinism belongs in the tests, which check negative
numbers, boundary values and rotated flags; the demos exist to
illustrate mechanics, and a figure that is identical every run invites
the suspicion that it was written rather than measured.

**A scene that needs dressing up gets cut instead.** If a mechanic
cannot be given a story that makes a reader care, that is evidence
about the mechanic, not about the story. Show only what actually
matters — otherwise, why was that part of the engine built at all?

### Separating what is measured from how it is told

Scene code stops printing. A scene runs its experiment and fills a
record of measured facts; a presenter renders that record through the
scene's story. Two consequences worth having: changing the voice of a
demo cannot disturb what it measures, and the same facts could later be
rendered another way — into the HTML documentation set, for instance,
without running the scene again.

### Paging, colour, and panels the reader steers

A demo is a sequence of pages rather than a stream. Each scene fills
the screen, offers itself, waits, and then erases precisely the rows it
wrote — counted as screen rows rather than lines, since a line longer
than the window costs two. Never the whole screen: whatever the reader
had above the demo is theirs. Everything erased is in the report, so
nothing is lost by clearing it.

The terminal is put into a mode where one keypress advances a page,
with the interrupt signal deliberately left enabled so ctrl-c stays a
signal rather than becoming a byte some read loop has to remember to
check for. Both a handler and an exit hook restore the terminal,
because a demo that dies owing the shell its settings leaves somebody
typing blind.

Colour carries one meaning each and the same meaning everywhere: the
engine's vocabulary, the story's vocabulary, a measured figure, framing
text, a key that can be pressed. A reader who has to relearn the scheme
per scene is worse off than one given no colour at all.

Where a mechanic is a process rather than a total, the scene is a live
panel instead of a table — a frame redrawn on a timer, with keys that
change the conditions while it runs. Frames are screen-only; mirroring
a redraw loop would bury the report under thousands of near-identical
pictures, so the panel writes nothing and the scene reports its outcome
afterwards in the ordinary way. Four scenes qualify: ring growth under
fan-out, backpressure at a gated station, the statics dial turned
mid-run, and the live map in phase 7.

This wants shared machinery rather than seven copies:

- A presenter for the compiled demos (phases 1, 2, 4, 5, 7), owning
  the report file, wrapping story paragraphs to a fixed width,
  printing the mapping block, and rendering a measured fact in story
  units beside engine units.
- A presenter for the shell-driven demos (phases 3 and 6), offering
  the same shapes so the two families read identically on screen.
- The build front from issue 105 compiles the C presenter into every
  demo, including phase 1 — the presenter is demo scaffolding, not
  engine, so phase 1 linking it does not weaken that demo's claim to
  need nothing above the pool.

### Backfilling from later phases

Where a later phase built a better instrument for something an earlier
demo already claimed, the earlier demo should use it rather than keep
its hand-rolled version. This is not decoration: it is the clearest
possible evidence that the phases compose.

The phase 2 growth scene ends by saying that memory absorbed a
producer/consumer mismatch and that phase 7 would say so out loud.
Phase 7's observer now does exactly that at map teardown, naming the
station and its growth. The scene should point at it deliberately
instead of leaving it to be noticed.

### The scenes

Thirty-five scenes across seven demos, each needing its own analogy:

| demo | scenes | mechanics wanting stories |
|---|---|---|
| 1 — the pool | 4 | queue growth under fan-out, termination with a long tail, the price of idleness, throughput against worker count |
| 2 — the graph | 5 | occupancy without a scheduler, one station overlapping itself safely, buffers absorbing mismatched speeds, what fan-out costs the delivering worker, backpressure watched |
| 3 — the generator | 7 | a box added live, the shim beside its function, the registry whole, the registry against the compiler, every value shape through one call site, the running figure, a box changed out from under its registry |
| 4 — the pull path | 5 | frozen versus fresh, the gather tax, a chain three pulls deep, the refused cycle, a knob turned mid-run |
| 5 — routing | 4 | a sorting network filling, one comparison six ways, even spreading with uneven finishing, the comparison that would have been wrong |
| 6 — the map file | 4 | one binary three programs, the shape changes without the C, the failure modes, every phase in one map |
| 7 — seeing inside | 6 | a live map, a bottleneck found by report, the dump versus the file, the round trip, a refused rewire, the cost of measuring |

## Suggested implementation steps

1. Write the two presenters — one C, one shell — and have the build
   front compile the C one into every demo.
2. Convert one demo end to end as the reference for voice and layout,
   then hold the rest to it.
3. For each scene: choose the analogy, name the mapping, decide which
   measured quantity carries the story's unit, and move the printing
   out of the scene function.
4. Introduce the seeded randomness, printing the seed. The presenter
   owns the generator so every demo draws from one place and one seed
   reproduces a whole run.
5. Remove the report-file fallback: a demo that cannot open its report
   stops and names the path.
6. Point the phase 2 growth scene at the observer's teardown report.
7. Resolve the phase 7 spare-station warning — either the story
   accounts for it where it appears, or the map stops provoking it.
8. Run every demo from the launcher after each conversion.
9. Update each phase demo issue's current behavior to describe the
   scenes as they now read.

## The note that started this

Kept verbatim, because the reasoning in it is the specification:

> hi, we need to update the demos. Not only do they have warnings and
> errors, but also it's very difficult to understand what's going on.
> The UX is poor.
>
> I think we should give a little paragraph before each test explaining
> it in story form. We should map the story description to the
> functionalities that are being tested, and we should measure parts of
> them in the units that the story demands. We should have several
> stories for each demo, each using a different analogy. The idea is,
> if we can correctly and clearly create parallels to some situation,
> problem, or context that is made out of tangible pieces, then we can
> use intuition and previous insight to consider it from multiple
> angles. So, to that end, we should create many different kinds of
> stories for each of the demos that each illustrate a different piece
> or mechanic developed in that phase. If we later go on to replace
> some of the functionality in a later phase, we should update the
> older demo to use the new functionality to illustrate the same thing
> that was built in phase 1. We shouldn't fake it, but rather, we
> should only display the parts of the program that are actually
> important - so, if we just display things that don't matter, or
> aren't really relevant, then why are we even building that phase at
> all?
>
> Treat the examples like math word problems. Provide a scenario, even
> if the scenario isn't realistic, so long as the mechanics and reasons
> are valid then the plausibility isn't relevant. Be sure to use
> randomized values when possible for the demos - the tests should be
> deterministic, and check all sorts of things like negative numbers
> and rotated bools. But those are for determining correctness - the
> phase demos, which this issue is concerned with, are for illustrating
> the mechanics and justifying the phase of development. It is a
> presentation of what has been worked on and specifically why it is
> present.

The errors named in the first line are fixed: every demo links and runs
from the launcher again. The rest of this issue is the second line.

## Related

- Issue 105 — the phase 1 demo, which establishes the launcher and
  build-front convention every demo follows
- Issues 208, 307, 406, 505, 606, 706 — the six other phase demos,
  each of which this rewrites
- Issue 705 — the HTML documentation set, the other half of making
  this project legible to someone who did not build it
- [010 — Roadmap](../../docs/010-roadmap.md), where each phase states what
  it exists to do — the source of what each demo has to justify
