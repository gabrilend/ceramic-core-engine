# Conversation Summary: 121932c7-06a5-40d0-9f3b-afe54319cd3b

Generated on: 2026-08-19 14:34:49

--------------------------------------------------------------------------------

### User Request 1

Hi, the demos aren't running correctly. Can you fix them?


--------------------------------------------------------------------------------

### Assistant Response 1

I'll start by seeing what the demos are and how they're supposed to run.


Phase 1 is fine. Let me check the rest.


Link failures — the shared source now references later-phase symbols. Let me
see the rest and the build scripts.


Now let me read the remaining demo scripts in full.


Now I'll write the shared build front for the demos.


Now updating each demo launcher to use it.


Now let me run every demo end to end.


All seven pass now. Let me check the output for warnings or errors hiding inside
the successful runs.


Let me verify the RAM-tier relink path, which was the second failure mode.


Final full run to confirm everything still passes after the header edits.


All seven demos run clean now (`make test` still passes too).


## What was wrong


Two separate breakages, both in the launcher scripts rather than the engine:


**1. Each demo carried a hand-written list of engine source files** — the ones
that existed when that phase was finished. That looks like layering, but the
engine isn't layered at link time: the station layer reaches into the statics
table, the gatherer, and the observer. So when phase 7 put a shutdown report
inside map teardown, phases 2 through 6 all stopped linking at once, each dying
on a symbol its author had never heard of. Phase 2 and 3 were missing six
symbols; 4, 5, and 6 were missing the observer's two.


**2. The shared-memory symlink was only ever created by `make`.** A demo run
straight from the launcher never calls make, so after a reboot every demo would
finish by announcing "report mirrored at …" and point at a path that wasn't
there.


## What I changed


A shared build front that all seven launchers source. It prepares both RAM tiers
(and relinks shared-memory), regenerates the box registry, and compiles a demo
against the engine — **discovering the engine's sources by pattern instead of
listing them**, the same rule the Makefile already uses for tests. A new engine
file now enrolls itself in every demo the moment it's written. A list of
filenames maintained by hand, beside code it doesn't live next to, drifts every
time; discovery can't.


**Phase 1 stays the exception** and still names its two sources, because the
pool genuinely depends on nothing above it and that independence is the demo's
entire claim. I left a comment saying that if that line ever needs a third file,
something has reached into the pool that shouldn't have.


One nice side effect: phase 2's scene 3 ends with "memory absorbed the
imbalance; phase 7 will say this out loud" — and now that the observer is
linked in, it does, naming the station and its growth right there.


I updated the phase 1 demo's issue file (it owns the launcher convention) and
bumped the file index counter to 059. No engine code was touched.


## Two things for you to decide


**I haven't committed.** The working tree has a lot of in-progress work that
isn't mine — the HTML doc set, `scripts/054-docs-html.lua`, two issue files
moved out of `completed/` and rewritten, and several untracked docs (056–058)
plus the loose notes in the project root. The index counter is the awkward one:
it was already bumped 055 → 058 by that work, and I took it to 059. Do you
want me to commit only my nine files, or is that other work yours to land first?


**The loose `issue-2` in the root asks for more than this.** It says the demos
are hard to follow and wants each scene framed as a story — several analogies
per phase, measured in the units the story asks for. That's a real piece of work
and a different job from making them link. Want me to sort that note into a
numbered issue file so it stops living in the project root?


--------------------------------------------------------------------------------

### User Request 2

okay great. Can you work on issue-2? Make sure it's a real issue file first.


--------------------------------------------------------------------------------

### Assistant Response 2

Let me check for existing related issues and read the demo issue files.


No duplicate exists — this is genuinely new. Phase 7 is the legibility
cluster, so it becomes issue 707.


Now the presenter. First let me check some project conventions the demos already
follow.


The phase 2 demo has a fallback I should note — the report file silently
degrades to screen-only. Let me record that in the issue, then build the
presenter.


Now the C presenter — the layer that separates measuring from telling.


