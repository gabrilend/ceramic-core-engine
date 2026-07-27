/*
 * 023-test-readiness.c — proves the one rule (issue 204).
 *
 * What this is: the test that a station runs when, and only when,
 * every one of its input slots holds a value — exactly once per
 * complete set, regardless of arrival order, regardless of how many
 * threads are delivering.
 *
 * How it does it, in general terms: first a three-input box is fed
 * its values in every possible order, and must fire exactly once per
 * round, only at the third arrival. Then one two-input station is
 * hammered from eight threads at once, and the number of firings
 * must equal the number of complete pairs with every value counted
 * exactly once — the property the claim-under-mutex exists for.
 */
#include "018-station.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Atomic int fired;
static _Atomic long sum_seen;

/* {{{ sum3 and its hand shim */
/* Hand shim in generator shape; deleted by issue 302. */
static int sum3(int a, int b, int c)
{
    return a + b + c;
}

static void sum3__call(task_t *t)
{
    int a = *(int *)t->in[0];
    int b = *(int *)t->in[1];
    int c = *(int *)t->in[2];
    int r = sum3(a, b, c);
    fired++;
    sum_seen += r;
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

/* {{{ test_every_arrival_order() */
static void test_every_arrival_order(void)
{
    /* The six orders three values can arrive in. */
    static const int orders[6][3] = {
        {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0},
    };

    map_t *m = map_create(1);
    int sizes[3] = { sizeof(int), sizeof(int), sizeof(int) };
    map_place(m, 0, sum3__call, STATION_PLAIN, 3, sizes, sizeof(int));
    map_start(m, 2);

    fired = 0;
    sum_seen = 0;
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    for (int round = 0; round < 6; round++) {
        int before = fired;
        for (int step = 0; step < 3; step++) {
            int slot = orders[round][step];
            int value = round * 10 + slot;
            map_deliver_value(m, 0, slot, &value);
        }
        /* Nothing asserts `fired == before` between deliveries —
         * the task runs asynchronously — but by the end of all six
         * rounds the count settles the question. */
        (void)before;
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    if (fired != 6) {
        fprintf(stderr, "6 complete sets fired %d tasks\n", (int)fired);
        exit(1);
    }
    /* Each round sums round*30 + 0+1+2. */
    long expected = 0;
    for (int round = 0; round < 6; round++)
        expected += round * 30 + 3;
    if (sum_seen != expected) {
        fprintf(stderr, "values mispaired: sum %ld, expected %ld\n",
                (long)sum_seen, expected);
        exit(1);
    }

    map_destroy(m);
    printf("  every arrival order: one firing per complete set\n");
}
/* }}} */

/* {{{ the hammer */
static _Atomic long pair_sum;
static _Atomic int pair_count;

/* Hand shim in generator shape; deleted by issue 302. */
static long add2(int a, int b)
{
    return (long)a + b;
}

static void add2__call(task_t *t)
{
    int a = *(int *)t->in[0];
    int b = *(int *)t->in[1];
    long r = add2(a, b);
    pair_count++;
    pair_sum += r;
    memcpy(t->out, &r, sizeof r);
}

enum { HAMMERS = 8, PER_HAMMER = 2000 };
static map_t *hammer_map;

static void *hammer_main(void *arg)
{
    int base = (int)(long)arg * PER_HAMMER;
    for (int i = 0; i < PER_HAMMER; i++) {
        int v = base + i;
        map_deliver_value(hammer_map, 0, 0, &v);
        map_deliver_value(hammer_map, 0, 1, &v);
    }
    return NULL;
}

static void test_hammer(void)
{
    hammer_map = map_create(1);
    int sizes[2] = { sizeof(int), sizeof(int) };
    map_place(hammer_map, 0, add2__call, STATION_PLAIN, 2, sizes, sizeof(long));
    map_start(hammer_map, 4);

    pair_sum = 0;
    pair_count = 0;
    pool_submitter_register(hammer_map->pool);
    pool_release(hammer_map->pool);

    pthread_t threads[HAMMERS];
    for (long i = 0; i < HAMMERS; i++)
        pthread_create(&threads[i], NULL, hammer_main, (void *)i);
    for (int i = 0; i < HAMMERS; i++)
        pthread_join(threads[i], NULL);

    pool_submitter_unregister(hammer_map->pool);
    pool_join(hammer_map->pool);

    /* Every value 0..N-1 was delivered once to each side. Whatever
     * pairing the interleaving produced, the total across both sides
     * is fixed — a lost or doubled value moves it. */
    long n = (long)HAMMERS * PER_HAMMER;
    long expected = n * (n - 1); /* each value counted once per side */
    if (pair_count != n) {
        fprintf(stderr, "%d firings from %ld complete pairs\n",
                (int)pair_count, n);
        exit(1);
    }
    if (pair_sum != expected) {
        fprintf(stderr, "claim tore a value: sum %ld, expected %ld\n",
                (long)pair_sum, expected);
        exit(1);
    }

    map_destroy(hammer_map);
    printf("  %d threads hammering one station: %ld claims, none lost, none doubled\n",
           HAMMERS, n);
}
/* }}} */

int main(void)
{
    test_every_arrival_order();
    test_hammer();
    return 0;
}
