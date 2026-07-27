# 105 — Phase 1 demo: the pool under load

## Current behavior

The pool works and is covered by unit tests, but there is nothing that
shows it working to someone who is not reading test output.

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
