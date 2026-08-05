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
| 705 — HTML documentation | in progress | Generator and 78-page site stand; two widgets and deep links remain. |
| 706 — phase 7 demo | complete | Live view, bottleneck relieved mid-run, dump truth, measured measurement. |
| 707 — demos as word problems | complete | All 35 scenes open with a story and report in its units; two presenters own the layout. |
| 709 — slideshow and transcripts | open | An introduction, and the conversation logs as a book. |

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
