/* tests/301-pool-test.c — unit tests for the thread pool.
 *
 * Covers the lifecycle (create / init barrier / destroy), single-
 * and many-task spawn, quiescence, the TLS worker context, recursive
 * spawn from inside an action, and a stress race that hammers the
 * queue mutex from multiple producers.
 *
 * Designed in issue 301.
 */

#include "pool.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Test harness */
static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "      %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 0; \
    } } while (0)

#define RUN(name) \
    do { \
        fprintf(stdout, "  %-44s ", #name); fflush(stdout); \
        if (test_##name()) { fprintf(stdout, "ok\n"); g_pass++; } \
        else                { fprintf(stdout, "FAIL\n"); g_fail++; } \
    } while (0)
/* }}} */

/* {{{ test_create_destroy_no_tasks() */
static int test_create_destroy_no_tasks(void)
{
    pool_t *p = pool_create(4);
    ASSERT(p);
    ASSERT(pool_n_workers(p) == 4);
    pool_init_barrier(p);
    pool_wait_quiescent(p);   /* immediate — no tasks */
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_default_n_workers() */
static int test_default_n_workers(void)
{
    pool_t *p = pool_create(0);
    ASSERT(p);
    /* 1..POOL_MAX_WORKERS depending on machine. */
    ASSERT(pool_n_workers(p) >= 1);
    ASSERT(pool_n_workers(p) <= POOL_MAX_WORKERS);
    pool_init_barrier(p);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_cap_at_max_workers() */
static int test_cap_at_max_workers(void)
{
    pool_t *p = pool_create(POOL_MAX_WORKERS + 50);
    ASSERT(p);
    ASSERT(pool_n_workers(p) == POOL_MAX_WORKERS);
    pool_init_barrier(p);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_spawn_one() */
static atomic_int g_count;
static void inc_action(void *arg) { (void)arg; atomic_fetch_add(&g_count, 1); }

static int test_spawn_one(void)
{
    atomic_store(&g_count, 0);
    pool_t *p = pool_create(2);
    pool_init_barrier(p);
    pool_spawn(p, inc_action, NULL);
    pool_wait_quiescent(p);
    ASSERT(atomic_load(&g_count) == 1);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_spawn_many() */
static int test_spawn_many(void)
{
    enum { N = 1000 };
    atomic_store(&g_count, 0);
    pool_t *p = pool_create(4);
    pool_init_barrier(p);
    for (int i = 0; i < N; i++) pool_spawn(p, inc_action, NULL);
    pool_wait_quiescent(p);
    ASSERT(atomic_load(&g_count) == N);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_current_worker_tls() */
static atomic_int g_max_idx;
static void check_worker_action(void *arg)
{
    (void)arg;
    worker_ctx_t *ctx = pool_current_worker;
    if (!ctx) return;
    int idx = ctx->thread_idx;
    int prev = atomic_load(&g_max_idx);
    while (idx > prev && !atomic_compare_exchange_weak(&g_max_idx, &prev, idx)) {
        /* retry */
    }
}

static int test_current_worker_tls(void)
{
    atomic_store(&g_max_idx, -1);
    pool_t *p = pool_create(4);
    pool_init_barrier(p);
    /* Spawn enough tasks that every worker likely picks one up. */
    for (int i = 0; i < 256; i++) pool_spawn(p, check_worker_action, NULL);
    pool_wait_quiescent(p);
    /* At least one worker observed a non-negative thread_idx. With
     * 256 tasks across 4 workers we should see most/all of them. */
    ASSERT(atomic_load(&g_max_idx) >= 0);
    ASSERT(atomic_load(&g_max_idx) < pool_n_workers(p));
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_recursive_spawn() */
static atomic_int g_recursive;

static void recursive_action(void *arg)
{
    int depth = (int)(intptr_t)arg;
    atomic_fetch_add(&g_recursive, 1);
    if (depth > 0) {
        /* Spawn a child from inside the worker. */
        pool_spawn(pool_current_worker->pool, recursive_action,
                   (void *)(intptr_t)(depth - 1));
    }
}

static int test_recursive_spawn(void)
{
    atomic_store(&g_recursive, 0);
    pool_t *p = pool_create(4);
    pool_init_barrier(p);
    /* depth 10 → 11 invocations (10..0). */
    pool_spawn(p, recursive_action, (void *)(intptr_t)10);
    pool_wait_quiescent(p);
    ASSERT(atomic_load(&g_recursive) == 11);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ test_concurrent_producers() */
struct producer_args { pool_t *p; int n; };

static void *producer_thread(void *arg)
{
    struct producer_args *a = arg;
    for (int i = 0; i < a->n; i++) pool_spawn(a->p, inc_action, NULL);
    return NULL;
}

static int test_concurrent_producers(void)
{
    enum { N_PROD = 8, PER = 500 };
    atomic_store(&g_count, 0);
    pool_t *p = pool_create(4);
    pool_init_barrier(p);

    pthread_t producers[N_PROD];
    struct producer_args args = { p, PER };
    for (int i = 0; i < N_PROD; i++)
        pthread_create(&producers[i], NULL, producer_thread, &args);
    for (int i = 0; i < N_PROD; i++)
        pthread_join(producers[i], NULL);

    pool_wait_quiescent(p);
    ASSERT(atomic_load(&g_count) == N_PROD * PER);
    pool_destroy(p);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    printf("301-pool-test:\n");
    RUN(create_destroy_no_tasks);
    RUN(default_n_workers);
    RUN(cap_at_max_workers);
    RUN(spawn_one);
    RUN(spawn_many);
    RUN(current_worker_tls);
    RUN(recursive_spawn);
    RUN(concurrent_producers);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
