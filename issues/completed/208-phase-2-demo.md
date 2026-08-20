# 208 — Phase 2 demo: a graph that runs itself

> **The program this built was deleted ([713](../713-demos-you-can-steer.md)).**
> Every word below stands as the record of what the first generation of
> demos was and what it proved; what replaced it is a live control panel
> per phase rather than a paged transcript. Nothing here needs rebuilding.

## Current behavior

Built and discoverable from the root launcher. Five scenes, all
measured: a sixteen-wide map reaching full worker occupancy with
nobody arranging it; one station with fourteen bodies inside it at
once and an intact checksum; the two kinds of backlog shown side by
side — a slow single-input consumer piling tasks into the pool's ring
while its slot stays one deep, and a pairing station's starved side
growing its buffer to hold four hundred waiting values (a distinction
the design docs did not draw; docs 002 now corrected, first-pass
report carries the lesson); fan-out cost measured at widths one, ten,
and a hundred; and a live text view of six hundred values pooling
behind a ticket gate and draining as tickets trickle in. Results
mirror to the shared-memory tier.

Each scene opens with a story and reports in that story's units beside
the engine's own, under the standard issue 707 sets: a print shop with
no foreman, one recipe card and many cooks, a restaurant pass where a
backlog on the ticket rail and a backlog on the warming shelf mean
opposite things, a dispatcher who dials every depot herself, and a
queue on a pavement outside a ticketed door. The backlog scene quotes
phase 7's buffer report rather than paraphrasing it, and the demo says
up front that lines beginning "observe:" are the engine's own alarm on
the error stream rather than something going wrong.

## Intended behavior

The first demo where the shape of the program is visible. It should
make one thing obvious to someone watching: **nobody scheduled any of
this.** A map was described, values were dropped in, and the machine
filled every core on its own.

It reuses phase 1's pool wholesale — same queue, same workers, same
termination — and adds stations on top, which is the point of a phase
demo. Show the old tool doing new work.

**What it should show, in order of how convincing it is:**

**Occupancy across a wide map.** A graph wide enough to saturate the
machine. Report how many workers were busy over time, sampled, against
the theoretical maximum. The interesting number is how close a graph
gets to full occupancy without anyone arranging for it.

**Concurrent invocations of one station.** Feed a single station far
more values than it can consume at once and report how many of its
invocations overlapped in time. This is the property that most needs
proving, because it is the one the design paid for by forbidding boxes
to remember.

**Buffer growth under mismatched rates.** A producer deliberately
faster than its consumer. Report each slot's growth count and final
capacity. This shows the growth path working and shows what it looks
like when a map is unbalanced — which is the same reading phase 7 will
later surface automatically.

**Fan-out cost.** One station wired to increasing numbers of
destinations. Report the time the delivering worker spends walking
the list against the time it took to run the box. This is where the
"one worker does a hundred lock-and-check cycles" decision either
justifies itself or does not.

**A visual, if it is cheap.** The map drawn as text — stations, arrows,
and a live count of values sitting in each buffer — redrawn as it runs.
Watching values pool up behind a slow station explains backpressure
better than any number does.

## Suggested implementation steps

1. Build the demo map with the construction calls from issue 207.
2. Instrument by measurement only. No number in the output should be a
   constant written into the demo's source.
3. Write results to `tmp/shared-memory/` as well as the screen so runs
   can be compared.
4. Confirm the root launcher finds it.

## Related

- [003 — Delivery](../docs/003-datapath-delivery.md)
- Issues 201 through 207 — everything being demonstrated
- Issue 105 — the phase 1 demo this builds on
