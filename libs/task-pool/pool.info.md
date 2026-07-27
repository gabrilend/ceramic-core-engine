# libs/task-pool/pool.c — public surface

SoraMech-owned thread pool. N pthread workers pulling tasks off a
single FIFO queue. Designed in issue 301. The 3d-rts pool at
`/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h`
was the design reference; nothing was vendored.

## Lifecycle

- `pool_t *pool_create(int n_workers)` — spawns workers and parks
  them at the init barrier. `n_workers <= 0` uses
  `sysconf(_SC_NPROCESSORS_ONLN)` capped at `POOL_MAX_WORKERS`
  (16). `SORAMECH_WORKERS=N` env var overrides. NULL on
  allocation / pthread failure.
- `int pool_init_barrier(pool_t *p)` — blocks until every worker
  reaches the barrier, then releases them all simultaneously.
  One-shot. Returns nonzero if any worker's init callback failed.
- `void pool_destroy(pool_t *p)` — flips shutdown, drains the
  queue, joins every worker, frees.

## Per-worker init and teardown

- `void pool_set_worker_init(pool_t *p, pool_init_cb_t cb, void *user)`
- `void pool_set_worker_teardown(pool_t *p, pool_teardown_cb_t cb, void *user)`

Register these between `pool_create` and `pool_init_barrier`. Each
worker runs the init callback before the barrier releases, so
nothing dispatches until every worker is fully set up. This is the
hook the spec registry uses to give each worker its own
interpreter state.

## Submission

- `void pool_spawn(pool_t *p, pool_action_t fn, void *arg, int priority)`
  — safe from any thread, including from inside another action.
  The priority argument is accepted and ordered on; see the note
  below about what that currently buys you.
- `void pool_wait_quiescent(pool_t *p)` — blocks until the
  active-task counter reaches zero.
- `int  pool_n_workers(const pool_t *p)` — count.

## Worker context

- `extern __thread worker_ctx_t *pool_current_worker` — set on
  every worker before the barrier; readable from any action.
- `worker_ctx_t.thread_idx` — 0-based worker index.
- `worker_ctx_t.pool` — back-pointer.
- `worker_ctx_t.handles[POOL_LANG_SLOTS]` — reserved for the
  spec registry (issue 303).

## Concurrency model

- One mutex for the FIFO queue (`q_mtx`), one CV (`q_cv`) for
  workers waiting on work.
- Atomic active-task counter; one mutex/CV pair (`quiet_mtx`,
  `quiet_cv`) for the quiescence waiter.
- One mutex/CV pair (`init_mtx`, `init_cv`) for the init barrier.
- The three pairs never nest. Lock-ordering is moot.

## What's NOT here (deferred)

- **Priority that changes anything.** The queue orders by the
  `priority` argument and the API is fully wired, but every
  caller passes the same value, so behaviour is FIFO. Issue 310
  shipped the mechanism; nothing yet distinguishes task
  priorities. Passing a distinct priority today will order tasks
  — there is simply no policy that does so.
- **Work stealing.** One shared FIFO, one mutex. Under many
  workers this queue is the contention point, not the slots.
- **Frame-ring scheduling / park-on-slot**: the 3d-rts pool's
  design has these; we don't need them for the dispatch model
  (issue 304), which always spawns tasks when inputs are ready
  rather than parking on slot wait lists.

## Related

- Issue 301 — design.
- Issue 302 — slot store, the thing tasks read and write through.
- Issue 303 — spec registry; per-worker init hook.
- Issue 304 — dispatch action, the function `pool_spawn`'d on
  every task.
- Issue 310 — priority queue (open).
