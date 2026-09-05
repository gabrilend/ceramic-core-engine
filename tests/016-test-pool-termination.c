/*
 * 016-test-pool-termination.c — proves the pool knows when it is done
 * (issue 104).
 *
 * What this is: the tests for the one decision the pool makes on its
 * own — that the program is finished. The failure this guards against
 * looks exactly like success: a pool that exits cleanly having
 * silently not done part of its work.
 *
 * How it does it, in general terms:
 *   1. Chains — each task enqueues its successor, several chains of
 *      wildly different lengths at once. The pool may only declare
 *      itself done after every chain has run to its end.
 *   2. An empty pool released with nothing to do terminates at once
 *      rather than sleeping forever.
 *   3. Many small pools in a row, each with a burst of work — a
 *      probabilistic net for premature termination, run enough times
 *      that a lost-wakeup class of bug would surface.
 *
 * A note on the test issue 104 asks for and this file does not have:
 * the issue wants the check-then-register race reproduced by delaying
 * a worker between those two steps. In this implementation the check
 * and the registration happen inside a single hold of the queue
 * mutex, and the wait releases that mutex atomically — the window the
 * race needs does not exist, so there is nothing to delay into. The
 * first-pass report records this as a doc/implementation mismatch:
 * the protocol as documented defends a looser lock discipline than
 * the one actually built.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static _Atomic long chain_total;

typedef struct chain_task {
    cera_task_t  base;
    cera_pool_t *pool;
    int     remaining;
} chain_task_t;

/* {{{ chain_step() */
/*
 * Two paths: links remaining means forge the next one and push it —
 * from inside the pool, which is the one legal place to push after
 * startup; none remaining means this chain is done and only the
 * count remains.
 */
static void chain_step(cera_task_t *t)
{
    chain_task_t *c = (chain_task_t *)t;
    chain_total++;
    if (c->remaining > 0) {
        chain_task_t *next = malloc(sizeof *next);
        if (!next) abort();
        next->base.call = chain_step;
        next->pool = c->pool;
        next->remaining = c->remaining - 1;
        cera_pool_push(c->pool, &next->base);
    }
}
/* }}} */

/* {{{ test_chains() */
static void test_chains(void)
{
    /* Lengths chosen to be wildly uneven so short chains leave their
     * workers idle long before the longest is done — the situation
     * where an eager termination check would cut the long chain off. */
    static const int lengths[] = { 1, 10, 100, 1000, 50000 };
    enum { CHAINS = sizeof lengths / sizeof lengths[0] };

    long expected = 0;
    for (int i = 0; i < CHAINS; i++)
        expected += lengths[i] + 1;

    chain_total = 0;
    cera_pool_t *p = cera_pool_create(4, NULL, NULL);
    for (int i = 0; i < CHAINS; i++) {
        chain_task_t *c = malloc(sizeof *c);
        if (!c) abort();
        c->base.call = chain_step;
        c->pool = p;
        c->remaining = lengths[i];
        cera_pool_push(p, &c->base);
    }
    cera_pool_release(p);
    cera_pool_join(p);

    if (chain_total != expected) {
        fprintf(stderr,
                "pool declared itself done after %ld of %ld chain links\n",
                (long)chain_total, expected);
        exit(1);
    }
    cera_pool_destroy(p);
    printf("  five uneven chains, %ld links, none cut short\n", expected);
}
/* }}} */

/* {{{ test_empty_pool_terminates() */
static void test_empty_pool_terminates(void)
{
    cera_pool_t *p = cera_pool_create(3, NULL, NULL);
    cera_pool_release(p);
    /* Nothing was ever pushed. If the last-sleeper rule is wrong in
     * the empty direction, this join never returns and the test
     * runner's timeout is the error message. */
    cera_pool_join(p);
    cera_pool_destroy(p);
    printf("  an empty pool terminates instead of sleeping forever\n");
}
/* }}} */

/* {{{ test_repeated_small_pools() */
static _Atomic int burst_ran;

static void burst_tick(cera_task_t *t)
{
    (void)t;
    burst_ran++;
}

static void test_repeated_small_pools(void)
{
    enum { ROUNDS = 200, BURST = 50 };
    for (int r = 0; r < ROUNDS; r++) {
        burst_ran = 0;
        cera_pool_t *p = cera_pool_create(4, NULL, NULL);
        for (int i = 0; i < BURST; i++) {
            cera_task_t *t = malloc(sizeof *t);
            if (!t) abort();
            t->call = burst_tick;
            cera_pool_push(p, t);
        }
        cera_pool_release(p);
        cera_pool_join(p);
        if (burst_ran != BURST) {
            fprintf(stderr, "round %d: %d of %d tasks ran\n",
                    r, (int)burst_ran, BURST);
            exit(1);
        }
        cera_pool_destroy(p);
    }
    printf("  %d fresh pools each ran their full burst\n", ROUNDS);
}
/* }}} */

int main(void)
{
    test_chains();
    test_empty_pool_terminates();
    test_repeated_small_pools();
    return 0;
}
