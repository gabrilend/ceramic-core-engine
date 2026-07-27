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
