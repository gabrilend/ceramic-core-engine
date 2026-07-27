# 103 — Sleeping and waking

## Current behavior

Built. A worker that finds the queue empty registers itself asleep and
waits on a condition variable; a push wakes every sleeper and whoever
arrives first takes the task. The sleeping count is exact because the
check, the registration, and the wait all happen inside one hold of
the queue's own mutex — the wait releases that mutex atomically, so
the window this issue warned about (checked-empty but not yet
registered) does not exist in this lock discipline. Wakes are treated
as rumors: a woken worker deregisters and re-checks the queue before
believing anything. Proven by a trickle test: four workers fed twenty
tasks across four-fifths of a second consumed three milliseconds of
processor time — sleeping, not spinning — and every task still ran.

## Intended behavior

A worker with nothing to do sleeps. A worker that puts something into
the queue wakes everyone.

**Sleep rather than spin.** The cost of sleeping is a trip into the
kernel and back, which is thousands of times more expensive than the
worst possible mispredicted branch — but it is paid only when there is
nothing to do anyway, and the alternative is a core held at full
occupancy producing nothing. No arithmetic trick avoids this; the cost
is the context switch, not a decision.

**Wake everyone, not a chosen one.** Picking a specific worker to wake
means reasoning about which one, and there is no information available
to make that choice better than arbitrary. Waking all of them lets
whoever gets there first take the task and the rest go back to sleep.

**A worker that has just enqueued does not sleep**, because the queue
is not empty — it just put something in it.

**A count of sleeping workers** is maintained, and it is the piece
issue 104 depends on entirely. It must be exact, which means it is
maintained under the same lock that guards the decision to sleep. A
count kept separately from the sleeping decision has a window between
"I checked and found nothing" and "I am now registered as asleep," and
that window is where issue 104's failure lives.

## Suggested implementation steps

1. Add a condition variable alongside the queue's existing mutex, plus
   an integer count of sleeping workers guarded by that same mutex.
2. Replace the run loop's empty case: while holding the mutex, confirm
   the queue is empty, increment the sleeping count, and wait. The wait
   must be the kind that releases the mutex and sleeps as one
   indivisible action, or the window described above reopens.
3. On waking, decrement the count and re-check the queue before
   assuming there is work — a wake can be spurious, and several workers
   wake for one task.
4. In push, after the task is in the queue, wake every sleeper.
5. A test with several workers and a slow trickle of tasks, asserting
   that idle workers consume no measurable processor time and that
   every task still runs.

## Related

- [006 — Scheduling](../docs/006-datapath-scheduling.md)
- Issue 102 — the run loop this modifies
- Issue 104 — depends on the sleeping count being exact
