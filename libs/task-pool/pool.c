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

    /* Init barrier. workers_ready is incremented by each worker
     * after its (currently empty) per-thread setup; main thread
     * waits for workers_ready == n_workers, then flips init_released
     * and broadcasts. */
    int              workers_ready;
    int              init_released;
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

    /* Per-worker init point — issue 303 plugs spec->init calls here
     * for every language the map uses. Empty for now. */

    /* Increment workers_ready, notify the main thread, then park at
     * the barrier until init_released flips. */
    pthread_mutex_lock(&p->init_mtx);
    p->workers_ready++;
    pthread_cond_broadcast(&p->init_cv);
    while (!p->init_released) {
        pthread_cond_wait(&p->init_cv, &p->init_mtx);
    }
    pthread_mutex_unlock(&p->init_mtx);

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

/* {{{ pool_init_barrier() */
void pool_init_barrier(pool_t *p)
{
    if (!p) return;
    pthread_mutex_lock(&p->init_mtx);
    while (p->workers_ready < p->n_workers) {
        pthread_cond_wait(&p->init_cv, &p->init_mtx);
    }
    p->init_released = 1;
    pthread_cond_broadcast(&p->init_cv);
    pthread_mutex_unlock(&p->init_mtx);
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

    /* If pool_init_barrier was never called (e.g. error before
     * use), release workers from the barrier so they can exit. */
    pthread_mutex_lock(&p->init_mtx);
    if (!p->init_released) {
        p->init_released = 1;
        pthread_cond_broadcast(&p->init_cv);
    }
    pthread_mutex_unlock(&p->init_mtx);

    atomic_store_explicit(&p->shutdown, 1, memory_order_release);

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
