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
The thread pool is the 3d-rts pool library at
`/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h`
(and its `.c` companion). It is vendored into SoraMech — copied, not
symlinked — so SoraMech's compilation does not depend on the 3d-rts
working tree. Vendored copy lives at `libs/task-pool/900-task-pool.h`
and `libs/task-pool/900-task-pool.c`.

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
    // 4. pool = pool_create(n_workers)
    // 5. init per-worker language runtime handles for each language present
    // 6. submit tasks for all entry-point boxes
    // 7. wait for active task count to reach zero
    // 8. pool_destroy(pool)
    // 9. write last-run.json
}
```

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

### Slots are owned by tasks (issue 302)
Slots are allocated when a task is created, not pre-allocated at
startup. A task allocates its output slot(s) at submission time, with
size specified by the task's language and function. The pool runner
does not own a slot region — slots are per-task allocations that
outlive the task only as long as wires hold references to them. See
issue 302 for the slot lifecycle.

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
- `/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h` — pool API
- `src/003-loader.lua` — current Lua loader, to be ported to C
- `src/004-executor.lua` — current synchronous executor, to be replaced
- `src/007-runner-main.lua` — current Lua runner, to be replaced
- `issues/302-wire-value-slot-store.md` — per-task slot lifecycle
- `issues/303-language-runtime-spec.md` — per-language init/call/teardown
- `issues/304-task-dispatch-layer.md` — replaces 004-executor.lua
- `docs/004-ipc-and-threading.md` — threading roadmap and IPC options
