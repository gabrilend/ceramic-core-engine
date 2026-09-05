/*
 * 013-test-pool-queue.c — proves the task queue ring (issue 101).
 *
 * What this is: the tests for the pool's queue considered purely as a
 * data structure — order, growth, and survival under many threads.
 *
 * How it does it, in general terms: a pool is created but its workers
 * are never released, so they stay parked at the starting gate and
 * the queue can be exercised directly from test threads without
 * anything competing for it.
 *
 * Two properties are proven:
 *   1. Everything pushed comes back out, once, in order, across
 *      several doublings — seeded with a ring that has already
 *      wrapped, because the unwrap copy is the part that would be
 *      wrong first.
 *   2. Under concurrent pushers and poppers, nothing is lost and
 *      nothing is duplicated.
 */
#include "cera.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* A task that exists only to be counted. The queue never calls it. */
typedef struct numbered_task {
    task_t base;
    int    id;
} numbered_task_t;

/* {{{ never_called() */
static void never_called(task_t *t)
{
    (void)t;
    fprintf(stderr, "queue test: a parked pool ran a task\n");
    abort();
}
/* }}} */

/* {{{ make_numbered() */
static numbered_task_t *make_numbered(int id)
{
    numbered_task_t *n = malloc(sizeof *n);
    if (!n) abort();
    n->base.call = never_called;
    n->id = id;
    return n;
}
/* }}} */

/* {{{ test_order_across_growth() */
/*
 * Push and pop so the ring wraps first, then push far past capacity
 * so it must double several times mid-wrap, and finally drain it —
 * asserting strict first-in-first-out the whole way.
 */
static void test_order_across_growth(void)
{
    pool_t *p = pool_create(1, NULL, NULL);

    /* Wrap the ring: advance head and tail together so the contents
     * straddle the array's end before any growth happens. */
    for (int i = 0; i < 5; i++)
        pool_push(p, &make_numbered(1000 + i)->base);
    for (int i = 0; i < 5; i++) {
        numbered_task_t *n = (numbered_task_t *)pool_pop(p);
        if (!n || n->id != 1000 + i) {
            fprintf(stderr, "wrap seeding broke order\n");
            exit(1);
        }
        free(n);
    }

    /* Now the real run: push several capacities' worth. */
    enum { COUNT = 1000 };
    for (int i = 0; i < COUNT; i++)
        pool_push(p, &make_numbered(i)->base);

    int capacity, high_water, growths;
    pool_queue_stats(p, &capacity, &high_water, &growths);
    if (growths < 3) {
        fprintf(stderr, "expected several growths, saw %d\n", growths);
        exit(1);
    }
    if (high_water < COUNT) {
        fprintf(stderr, "high water %d never reached the %d held\n",
                high_water, COUNT);
        exit(1);
    }

    for (int i = 0; i < COUNT; i++) {
        numbered_task_t *n = (numbered_task_t *)pool_pop(p);
        if (!n) {
            fprintf(stderr, "queue ran dry at %d of %d\n", i, COUNT);
            exit(1);
        }
        if (n->id != i) {
            fprintf(stderr, "order broken: wanted %d, got %d\n", i, n->id);
            exit(1);
        }
        free(n);
    }
    if (pool_pop(p) != NULL) {
        fprintf(stderr, "queue held more than was pushed\n");
        exit(1);
    }

    pool_destroy(p);
    printf("  order across growth: ok (capacity %d, %d growths)\n",
           capacity, growths);
}
/* }}} */

/* Shared state for the concurrency test. `seen` counts how many
 * times each id came back out; every slot must end at exactly one. */
enum { PUSHERS = 4, POPPERS = 4, PER_PUSHER = 5000 };
static pool_t *shared_pool;
static int seen[PUSHERS * PER_PUSHER];
static pthread_mutex_t seen_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int total_popped;

/* {{{ pusher_main() */
static void *pusher_main(void *arg)
{
    int base = (int)(long)arg * PER_PUSHER;
    for (int i = 0; i < PER_PUSHER; i++)
        pool_push(shared_pool, &make_numbered(base + i)->base);
    return NULL;
}
/* }}} */

/* {{{ popper_main() */
static void *popper_main(void *arg)
{
    (void)arg;
    /* Poppers spin until every id is accounted for; an empty pop is
     * just a pusher that has not caught up yet. */
    while (total_popped < PUSHERS * PER_PUSHER) {
        numbered_task_t *n = (numbered_task_t *)pool_pop(shared_pool);
        if (!n)
            continue;
        pthread_mutex_lock(&seen_mutex);
        seen[n->id]++;
        pthread_mutex_unlock(&seen_mutex);
        total_popped++;
        free(n);
    }
    return NULL;
}
/* }}} */

/* {{{ test_concurrent_push_pop() */
static void test_concurrent_push_pop(void)
{
    shared_pool = pool_create(1, NULL, NULL);

    pthread_t pushers[PUSHERS], poppers[POPPERS];
    for (long i = 0; i < POPPERS; i++)
        pthread_create(&poppers[i], NULL, popper_main, (void *)i);
    for (long i = 0; i < PUSHERS; i++)
        pthread_create(&pushers[i], NULL, pusher_main, (void *)i);

    for (int i = 0; i < PUSHERS; i++)
        pthread_join(pushers[i], NULL);
    for (int i = 0; i < POPPERS; i++)
        pthread_join(poppers[i], NULL);

    for (int i = 0; i < PUSHERS * PER_PUSHER; i++) {
        if (seen[i] != 1) {
            fprintf(stderr, "id %d came out %d times\n", i, seen[i]);
            exit(1);
        }
    }

    pool_destroy(shared_pool);
    printf("  concurrent push/pop of %d tasks: nothing lost, nothing doubled\n",
           PUSHERS * PER_PUSHER);
}
/* }}} */

int main(void)
{
    test_order_across_growth();
    test_concurrent_push_pop();
    return 0;
}
