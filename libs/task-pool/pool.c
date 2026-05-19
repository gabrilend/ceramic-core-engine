/* libs/task-pool/pool.c — SoraMech thread pool, implementation.
 *
 * Pthreads, a singly-linked FIFO task queue under one mutex, a
 * condition variable for "queue has work or we're shutting down",
 * an atomic active-task counter with its own mutex/cv for the
 * quiescence wait, and a one-shot init barrier under a third
 * mutex/cv.
 *
 * Designed in issue 301. The three sync primitives never nest —
 * pool_spawn takes the queue mutex, pool_wait_quiescent takes the
 * quiet mutex, pool_init_barrier takes the init mutex. No lock
 * ordering to worry about.
 */

#include "pool.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

__thread worker_ctx_t *pool_current_worker = NULL;

/* {{{ Internal task node — singly-linked FIFO */
typedef struct task_node {
    pool_action_t      fn;
    void              *arg;
    struct task_node  *next;
} task_node_t;
/* }}} */

/* {{{ Pool struct */
struct pool {
    int            n_workers;
    pthread_t     *threads;
    worker_ctx_t  *contexts;

    /* Per-worker init / teardown callbacks (issue 303 plumbing). */
    pool_init_cb_t      init_cb;
    void               *init_user;
    pool_teardown_cb_t  teardown_cb;
    void               *teardown_user;

    /* Task queue. */
    pthread_mutex_t  q_mtx;
    pthread_cond_t   q_cv;
    task_node_t     *q_head;
    task_node_t     *q_tail;

    /* Active-task counter and its quiescence wait. */
    _Atomic int      active;
    pthread_mutex_t  quiet_mtx;
    pthread_cond_t   quiet_cv;

    /* Shutdown flag. Set by pool_destroy; checked by workers. */
    _Atomic int      shutdown;

    /* Init barrier. Three phases, all protected by init_mtx and
     * woken via init_cv:
     *
     *   init_starting=0  workers are spawned but parked on the cv,
     *                    waiting for pool_init_barrier to authorise
     *                    them to run their init callbacks. The
     *                    callback can't be safely registered after
     *                    pool_create otherwise — workers would
     *                    race past the (then-NULL) callback before
     *                    pool_set_worker_init is called.
     *   init_starting=1  workers run their init callbacks, update
     *                    workers_ready / init_fail, and park on
     *                    init_released.
     *   init_released=1  workers proceed to the task loop. */
    int              init_starting;
    int              workers_ready;
    int              init_released;
    int              init_fail;
    pthread_mutex_t  init_mtx;
    pthread_cond_t   init_cv;
};
/* }}} */

/* {{{ default_n_workers() */
static int default_n_workers(void)
{
    const char *env = getenv("SORAMECH_WORKERS");
    if (env && env[0]) {
        int v = atoi(env);
        if (v > 0) return v > POOL_MAX_WORKERS ? POOL_MAX_WORKERS : v;
    }
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc < 1) nproc = 1;
    if (nproc > POOL_MAX_WORKERS) nproc = POOL_MAX_WORKERS;
    return (int)nproc;
}
/* }}} */

