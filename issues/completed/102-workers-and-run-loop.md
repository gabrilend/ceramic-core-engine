# 102 — Worker threads and the run loop

## Current behavior

Built. A fixed set of threads is spawned at pool creation and parked
at a starting gate; a separate release call opens the gate, which is
the window where a map will later be seeded. Each worker loops: take
the oldest task, run its call, hand it to the finish hook (delivery's
reserved seat, null for now), free it, repeat. Thread count obeys the
creation argument, then the SORAMECH_WORKERS environment variable,
then one per online processor. Each worker knows its own index through
a thread-local read. The empty-queue case went straight to sleeping
rather than passing through the planned busy-return placeholder,
because the sleeping machinery was built in the same sitting — the
run loop, the sleep rule, and the termination rule are one function.
Proven by a test that twenty thousand counting tasks each run exactly
once, and that nothing runs before the gate opens.

## Intended behavior

A fixed set of threads, created when the pool is created and joined
when it is destroyed. Each one loops: take a task, run it, hand back
its result, free it, repeat.

**The worker knows nothing about boxes.** It moves opaque task structs
between threads. Everything about what a task *means* — which station
produced it, where its output goes — lives on the delivery path, which
does not exist yet and is not this issue's concern. In phase 1 a task
carries a function pointer and an argument, and the worker calls it.

**Thread count** is decided when the pool is created. Defaulting to the
number of online processors is the obvious choice, with an environment
variable able to override it, because tests want a single worker and
reproducing a race wants two.

**The run loop:**

1. Take a task from the queue.
2. If there was one, run it, then free it, then go back to step 1.
3. If there was not, sleep — which is issue 103.

**Creation and the starting line.** Every worker must be alive and
ready before the first task is allowed to run, or a task pushed during
startup gets picked up by a partially-constructed pool. A barrier at
pool creation, released once every worker has reached it, keeps the
starting line straight. It also leaves a place to hook per-worker
initialization later, should any be needed.

**A worker index** is worth giving each thread, readable from inside a
running task. It costs nothing and it is what makes per-worker
statistics possible in phase 7.

## Suggested implementation steps

1. Extend the pool with a worker array, a thread count, and per-worker
   context holding at minimum its own index and a pointer back to the
   pool.
2. Pool creation spawns the threads and parks them at the barrier;
   a separate call releases them, so anything that must happen between
   spawn and start has somewhere to go.
3. The run loop, with the empty-queue case left as a busy return for
   now — issue 103 replaces it with sleeping. Note in a comment that
   this is temporary, because a spinning worker that ships is a
   worker that burns a core forever.
4. Pool destruction: flip a shutdown flag, wake everything, join every
   thread, free.
5. A test that submits a known number of counting tasks and asserts
   that the total is right and every task ran exactly once.

## Related

- [006 — Scheduling](../docs/006-datapath-scheduling.md)
- Issue 101 — the queue this pops from
- Issue 103 — replaces the temporary busy return
