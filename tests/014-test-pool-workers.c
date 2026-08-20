/*
 * 014-test-pool-workers.c — proves the workers and run loop (issue 102).
 *
 * What this is: the test that a fixed set of worker threads actually
 * runs what the queue holds — every task exactly once, none invented,
 * none dropped — and that the starting gate holds them back until
 * they are released.
 *
 * How it does it, in general terms: a known number of counting tasks
 * is pushed while the workers are still parked, the gate is opened,
 * and the pool is left to terminate itself. Each task marks its own
 * slot in a ledger; the ledger must read "exactly once" everywhere.
 */
#include "011-pool.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum { TASKS = 20000 };

static _Atomic int runs[TASKS];
static _Atomic long total;
static _Atomic int ran_before_release;
static _Atomic int release_flag;

typedef struct counting_task {
    task_t base;
    int    id;
} counting_task_t;

/* {{{ count_once() */
static void count_once(task_t *t)
{
    counting_task_t *c = (counting_task_t *)t;
    /* A task running while the gate is still shut would mean the
     * barrier leaks — remembered here, asserted in main. */
    if (!release_flag)
        ran_before_release = 1;
    runs[c->id]++;
    total++;
}
/* }}} */

int main(void)
{
    pool_t *p = pool_create(4, NULL, NULL);

    for (int i = 0; i < TASKS; i++) {
        counting_task_t *c = malloc(sizeof *c);
        if (!c) abort();
        c->base.call = count_once;
        c->id = i;
        pool_push(p, &c->base);
    }

    release_flag = 1;
    pool_release(p);
    pool_join(p);

    if (ran_before_release) {
        fprintf(stderr, "a task ran before the pool was released\n");
        exit(1);
    }
    if (total != TASKS) {
        fprintf(stderr, "ran %ld of %d tasks\n", (long)total, TASKS);
        exit(1);
    }
    for (int i = 0; i < TASKS; i++) {
        if (runs[i] != 1) {
            fprintf(stderr, "task %d ran %d times\n", i, runs[i]);
            exit(1);
        }
    }

    if (pool_worker_count(p) != 4) {
        fprintf(stderr, "asked for 4 workers, got %d\n", pool_worker_count(p));
        exit(1);
    }

    pool_destroy(p);
    printf("  %d tasks each ran exactly once across 4 workers\n", TASKS);
    return 0;
}
