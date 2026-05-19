# 301 — Thread pool lifecycle and per-worker initialization

## Status
open

## Current behavior
The executor (`src/004-executor.lua`) runs boxes synchronously in a
single Lua coroutine. There is no thread pool, no pthreads, and no
per-thread language runtime state.

## Phase model

Phase 2 is the synchronous interpreter path. It is the development
target while editor work is in progress.

Phase 3 is the thread pool path. It is the only execution path going
forward. The synchronous interpreter does not stay around as an
alternative entry point — once phase 3 lands, the pool runner
replaces it. There is no "fallback to synchronous" mode.

### Phase 2 → 3 refactor surface

The following components are replaced (not extended) in phase 3:

| Phase 2 file                  | Phase 3 replacement                  |
|-------------------------------|--------------------------------------|
| `src/003-loader.lua`          | C loader inside `src/008-pool-runner.c` |
| `src/004-executor.lua`        | C task dispatch layer (issue 304)    |
| `src/007-runner-main.lua`     | `src/008-pool-runner.c`              |
| `drivers/*.sh`                | Language runtime spec (issue 303)    |

All threading-pool-adjacent functionality is C code that lives next to
the pool. There is no embedded Lua runtime for graph loading or
dispatch — Lua appears only as a language runtime that boxes can use,
on equal footing with C, Bash, and any future language.

## Intended behavior

### Pool source
The thread pool is **SoraMech-owned**, written from scratch under
`libs/task-pool/`. The 3d-rts pool at
`/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h`
is the **design reference**, not a dependency. We need full control
over the spawn primitive, the worker context, the init barrier
(below), and iterator-pinning extensions — all reasons not to inherit
an external implementation.

The pool exposes (at minimum):

```c
pool_t *pool_create(int n_workers);
void    pool_destroy(pool_t *p);
void    pool_spawn(pool_t *p, action_fn_t fn, void *arg);
void    pool_wait_quiescent(pool_t *p);   // blocks until active count hits zero
void    pool_init_barrier(pool_t *p);     // see "Init barrier" below
```

3d-rts's frame-ring + park-on-slot machinery is referenced for
patterns; we don't reimplement features we don't need.

### Where the pool lives in the process
The pool is owned by the map runner's C entry point. One pool per map
run. It is created after the graph is loaded and validated, and
destroyed after the run completes.

The C entry point (`src/008-pool-runner.c`):

```c
int main(int argc, char **argv) {
    // 1. parse map path from argv
    // 2. load and validate graph (C loader, no Lua involved)
    // 3. determine which language runtimes the map uses
    // 4. allocate per-input-port slots for every box (issue 302)
    // 5. pool = pool_create(n_workers)
    // 6. init per-worker language runtime handles for each language present
    // 7. pool_init_barrier(pool)  ← block until every worker is fully initialized
    // 8. push every input port's literal value into its slot
    // 9. spawn tasks for boxes whose input set is now satisfied
    // 10. pool_wait_quiescent(pool)
    // 11. write last-run.jsonl
    // 12. pool_destroy(pool)
}
```

### Init barrier

Workers are not safe to dispatch tasks to until **every worker has
fully completed init for every language spec the map uses**. If the
main thread spawns a task before a worker's `lua_State` is up, the
task lands on a null handle.

`pool_init_barrier` is the join point:

1. Each worker's startup runs all `lang->init(worker_idx)` calls in
   a defined order, populating its `worker_ctx_t.handles[]`.
2. After completing its inits, each worker increments a shared
   atomic `workers_ready` counter and blocks on a condition
   variable.
3. The main thread waits until `workers_ready == n_workers`.
4. Main thread broadcasts the condition variable; all workers
   release simultaneously and begin pulling tasks.

This is a one-shot barrier — it runs once at pool startup. After
quiescence it doesn't re-engage; teardown takes a different path.

The barrier also protects against partial init failures: if any
worker's `init` returns an error, the barrier never completes and
the main thread can detect it via a timeout or per-worker status
flag.

### Graph loading: ported to C
The graph loader is ported from `src/003-loader.lua` to C and lives
inside (or alongside) `008-pool-runner.c`. The pool runner has no
embedded Lua state for loader purposes. JSON parsing uses a small
vendored C JSON library (cJSON or similar; decision deferred to
implementation).

Lua appears in the runner only when a box's language is Lua — and even
then, only inside that worker's language runtime handle. The pool
runner itself does not link to liblua at the top level.

### Per-worker language runtimes
No language is treated as the "native" or preferred language at the
thread level. Every box — regardless of language — goes through the
same task dispatch path. The per-box cost is determined by that box's
invocation wrapper, not by any map-level language setting.

Each worker thread holds a set of language runtime handles, one per
language it may be asked to run. These are initialized once at pool
startup, before any tasks are submitted, and torn down at shutdown:

```c
typedef struct {
    void   *handles[N_LANGS];   // opaque per-language runtime state
    int     thread_idx;
} worker_ctx_t;
```

Only the language handles actually needed by the map are initialized.
If the map contains no Bash boxes, no Bash handle is started for any
worker. The graph loader determines which languages are present and
passes that set to the pool runner at startup.

