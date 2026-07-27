# 104 — Termination: the last worker to sleep looks again

## Current behavior

Workers sleep when there is nothing to do (issue 103) and wake when
something arrives. Nothing decides that the program is finished, so a
pool that runs out of work sleeps forever.

## Intended behavior

**The worker whose own registration brings the sleeping count up to the
worker total re-scans the queue before anyone actually sleeps.**

If it finds a task, it deregisters itself, wakes everyone, and goes to
work. If it finds nothing, the program is finished: a stop flag is set,
every worker is woken, each sees the flag and returns, and the main
thread joins them.

## Why the re-scan is not decoration

Without it there is a race whose outcome is a silent wrong answer
rather than a hang, which is strictly worse:

1. Worker A scans the queue, finds it empty, decides to sleep — and is
   preempted right there, before registering itself.
2. Worker B finishes a task, delivers, enqueues a new one, checks the
   sleeping count to decide whom to wake, sees zero, and wakes nobody.
3. Worker A resumes and registers itself asleep.

Every worker is now asleep, the count reads "all of them," and there is
a task sitting in the queue. A termination check that trusts the count
alone declares the program finished and exits cleanly, having silently
not done part of its work.

With the re-scan, worker A is the one whose registration completed the
count, so it looks again and finds B's task.

## Why the negative result is trustworthy

When the re-scan does come up empty, the conclusion is sound. The count
only reaches the worker total when every other worker has already
finished its task and found nothing — a worker mid-task is not counted
as sleeping. So there is nobody left who could enqueue anything, and
the emptiness is permanent rather than momentary.

The one thing this relies on: nothing outside the pool pushes tasks
after startup. The seed sweep in phase 6 pushes before the workers are
released, so it does not violate this. Anything added later that
submits from outside must either register itself in the count or the
rule stops holding.

## Shut down by broadcast, never by breaking out

A worker left parked in a wait that no longer receives signals turns
shutdown into a hang at the join. Set the flag, wake everyone, let each
one see the flag and return on its own.

## Suggested implementation steps

1. Add a stop flag to the pool, guarded by the queue mutex.
2. In the sleep path from issue 103, after incrementing the count and
   before waiting: if the count now equals the worker total, re-scan
   the queue.
3. Queue non-empty: decrement, wake everyone, continue the run loop.
   Queue empty: set the stop flag, wake everyone, return.
4. Every waking worker checks the stop flag before checking the queue,
   and returns if it is set.
5. A test that reproduces the race in step 1's description
   deliberately, by delaying a worker between its check and its
   registration. This is the most important test in phase 1, because
   the failure it guards against looks exactly like success.
6. A test that a pool given a chain of tasks — each one enqueuing the
   next — terminates only after the whole chain has run.

## Related

- [006 — Scheduling](../docs/006-datapath-scheduling.md)
- Issue 103 — the sleeping count this reads
- Issue 605 — the seed sweep, the one thing that pushes from outside
