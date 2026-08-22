# Phase 7 progress — seeing inside it

Phase 7's goal: everything that makes the engine legible while it
runs. None of it required for correctness, all of it required for
confidence.

| Issue | State | In one line |
|---|---|---|
| 701 — buffer growth reporting | complete | Both piles named; periodic observer; loud shutdown word. |
| 702 — station statistics | **extended** | Counts always on; timing compiles in and out. The gather column goes; an output-buffer warning arrives. |
| 703 — map dump | **extended** | The live table as a map file; needs a bytes-to-text formatter once statics leave the table. |
| 704 — runtime rewiring | **extended** | Check and change under one lock; the snapshot it added is replaced, and adding a station stops being out of scope. |
| 705 — HTML documentation | in progress | Generator and site stand; two widgets and deep links remain. Discovery now walks rather than being told where to look, and the output is swept of pages no source produces — which one working session of renames had made a chore. |
| 706 — phase 7 demo | complete, **artifact deleted** | Live view, bottleneck relieved mid-run, dump truth, measured measurement. The program it built is gone; the account of what it proved stands. |
| 707 — demos as word problems | complete, **contract replaced** | All 35 scenes open with a story and report in its units; two presenters own the layout. The justification rule survives into the panels; the scene shape does not. |
| 709 — slideshow and transcripts | open | An introduction, and the conversation logs as a book. |
| [710 — demos after the pull path](710-demos-after-the-pull-path.md) | **superseded** | Every scene that showed pulling is rewritten; the launcher reads a list instead of globbing. The three programs it repairs no longer exist — see 713. |
| [711 — the index means reading order](711-the-index-means-reading-order.md) | **rule written, tool built**, order not yet changed | An index means where a file sits in the reading order and nothing else — stated in the table of contents, where a reader arrives. The renumbering tool takes a desired order, refuses anything missing or duplicated, renames through git and rewrites every reference; its `--check` mode reports indexed filenames mentioned anywhere that are not files, and found two stale ones in a header on its first run. What is left is the judgement: four places the sequence stops reading well are written down, and moving fifty files should be argued about first. |
| [712 — capturing a running program](712-capturing-a-running-program.md) | open | Not the schematic but the whole thing: drain the pool, write out every value still sitting in a port, and revive it exactly there. Feasible only because a box may not remember anything between calls, so there is no hidden state to miss. |
| [713 — the demos you can steer](713-demos-you-can-steer.md) | open, **in progress** | The paged demos are deleted and replaced by one live control panel per phase: three regions, levers the reader moves, and a log of what the engine did about it. |

**The demos were deleted, and the reason is a disagreement about
shape.** Seven paged demos ran, measured, and ended, and a reader
advanced through a transcript of decisions somebody else had made.
This engine's three most unusual capabilities — a write being an
event, a shape that can be edited while it runs, and code arriving
after the program started — have no subject until somebody outside the
program acts. A demo that supplies its own acts demonstrates each of
them with the interesting half removed. What replaces them stays up
until the reader quits and hands them the dial.

Every phase's demo issue therefore now describes an artifact that
lived and was replaced rather than one sitting in the tree. They are
left exactly as written: they are the record of the first generation,
and 713 is where the second is described.

**Growing the station table left this phase.** It was written here as a
late convenience for a running program; under one construction surface
it is the mechanism underneath every program's *first* moment, so it
moved to phase 2 where stations come from.

Phase 7 stands with the HTML set honestly open. The engine is legible:
nothing hidden, most things alterable, every claim measured — and now
legible to somebody who did not build it, which is a different and
harder property.

Notes for the phase: reporting, dumping, and rewiring landed as
three modules behind one header. Rewiring required delivery to
snapshot destination lists under the station mutex — a retrofit the
second pass should design in from phase 2. The second pass is going
further than that: the snapshot is replaced entirely by an immutable
array published in one atomic write, so the delivery walk takes no lock
at all.

Two diagnoses this phase drew are becoming three. A slot piling up
means uneven inputs; the task ring piling up means slow consumers; and
an **output buffer** piling up means nobody is collecting the program's
results. The third is louder than the others by design — the first two
are performance signals reported at teardown, and the third fires from
the first doubling, because it means the program is computing into
somewhere nobody is looking.

The storytelling pass paid for itself in a way nobody expected: giving
the overhead scene a story forced somebody to ask what its number
meant, and the number turned out to be wrong — the two builds were
being timed in two blocks, so the second inherited a warm machine.
Writing down what a measurement is supposed to show is a way of
checking that it shows it.