Now wire the presenter into the build front.


Now let me read the phase 1 demo source and convert it as the reference.


Now converting phase 1 as the reference demo.


Four layout defects to fix — a truncated buffer, missing block separation, and
column overflow.


Now shortening the two labels that overflowed their column.


Phase 1 is the reference. Now phase 2 — let me read its source.


The observer shouts on stderr — so the demo must call the buffer report itself
to mirror it. Let me expose the presenter's streams.


Now the phase 2 conversion — five scenes, five analogies.


Two fixes: a missing separator after the quoted engine report, and the
unexplained stderr voice.


Two demos done. Now the shell presenter, for the two shell-driven demos.


Now let me read the phase 3 compiled demo so I can convert both halves together.


Now the phase 3 compiled half — scenes 3 through 6.


Now the phase 3 shell half — scenes 1, 2, and 7.


The shell wrapper is preserving source-line breaks. Let me fix it to reflow
properly.


Phase 3 done. Let me verify the report is one document, then continue to phase
4.


Now converting phase 4 — five scenes.


Good. Now rewriting phase 4's five scene tellings.


Understood — switching to the Write tool. Let me read the current state of the
file first.


Now writing the complete phase 4 demo.


Phase 4 done. Now phase 5.


Now writing the phase 5 conversion.


Phase 5 done. Now phase 6 — the shell-driven capstone.


Two readability fixes in the shell presenter and phase 6.


Now the last one — phase 7, six scenes.


Now phase 7 — the last demo. First the compiled half.


Now the phase 7 driving script, which owns scene 6.


Three fixes in phase 7 — the overhead measurement is noise-dominated and
reported a negative cost.


The instrumented build is consistently faster — that's not noise, that's
run-ordering bias. Let me interleave the runs.


All seven converted. Full verification run.


All 35 scenes run, tests pass, reports are clean. Now the bookkeeping.


Now updating issue 707's current behavior to reflect what was built.


The generator discovers issues automatically. Let me add the required interface
files for the three new source files, per project convention.


Now recording the one piece of deferred work, so the issue isn't closed
dishonestly.


Done. All seven demos run, all 35 scenes converted, `make test` passes.


## First, the issue file


`issues/707-demos-as-word-problems.md` — phase 7, since that's the cluster for
making the engine legible (the HTML doc set lives there for the same reason). It
searched clean: no existing or completed issue covered this. Your note is kept
verbatim inside it, because the reasoning in it *is* the specification.


The heart of it is a six-rule contract: one analogy per scene never reused
within a demo; the mapping printed rather than implied; **story units must be
relabelled measurements, never invented ones**; plausibility optional but
mechanics valid; randomised where the mechanic doesn't depend on the value; and
a scene that needs dressing up gets cut instead.


## Then the work


Every scene now opens with a story, names what each tangible piece stands for,
and reports figures in the story's units beside the engine's own:


```

scene 1 — the depot that rebuilt its own wall


  A parcel depot opens for the day with eight pigeonholes on the sorting

  wall. A single delivery manifest arrives, and reading it turns out to

  name a whole further wave of manifests... the depot is not allowed to

  shut its doors while it builds a bigger wall.


    a manifest               one task struct

    a pigeonhole             one cell in the queue's ring

    shutting the doors       stopping the world to resize


    manifests handled        551 manifests          551 tasks run

    pigeonholes at opening   8                      8 cells

    times the wall grew      6 rebuilds             6 doublings

```


Thirty-five analogies: a parcel depot, a relay race, a toll plaza at night, one
wire basket and many cashiers, a print shop with no foreman, one recipe card and
many cooks, a restaurant pass, a dispatcher on the phone, a queue on a pavement,
a self-cataloguing library, a translator nobody briefed, a parts book, two
surveyors, a loading door, a passport photo, a departures board, a rate card, a
coin sorter, a doorman with a height stick, a card dealer, a ledger in red ink,
a player piano, a signal box, a customs desk, an orchestra's first rehearsal, a
control room, a coned-off lane, as-built drawings, a safety interlock, a
thermometer that warms the room.


