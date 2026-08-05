# 105 — Phase 1 demo: the pool under load

## Current behavior

Built and discoverable from the root launcher. Four scenes, every
number measured on the run: a fan-out burst that doubles the queue six
times from its starting eight cells without ever overflowing; four
wildly uneven chains whose termination decision costs a twentieth of a
millisecond after the last task ends; a trickle across a second of
near-total idleness costing three milliseconds of processor time —
three orders under what spinning would burn; and the same two hundred
thousand tasks run at every power-of-two worker count, where the
single queue mutex's ceiling becomes visible as the line flattens.
Results mirror to the shared-memory tier for comparison across runs.
The demo's C source carries the file index; the launcher script keeps
the phase-* name the launcher discovers by, a naming collision the
first-pass report records.

Every phase's launcher shares one build front, which prepares the two
RAM tiers, regenerates the box registry, and compiles a demo against
the engine — the engine's sources discovered by pattern, never listed.
Each launcher used to carry its own list of the files that existed
when its phase finished, which reads like layering but is not: the
station layer calls into the statics table, the gatherer, and the
observer, so the moment the observer put a shutdown report inside map
teardown, phases 2 through 6 all stopped linking. Phase 1 is the one
launcher that still names its sources, linking the pool alone, because
the pool's independence from everything above it is exactly what this
demo claims.

Each scene opens with a story and reports its figures in that story's
units beside the engine's own, under the standard issue 707 sets: a
parcel depot rebuilding its wall without shutting its doors, a relay
race where the question is how long the floodlights stay on after the
last baton, four toll attendants who doze rather than idle their
engines, and a shop whose cashiers all reach into one wire basket.
Every scene is two functions, one measuring and one telling, and the
counts vary per run from a seed printed in the banner.

## Intended behavior

A runnable program in `issues/completed/demos/` that puts the pool
under conditions it will actually meet and reports what happened in
numbers rather than prose.

Phase demos are part of the deliverable, not a development artifact.
They are the thing someone runs to see whether this project is worth
their attention.

**What it should show, in order of how convincing it is:**

**Growth under fan-out.** Start one task that enqueues a hundred, each
of which enqueues a few more. Report the queue's capacity over time —
its starting size, every doubling, and its high-water mark. This is the
number that proves the ring never overflowed and never had to stop the
world to avoid it.

**Termination with a long tail.** A chain where each task enqueues its
successor, several such chains of wildly different lengths running at
once. Report the wall-clock moment the last task finished and the
moment the pool declared itself done. The gap between them is the cost
of the termination check, and it should be invisible.

**Idle cost.** Workers sleeping while a slow trickle of tasks arrives.
Report processor time consumed against wall-clock time elapsed. A
spinning pool shows one core per idle worker; a sleeping one shows
close to nothing.

**Throughput against worker count.** The same total work run with one
worker, then two, then as many as the machine has. Report tasks per
second for each. This is where the single global queue mutex will show
its ceiling, and knowing where that ceiling is now is worth more than
guessing about it later.

## Suggested implementation steps

1. Write the demo as a C program driven by a small shell script, both
   in `issues/completed/demos/`, named so the root launcher finds it.
2. Have it write its numbers to `tmp/shared-memory/` as well as the
   screen, so a run can be compared against an earlier one.
3. Report every number by measuring it. Nothing in the output should be
   a constant written into the demo's source.
4. Check the launcher script in the project root picks it up.

## Related

- [006 — Scheduling](../docs/006-datapath-scheduling.md)
- Issues 101 through 104 — everything being demonstrated
