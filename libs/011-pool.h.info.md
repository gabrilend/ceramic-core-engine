# 011-pool.h — the thread pool, from outside

The pool is a box you drop work into. A fixed set of threads takes the
work out and runs it, sleeps when there is none, and shuts the whole
thing down the moment it can prove nothing more will ever arrive.
Treat everything here as a black box: what goes in, what comes out.

## Data structures

**task** — one unit of work.
| field | type | meaning |
|---|---|---|
| call | function pointer, `void (*)(task_t *)` | The function a worker will invoke, handed the task itself. |

The pool only ever reads `call`. Creators may allocate a larger struct
whose first member is a task and hang any payload after it; the worker
frees the whole allocation with `free` after running it, so tasks must
come from `malloc`.

**pool** — opaque. All fields private to the implementation.

## Functions

**pool_create(worker count, finish hook, hook context) → pool**
- worker count: `int`. Positive is obeyed; zero or below falls back to
  the `SORAMECH_WORKERS` environment variable, then to one per online
  processor.
- finish hook: function pointer `void (*)(void *, task_t *)` or null.
  Called by the worker after a task runs, before it is freed. This is
  where delivery (phase 2) plugs in. Null means "run and free".
- Workers are spawned immediately but parked; nothing runs until
  release. Returns the pool, or aborts loudly — there is no error
  return, because a half-created pool is not a thing to hand back.

**pool_push(pool, task)** — enqueue one malloc-allocated task. Grows
the ring if full (bounded memory copy), wakes all sleepers. Never
blocks on user code.

**pool_pop(pool) → task or null** — take the oldest task, or null when
empty. Never blocks. Workers use their own internal path; this is for
code that owns the pool, chiefly tests.

**pool_release(pool)** — open the starting gate. Called once, after
any seeding.

**pool_join(pool)** — wait until the pool has terminated itself: every
task run, every worker returned. Idempotent; a second call returns at
once.

**pool_destroy(pool)** — join if not already joined, then free
everything. Destroying a never-released pool stops the parked workers
where they stand. Complains to stderr if tasks were left unrun.

**pool_submitter_register(pool)** / **pool_submitter_unregister(pool)**
— a standing promise that the calling context may still push. While
any registration is held the pool will idle rather than terminate.
Anything outside the workers that pushes after release must hold one,
or the pool may declare itself finished between two pushes.

**pool_worker_index() → int** — the calling thread's worker index, or
-1 off-pool. Thread-local read; free.

**pool_worker_count(pool) → int** — the actual worker count after
defaulting.

**pool_queue_stats(pool, out capacity, out high water, out growths)**
— the queue's measurements, for demos and diagnostics. Any out
pointer may be null.

## Stopping on purpose (issue 106)

**pool_signal_when_finished(pool, signal)** — raise this signal, once,
when the pool decides by itself that the work has run out. The thread
that wants to know a program is over is usually also waiting to be
*told* to stop, and one waiting point woken for two reasons is simpler
than two waits that have to be combined; so the end of the work
arrives as one more signal, told apart by its number.

It is raised at the process rather than at a thread, which needs no
thread identity recorded and lands wherever somebody is waiting.
**Asking after the fact still gets an answer**: if the pool has
already finished, the signal is raised at the moment of asking. A
short program can run out of work between being released and anybody
sitting down to wait, and without that a waiter would wait forever.

Opt-in, and it has to be: most signals kill a process by default, so a
pool that raised one unasked would end every program that did not
expect it.

**pool_stop(pool)** — stop starting new things, which is what halting
honestly means here. A worker inside a box finishes that box, because
there is no safe way to interrupt executing C; a worker looking for
work finds the flag and returns. Whatever is queued stays queued and
is never run, which tearing down afterwards reports rather than hides.

**pool_queued(pool) → int** — how many tasks are waiting right now.

**pool_worker_station(pool, worker) → int** — which station that
worker is inside, or -1. A number the pool ferries and never
interprets, like the one on the task it came from — and **a number
rather than the task's address** on purpose. A report written while a
lock is held by something that will never release it must not
dereference anything: a stale integer is a wrong answer, while a stale
pointer is a crash inside the thing that exists to explain a crash.