**Measuring and telling are now separate functions.** A scene fills a record of
facts; a different function tells it. Two presenters (one C, one shell) own
every margin and column, so no demo decides its own layout and the two families
are indistinguishable on screen. Values are drawn per run from a printed seed
— `SORA_DEMO_SEED` repeats a run exactly.


## The part worth your attention


**Giving the phase 7 overhead scene a story caught a real measurement bug.** It
was reporting the instrumented build as *faster* than the plain one —
impossible, since it does strictly more work. My first fix, best-of-five each,
reported the same thing more confidently. The bias was ordering: the two builds
ran as two blocks, so whichever went second inherited a warm cache and an
already-scaled-up processor. Five repetitions made that bias *precise* rather
than removing it.


Runs now alternate after a discarded warm-up, and the cost lands around 2%. Both
failed attempts are recorded in the script, because the lesson generalises:
repeating a measurement carefully is not the same as taking it fairly.


I also removed a fallback — the compiled demos would fail to open their report
file, print a notice, carry on to screen only, and then close by announcing
where the report had been mirrored. That's now fatal.


## Two things for you


**Issue 707 is still open, deliberately.** One item left: the HTML set has no
page for it or for the seven updated demo issues. That's one run of the
generator and no editing — but the generator is mid-edit under 705, and
running it now would sweep someone's unfinished work into the output. Say the
word once 705 settles and it's one command.


**Still not committed.** The tree has grown *more* concurrent work since we
started — `210-input-port-record` and `506-multi-output-boxes` appeared while
I was writing. My change set is 24 modified files plus 8 new ones, all under
`issues/`. I left the loose `issue-2` note in the project root untouched. Want
me to stage only my files and commit, or is the other work landing first?


--------------------------------------------------------------------------------

### User Request 3

for the demos, we have to connect the narrative to the mechanics a bit more
plainly. Start by setting the scene, and describe the problem we're trying to
solve using the idiomatic terms. Then say "to visualize it a bit easier, try
imagining the thing like a thing." then go through each of the named terms and
explain why each term matches with each thing from the story. "the table is like
a book in the library because each record in the table corresponds to a chapter
in the book" where everything after "because" is the justification, which we're
lacking currently. We need one for each element of the example scene. We should
also list them at the start in a table, so we can pattern match.