The per-language initialization protocol is defined by the language
runtime spec (issue 303). The pool runner calls each spec's `init`
function at startup and its `teardown` function at shutdown.

### Worker context access
Worker context is stored in thread-local storage (`__thread
worker_ctx_t *current_worker`), set by each worker immediately after
the thread is started. This avoids per-task `pthread_self()` lookups.

### Pool size
Number of worker threads defaults to the number of logical CPUs
(`sysconf(_SC_NPROCESSORS_ONLN)`), capped at a compile-time maximum
(`MAX_WORKERS`, initially 16). Can be overridden by an environment
variable `SORAMECH_WORKERS=N`.

### Slots are owned by input ports (issue 302)
Each box has one ring-buffer slot per input port, allocated at
graph load time and durable for the run. The pool runner walks
every box during the load step and populates a per-box slot table.
Slots are not allocated per-task; tasks pop and push but never
allocate. See issue 302 for the slot lifecycle and lifetime model.

### Quiescence
The runner tracks an active-task counter, incremented on `pool_spawn`
and decremented when a task reaches `ACT_DONE`. The main thread waits
on a condition variable that signals when the counter reaches zero.

This naturally handles iterators that re-queue themselves: each
re-queue increments the counter, each completion decrements. When the
counter reaches zero, the run is over.

Deadlock is not a concern at runtime. The graph validator (running at
load) rejects non-iterator cycles and inputs without producers. Errors
inside a task crash the program (issue 303), so there is no
half-completed-producer case to worry about. An iterator parking
because its upstream queue is empty is not deadlock — it is the
natural end-of-stream condition; once the active-task counter reaches
zero with the iterator parked, the run ends cleanly.

## Open questions
- Vendored JSON library: cJSON, jsmn, or hand-rolled. cJSON is heavier
  but easiest. Decision deferred.

## Suggested implementation sequence
1. Vendor `900-task-pool.h` + `.c` into `libs/task-pool/`.
2. Vendor a JSON parser into `libs/json/`.
3. Port `003-loader.lua` to C inside `src/008-pool-runner.c`.
4. Write the pool lifecycle skeleton: pool_create, worker context TLS,
   active-task counter, quiescence wait.
5. Add `Makefile` target `soramech-pool` that compiles the C runner and
   links against the vendored pool and JSON parser.
6. Smoke test: single-box Lua map, pool with 2 workers, box runs, result
   written. No task graph complexity yet.

## Relevant files
- `libs/task-pool/` — SoraMech-built pool (to be created)
- `/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h` — design reference only
- `src/003-loader.lua` — current Lua loader, to be ported to C
- `src/004-executor.lua` — current synchronous executor, to be replaced
- `src/007-runner-main.lua` — current Lua runner, to be replaced
- `issues/302-wire-value-slot-store.md` — per-port slot model
- `issues/303-language-runtime-spec.md` — per-language init/call/teardown
- `issues/304-task-dispatch-layer.md` — replaces 004-executor.lua
- `docs/004-ipc-and-threading.md` — threading roadmap and IPC options

## Implementation log

### Pool skeleton — 2026-05-12

What shipped:
- `libs/task-pool/pool.h` + `pool.c` — SoraMech-owned pool with N
  pthread workers, a singly-linked FIFO queue under one mutex,
  atomic active-task counter with its own cv for quiescence wait,
  one-shot init barrier with workers_ready / init_released
  predicates under a third mutex. Worker count defaults to logical
  CPUs (capped at `POOL_MAX_WORKERS=16`), overridable via
  `SORAMECH_WORKERS=N`. Recursive `pool_spawn` from inside an
  action works — the queue mutex is re-entered cleanly.
- TLS `pool_current_worker` for actions that need their worker
  index or the back-pointer to the pool.
- `tests/301-pool-test.c` — 8 unit tests: create/destroy with no
  tasks, default worker count, cap-at-max, single spawn, 1000-task
  spawn-and-quiesce, TLS context observable from inside actions,
  recursive spawn (depth 10 → 11 invocations), and 8 concurrent
  producer threads each pushing 500 tasks while 4 workers drain
  (4000 total). All pass under STRICT.
- `src/008-pool-runner.c` now loads the graph via 305 and stands up
  the pool: `./soramech-pool tests/maps/hello` prints the box count,
  entry box, and worker count, then quiesces cleanly with no tasks.

What's deferred to follow-ons within 301:
- **Per-worker spec init** between `pool_create` and
  `pool_init_barrier`. The hook is documented in `worker_main`;
  lands with issue 303 once specs can be `dlopen`ed and
  per-worker handles allocated.
- **Priority queue.** Issue 310 covers the "wired in, no-op
  behaviorally" upgrade. Current queue is plain FIFO.

Neither blocks 304 (dispatch) from starting.

### Per-worker teardown hook — 2026-05-12

Symmetric counterpart to `pool_set_worker_init`. Registered via
`pool_set_worker_teardown(p, cb, user)`; the worker thread runs
the callback right before exiting, after the task loop drains.
The runner uses it to call `spec_registry_teardown_worker`,
which sends bash subprocesses `QUIT`, closes their socket fds,
and waitpids them. Without this, soramech-pool was leaking
orphan bash processes (16 per run) that piled up across runs
and slowed startup. Verified: zero orphans after a fresh run.
