/* libs/task-pool/pool.h — SoraMech-owned thread pool, public API.
 *
 * What it is, in a sentence: N worker threads pulling tasks off a
 * single mutex-protected queue, with a one-shot init barrier and a
 * condition-variable-based quiescence wait.
 *
 * Designed in issue 301. Reference: the 3d-rts pool at
 * /home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h
 * (design only, not vendored).
 *
 * Lifecycle:
 *
 *   1. pool_create(n_workers)
 *        - spawns the worker threads
 *        - each worker sets its TLS context and parks at the init
 *          barrier (no init work to do yet; 303 plugs spec init in
 *          here later)
 *   2. pool_init_barrier(pool)
 *        - blocks until every worker has reached the barrier
 *        - releases them all simultaneously
 *        - one-shot; returns; barrier never engages again
 *   3. pool_spawn / pool_wait_quiescent — main loop of work
 *   4. pool_destroy(pool)
 *        - flips the shutdown flag, broadcasts the queue cv,
 *          joins every worker, frees the pool
 *
 * Per-worker context is reachable from inside any task action via
 * the `pool_current_worker` TLS pointer. The handle slots are
 * reserved for the spec registry (issue 303); ignore them for now.
 *
 * Thread safety: every public function is safe to call from any
 * thread. pool_spawn from inside an action is fine and is how
 * iterators and graph traversal will work.
 */

#ifndef SORAMECH_POOL_H
#define SORAMECH_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Types */
typedef struct pool pool_t;

/* A pool action takes one void* argument and returns nothing.
 * Errors are the action's business — the pool just dispatches. */
typedef void (*pool_action_t)(void *arg);

/* Per-worker context. Reserved for issue 303's per-language runtime
 * handles; the pool itself only uses thread_idx and pool. */
#define POOL_LANG_SLOTS 16

typedef struct worker_ctx {
    int     thread_idx;
    pool_t *pool;
    void   *handles[POOL_LANG_SLOTS];
} worker_ctx_t;

/* Set on every worker thread before the init barrier; readable from
 * any pool action. */
extern __thread worker_ctx_t *pool_current_worker;

#define POOL_MAX_WORKERS 16
/* }}} */

/* {{{ Lifecycle */
/* Create a pool with n_workers threads. If n_workers <= 0, defaults
 * to sysconf(_SC_NPROCESSORS_ONLN) capped at POOL_MAX_WORKERS. The
 * SORAMECH_WORKERS environment variable, if set and >0, overrides
 * the default. Returns NULL on allocation or pthread_create failure. */
pool_t *pool_create(int n_workers);

/* Block until every worker has finished its (currently empty) init
 * sequence and is parked at the barrier; then release them all. The
 * 303 spec-registry plug-in point lives between pool_create and
 * pool_init_barrier. One-shot. */
void    pool_init_barrier(pool_t *p);

/* Stop accepting tasks, drain whatever's queued, join every worker,
 * free the pool. Safe on NULL. */
void    pool_destroy(pool_t *p);

int     pool_n_workers(const pool_t *p);
/* }}} */

/* {{{ Submission and wait */
/* Submit a task. Safe from any thread (including from inside another
 * action). Increments the active-task counter; the worker that runs
 * the task decrements it on return. */
void    pool_spawn(pool_t *p, pool_action_t fn, void *arg);

/* Block until the active-task counter reaches zero. Calling this
 * before pool_init_barrier with pending tasks is a programming
 * error — workers haven't been released yet, so nothing will drain. */
void    pool_wait_quiescent(pool_t *p);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_POOL_H */