also, we should run one scene in the phase demo at a time. The user should
"press space to continue" or similar, while respecting ctrl+c's and such. Also,
we should clear the output we made for that particular scene after we move on.
Not clear the whole screen, but keep track of how many lines we output and clear
that many before writing the next page or exiting the phase demo script (because
we're out of scenes).


also, we should use more color in the phase demo scenes. We should use
dynamically displaying progress bars with tweakable values. Like "push 1 to add
tasks to the task queue" and "push 2 to increase the emulated processor speed"
which really is just, lowering the delay between "operations" in the phase demo
sleeps.


--------------------------------------------------------------------------------

### Assistant Response 3

This is a substantial redesign — narrative structure, paging, colour, and
interactivity. Let me start with the presenter, since everything depends on it.


Now the implementation — colour, paging with erase, raw input, and live
panels.


Expected — the demos need converting to the new API. Let me do phase 1 as the
reference, with a live steerable panel.


Two real errors: the pool already frees tasks, and there's no depth accessor.
Let me fix both without touching the engine.


The structure works. Four refinements: sentence capitalisation, block
separation, and the panel's spacing.


Now let me verify the interactive path under a real pty.


Paging, erasing, colour, and quit all work. Let me guard a double
cursor-restore, then convert phase 2.


Two column defects. Let me fix padding in the presenter so a long term can never
eat its gap.


Now phase 4 — with the factory dial as a live knob, which is the most natural
one in the project.


Now scene 5 — the dial becomes a live knob the reader turns.


Now phase 5 — four scenes, with the coin sorter as a live panel.


Phase 5 done. Now phase 7's compiled half.


Now the shell presenter needs the same narrative structure, paging, and colour
for phases 3 and 6.


Now the phase 3 compiled half's narrative, then its shell half.


Now the phase 3 shell half — scenes 1, 2, and 7.


Phase 3 works. Now phase 6's four scenes.


Now phase 7's shell scene 6, then a full verification pass.


Now updating the issue file and the interface docs to match what was built.


All three changes are in and verified — interactively under a pty and
non-interactively, with `make test` passing.


## The narrative structure


Every scene now runs: **problem → bridge → table → justifications →
measurements → finding.**


```

scene 1 — a queue that grows while it is being used


  The pool holds tasks in a ring of pointers with a fixed number of

  cells. Work arrives in bursts — one task can enqueue a hundred more

  in a single turn — so the ring will sometimes be asked to hold more

  than it has room for. It answers by doubling...


  To picture it more easily, imagine a parcel depot whose sorting wall

  is a grid of pigeonholes...


    in the engine               in the story

    --------------------------  --------------------------

    a task struct               a delivery manifest

    a cell in the ring          a pigeonhole

    doubling the ring           building a larger wall


  A cell in the ring is like a pigeonhole because each holds exactly one

  item, is found by its position rather than by searching, and is

  entirely indifferent to what is in the ones beside it.

```


The problem block is pure idiomatic terms — someone who knows the engine can
stop there. The bridge sentence is written by the presenter, not by each demo,
so the hinge between mechanism and picture is identical on all 35 pages. The
table comes first for pattern-matching, then each row gets its own "because".


**The justification isn't optional — the interface can't express a
correspondence without one.** `scene_stands_for` takes three arguments. That's
142 justified rows across the seven demos.


## Paging


Each scene fills the screen, waits on `[space]`, then erases *exactly the rows
it wrote* — counted as screen rows, not lines, since a line longer than the
window costs two. Never a full clear: whatever you had above the demo stays.
`[q]` quits and says the report still holds every scene.


Ctrl-C works properly: I left `ISIG` enabled so it stays a signal instead of
becoming a byte some read loop has to remember to check, and both a handler and
an `atexit` hook restore the terminal — a demo that dies owing the shell its
settings leaves you typing blind.


## Colour and live panels


Colour carries one fixed meaning each: engine vocabulary, story vocabulary,
measured figures, framing text, pressable keys.


Four scenes became steerable panels, chosen because their mechanic is a
*process* rather than a total:


| demo | panel | keys |

|---|---|---|

| 1 | the ring growing under fan-out | `1` another wave, `2`/`3` clerks faster/slower |

| 2 | backpressure at a gated station | `1` a crowd arrives, `2` release tickets |

| 4 | the statics dial, turned mid-run | `1`/`2` turn it up/down |

| 7 | the map under load | space to move on |


Bars change colour by how full they are. `2` and `3` in phase 1 are exactly what
you described — they scale the arithmetic each task burns, so the workers
visibly outpace or fall behind the queue.


Frames are screen-only. Mirroring a redraw loop would bury the report under
thousands of near-identical pictures, so a live scene still ends with ordinary
measured rows, and those are what the document keeps.


## Two things worth flagging


**A piped run behaves differently, and says so.** No paging, no colour, no
waiting, and the live panels play a scripted version. Its banner reads `(not a
terminal: no paging, no colour, every scene printed at once)`. That's what keeps
`make test` and redirected runs working.


**I found and fixed a real double-free while converting phase 1.** The pool
frees each task after the call returns; I'd added `free(t)` inside the task
functions. Also removed a `pool_queue_depth` I'd assumed existed — the live
depth is now pushed-minus-handled, counted in the demo, which needs no engine
change and is exactly the quantity the story calls "waiting".


Issue 707 and both presenter interface files are updated. Still uncommitted —
the tree has even more concurrent work than before, and 707 stays open on the
one deferred item (the HTML set needs one generator run, blocked on 705 being
mid-edit).


--------------------------------------------------------------------------------
