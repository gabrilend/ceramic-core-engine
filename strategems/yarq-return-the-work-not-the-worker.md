# Strategem — when a worker can't make progress, return the work to the scheduler, not the worker

## The pattern

In a runtime where many small tasks compete for a small pool of
workers, synchronization primitives that *hold workers* (locks,
spinlocks, parked threads) silently flip the cost model. The
worker is the scarce resource; the scheduler is the abundant
coordinator. When a worker hits a contended boundary and can't
make progress, the cooperative move is to **hand the task back
to the scheduler** — not to camp on the worker until the
boundary clears.

YARQ — yield-and-requeue, the primitive that protects in-place
box mutation in issue 420 (formerly 320, renumbered when runtime
graph mutation moved to phase 4) — is the project's name for
this pattern in one specific place. The shape is general: any time a
worker realises "I can't finish this task right now," it should
re-submit the task to the pool queue and return its body to the
scheduler for the next ready unit of work.

The trap is that locks read as "the right tool" because they
work fine in the textbook setting where threads are heavyweight
and tasks are large. Spinlock or mutex, every contended worker
either burns CPU or parks; either way, the worker is committed
to that task until it can proceed. When tasks are tiny and
workers are few, that commitment is expensive — the worker
could have done five other things in the time it spent waiting.

Symptoms that suggest the wrong primitive:

- Many short tasks per second, few workers, occasional bursts
  of contention. The contended workers stall, throughput
  collapses for the duration of the burst, then recovers.
- Priority inversion: a low-priority task holds a lock, a
  high-priority task can't proceed, the scheduler's careful
  ordering is silently overridden by the lock contract.
- Workers parked on locks that other parked workers depend on
  — the deadlock-prevention dance grows around the locks
  because the locks themselves don't compose.

## How to apply

When you reach for a lock to protect shared mutable state:

- Ask: **is the worker the scarce resource, or is the lock
  contract genuinely needed?** If workers are cheap (one per
  CPU, OS threads, the system is throughput-bounded by CPU),
  a lock is fine. If workers are scarce (a fixed-size pool
  smaller than the task fanout, the system is latency-bounded
  by scheduler trips), prefer YARQ.
- Express acquire as a non-blocking CAS that **returns failure
  instead of waiting**. On failure, the worker re-submits the
  task to the scheduler (usually at the queue tail — see below)
  and exits the task body. No spin, no park, no waiting.
- Release is a plain atomic store. The contended worker's
  re-submission lands later by virtue of scheduler progress,
  not because the holder signalled.
- Pick the requeue end thoughtfully. **Tail** is fairer (other
  pending work runs first, the contended task doesn't
  immediately re-collide with the holder, the system as a
  whole keeps draining). **Head** is faster recovery for one
  task (it retries soonest) at the cost of unfairness to
  whatever else is queued. Default to tail; choose head only
  when latency on this one task dominates.
- Fold every mutation path that touches the shared state into
  the same YARQ barrier — not just the path you noticed first.
  If `connect` and `reconfigure` both mutate a box, they share
  YARQ. The barrier is "no concurrent mutator," not "no
  concurrent reconfigure."

## Adjacent pattern — async/await is YARQ at the language level

The async-await primitive in modern runtimes is the same shape
generalised: a coroutine returns `Pending` instead of blocking,
the executor reschedules the coroutine when its waker fires.
The worker (the executor thread) is freed for other coroutines
in the meantime. The pattern's name varies — green threads,
fibers, futures, tasks — but the core invariant is identical:
**when a unit of work can't proceed, hand the unit back, not
the worker.**

YARQ is the lower-level version: no waker, no future. The
contended task just re-submits itself and trusts the scheduler
to eventually try again. Coarser-grained than async-await, but
strictly simpler to implement and reason about — there's no
waker chain to keep coherent, no `Poll` state machine, just
"try, fail, re-submit."

## Why this happens

Lock-style primitives carry a hidden assumption: *workers are
interchangeable units of capacity, plentiful enough that
holding one is cheap.* That assumption is right for OS threads
on big servers and wrong for almost every other context — task
pools, async executors, embedded runtimes, the actor model. In
the wrong context, locks turn the scheduler's careful work
distribution into a coordination bottleneck.

The strategem is to notice which side of that assumption the
system actually lives on. If the scheduler is the load-bearing
piece — the thing making the system fast — then synchronization
should be **expressed in scheduler vocabulary**: re-submit,
yield, requeue. Locks express it in thread vocabulary: park,
spin, wake. Mixing the two means the lock's parked workers are
invisible to the scheduler, and the scheduler's careful
ordering is broken by waits the lock owns.

## Related issues

- Issue 420 — the box reconfigure path that introduces YARQ to
  this project (renumbered from 320 during the phase-4 split).
- Issue 421 — the unified connections-array mutation path; the
  comprehensive YARQ-folded design subsumes the old
  copy-and-publish atomic dance under the same barrier.
- Issue 301 / 304 — the pool runner and dispatch layer whose
  scheduler-vocabulary YARQ speaks. The fact that the pool
  already routes tasks by re-submission is what makes YARQ
  cheap to add — the machinery is there.