/* {{{ worker_main() — what each worker thread runs */
static void *worker_main(void *arg)
{
    worker_ctx_t *ctx = (worker_ctx_t *)arg;
    pool_t       *p   = ctx->pool;
    pool_current_worker = ctx;

    /* Stage 1: wait for pool_init_barrier to authorise init.
     * Without this, a callback registered between pool_create and
     * pool_init_barrier would race the workers running init with
     * a still-NULL p->init_cb. */
    pthread_mutex_lock(&p->init_mtx);
    while (!p->init_starting && !atomic_load(&p->shutdown)) {
        pthread_cond_wait(&p->init_cv, &p->init_mtx);
    }
    int shutting_down = atomic_load(&p->shutdown);
    pthread_mutex_unlock(&p->init_mtx);
    if (shutting_down) return NULL;

    /* Stage 2: per-worker init point. Issue 303's spec_registry
     * helper registers itself via pool_set_worker_init; this is
     * where it runs, populating ctx->handles[] before the worker
     * is allowed to pull tasks. */
    int cb_rc = 0;
    if (p->init_cb) cb_rc = p->init_cb(ctx->thread_idx, ctx, p->init_user);

    /* Stage 3: report ready (or failure) and park on init_released. */
    pthread_mutex_lock(&p->init_mtx);
    if (cb_rc != 0) p->init_fail = 1;
    p->workers_ready++;
    pthread_cond_broadcast(&p->init_cv);
    while (!p->init_released) {
        pthread_cond_wait(&p->init_cv, &p->init_mtx);
    }
    int fail = p->init_fail;
    pthread_mutex_unlock(&p->init_mtx);

    /* If anyone's init failed, every worker bails before touching
     * the task queue; the pool is unusable until destroyed. Run
     * the teardown so partially-initialised handles get cleaned
     * up symmetrically with init. */
    if (fail) {
        if (p->teardown_cb)
            p->teardown_cb(ctx->thread_idx, ctx, p->teardown_user);
        return NULL;
    }

    /* Task loop. */
    while (1) {
        pthread_mutex_lock(&p->q_mtx);
        while (p->q_head == NULL && !atomic_load_explicit(&p->shutdown, memory_order_acquire)) {
            pthread_cond_wait(&p->q_cv, &p->q_mtx);
        }
        /* Drain queue even after shutdown is signaled so already-
         * spawned tasks complete. */
        if (p->q_head == NULL) {
            pthread_mutex_unlock(&p->q_mtx);
            break;
        }
        task_node_t *t = p->q_head;
        p->q_head = t->next;
        if (p->q_head == NULL) p->q_tail = NULL;
        pthread_mutex_unlock(&p->q_mtx);

        t->fn(t->arg);
        free(t);

        /* Decrement active; on transition to zero, signal the
         * quiescence waiter. The lock is taken under the
         * pre-check-then-recheck pattern so the waiter doesn't miss
         * the signal even if multiple workers reach zero
         * concurrently. */
        int prev = atomic_fetch_sub_explicit(&p->active, 1, memory_order_acq_rel);
        if (prev == 1) {
            pthread_mutex_lock(&p->quiet_mtx);
            pthread_cond_broadcast(&p->quiet_cv);
            pthread_mutex_unlock(&p->quiet_mtx);
        }
    }

    /* Worker is exiting. Run the per-worker teardown so language
     * specs (issue 303) can release their handles symmetrically
     * with init. */
    if (p->teardown_cb) {
        p->teardown_cb(ctx->thread_idx, ctx, p->teardown_user);
    }
    return NULL;
}
/* }}} */

/* {{{ pool_create() */
pool_t *pool_create(int n_workers)
{
    if (n_workers <= 0) n_workers = default_n_workers();
    if (n_workers > POOL_MAX_WORKERS) n_workers = POOL_MAX_WORKERS;

    pool_t *p = calloc(1, sizeof *p);
    if (!p) return NULL;

    p->n_workers = n_workers;
    p->threads   = calloc((size_t)n_workers, sizeof(pthread_t));
    p->contexts  = calloc((size_t)n_workers, sizeof(worker_ctx_t));
    if (!p->threads || !p->contexts) goto fail;

    if (pthread_mutex_init(&p->q_mtx,     NULL) != 0) goto fail;
    if (pthread_cond_init (&p->q_cv,      NULL) != 0) goto fail;
    if (pthread_mutex_init(&p->quiet_mtx, NULL) != 0) goto fail;
    if (pthread_cond_init (&p->quiet_cv,  NULL) != 0) goto fail;
    if (pthread_mutex_init(&p->init_mtx,  NULL) != 0) goto fail;
    if (pthread_cond_init (&p->init_cv,   NULL) != 0) goto fail;

    atomic_init(&p->active,   0);
    atomic_init(&p->shutdown, 0);

    for (int i = 0; i < n_workers; i++) {
        p->contexts[i].thread_idx = i;
        p->contexts[i].pool       = p;
        if (pthread_create(&p->threads[i], NULL, worker_main, &p->contexts[i]) != 0) {
            /* Tear down: flip shutdown, release any already-started
             * workers from the barrier so they can exit. */
            atomic_store(&p->shutdown, 1);
            pthread_mutex_lock(&p->init_mtx);
            p->init_released = 1;
            pthread_cond_broadcast(&p->init_cv);
            pthread_mutex_unlock(&p->init_mtx);
            pthread_mutex_lock(&p->q_mtx);
            pthread_cond_broadcast(&p->q_cv);
            pthread_mutex_unlock(&p->q_mtx);
            for (int j = 0; j < i; j++) pthread_join(p->threads[j], NULL);
            goto fail;
        }
    }
    return p;

fail:
    if (p) {
        free(p->threads);
        free(p->contexts);
        free(p);
    }
    return NULL;
}
/* }}} */

/* {{{ pool_set_worker_init() */
void pool_set_worker_init(pool_t *p, pool_init_cb_t cb, void *user)
{
    if (!p) return;
    p->init_cb   = cb;
    p->init_user = user;
}
/* }}} */

/* {{{ pool_set_worker_teardown() */
void pool_set_worker_teardown(pool_t *p, pool_teardown_cb_t cb, void *user)
{
    if (!p) return;
    p->teardown_cb   = cb;
    p->teardown_user = user;
}
/* }}} */

/* {{{ pool_init_barrier() */
int pool_init_barrier(pool_t *p)
{
    if (!p) return -1;
    pthread_mutex_lock(&p->init_mtx);
    /* Stage 1: authorise workers to run their init callbacks. */
    p->init_starting = 1;
    pthread_cond_broadcast(&p->init_cv);
    /* Stage 2: wait for all workers to report ready. */
    while (p->workers_ready < p->n_workers) {
        pthread_cond_wait(&p->init_cv, &p->init_mtx);
    }
    int fail = p->init_fail;
    /* Stage 3: release everyone into the task loop. */
    p->init_released = 1;
    pthread_cond_broadcast(&p->init_cv);
    pthread_mutex_unlock(&p->init_mtx);
    return fail ? -1 : 0;
}
/* }}} */

/* {{{ pool_spawn() */
void pool_spawn(pool_t *p, pool_action_t fn, void *arg)
{
    if (!p || !fn) return;
    task_node_t *t = malloc(sizeof *t);
    if (!t) return;        /* OOM: silently drop; revisit error path */
    t->fn   = fn;
    t->arg  = arg;
    t->next = NULL;

    /* Increment active BEFORE enqueueing so the pop+decrement on the
     * worker side never sees active drop below the pending-work
     * count. */
    atomic_fetch_add_explicit(&p->active, 1, memory_order_acq_rel);

    pthread_mutex_lock(&p->q_mtx);
    if (p->q_tail) p->q_tail->next = t;
    else           p->q_head       = t;
    p->q_tail = t;
    pthread_cond_signal(&p->q_cv);
    pthread_mutex_unlock(&p->q_mtx);
}
/* }}} */

/* {{{ pool_wait_quiescent() */
void pool_wait_quiescent(pool_t *p)
{
    if (!p) return;
    pthread_mutex_lock(&p->quiet_mtx);
    while (atomic_load_explicit(&p->active, memory_order_acquire) > 0) {
        pthread_cond_wait(&p->quiet_cv, &p->quiet_mtx);
    }
    pthread_mutex_unlock(&p->quiet_mtx);
}
/* }}} */

/* {{{ pool_n_workers() */
int pool_n_workers(const pool_t *p)
{
    return p ? p->n_workers : 0;
}
/* }}} */

/* {{{ pool_destroy() */
void pool_destroy(pool_t *p)
{
    if (!p) return;

    /* Release every stage of the init barrier in case
     * pool_init_barrier was never called (e.g. caller hit an error
     * between pool_create and pool_init_barrier). Setting shutdown
     * first means workers parked on stage 1 will see it and exit
     * cleanly without running their init callback. */
    atomic_store_explicit(&p->shutdown, 1, memory_order_release);

    pthread_mutex_lock(&p->init_mtx);
    if (!p->init_released) {
        p->init_starting = 1;
        p->init_released = 1;
        pthread_cond_broadcast(&p->init_cv);
    }
    pthread_mutex_unlock(&p->init_mtx);

    pthread_mutex_lock(&p->q_mtx);
    pthread_cond_broadcast(&p->q_cv);
    pthread_mutex_unlock(&p->q_mtx);

    for (int i = 0; i < p->n_workers; i++) {
        pthread_join(p->threads[i], NULL);
    }

    pthread_mutex_destroy(&p->q_mtx);
    pthread_cond_destroy (&p->q_cv);
    pthread_mutex_destroy(&p->quiet_mtx);
    pthread_cond_destroy (&p->quiet_cv);
    pthread_mutex_destroy(&p->init_mtx);
    pthread_cond_destroy (&p->init_cv);

    free(p->threads);
    free(p->contexts);
    free(p);
}
/* }}} */
