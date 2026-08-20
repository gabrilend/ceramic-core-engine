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
#include "026-registry.h"

#include <pthread.h>
#include <stdint.h>
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
            int port = orders[round][step];
            int value = round * 10 + port;
            map_deliver_value(m, 0, port, &value);
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

/* {{{ test_unconfigured_port_never_ready() */
/*
 * A port with no source is a state, not a value (issue 210b): the
 * station holding it can never be ready, no matter how much arrives
 * at its other ports, and becomes ready the moment that port is given
 * a source.
 *
 * The two halves are one run on one station rather than two, because
 * the interesting claim is not "it did not fire" — a station that is
 * simply broken also does not fire — but "it did not fire, and then
 * the same station did." Rebuilding between the halves would prove
 * only that two different stations behaved two different ways.
 *
 * The final count is what makes it airtight. Reading the firing count
 * during the unconfigured stretch is weak on its own, since a task
 * built wrongly might still be sitting in the queue unrun. Three
 * firings at the end is only reachable if that stretch produced none:
 * five values went to each of the other two ports, so anything that
 * fired early would push the total above three.
 *
 * It also proves what issue 210f is about. The five values that
 * arrived at ports 0 and 1 while port 2 was unconfigured are still
 * there afterwards — the conversion changed a tag and destroyed
 * nothing — and three of them are what the three firings consume.
 * Every value on a side is identical, so the sums hold whatever
 * order the pairing happens in; issue 210d is about to stop promising
 * that values leave a port in the order they arrived, and a test that
 * quietly depended on it would fail later for an unrelated reason.
 */
static void test_unconfigured_port_never_ready(void)
{
    enum { WAITING = 5, FED = 3 };

    map_t *m = map_create(1);
    int sizes[3] = { sizeof(int), sizeof(int), sizeof(int) };
    map_place(m, 0, sum3__call, STATION_PLAIN, 3, sizes, sizeof(int));

    /* Port 2 has no source. Nothing about the station is otherwise
     * unusual — it is fully placed, its slots are allocated, and it
     * would run happily if anyone said where port 2's values come
     * from. */
    map_in_port_convert(m, 0, 2, IN_PORT_NONE);

    map_start(m, 2);
    fired = 0;
    sum_seen = 0;
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    for (int i = 0; i < WAITING; i++) {
        int a = 100, b = 200;
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &b);
    }

    int fired_while_unconfigured = fired;

    /* The port is given a source. The slots it has been carrying all
     * along are what it starts using — no allocation happens here,
     * which is the whole of issue 210b's standing-buffer decision. */
    map_in_port_convert(m, 0, 2, IN_PORT_RING);
    for (int i = 0; i < FED; i++) {
        int c = 300;
        map_deliver_value(m, 0, 2, &c);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    if (fired_while_unconfigured != 0) {
        fprintf(stderr, "a station with an unconfigured port fired %d times\n",
                fired_while_unconfigured);
        exit(1);
    }
    if (fired != FED) {
        fprintf(stderr, "%d firings after the port was given a source, expected %d\n",
                (int)fired, FED);
        exit(1);
    }
    if (sum_seen != (long)FED * 600) {
        fprintf(stderr, "the waiting values did not survive: sum %ld, expected %ld\n",
                (long)sum_seen, (long)FED * 600);
        exit(1);
    }

    map_destroy(m);
    printf("  an unconfigured port held a station still, then let it run "
           "with %d values still waiting\n", WAITING - FED);
}
/* }}} */


/* {{{ test_values_survive_a_round_trip_through_static */
/*
 * The second half of issue 210f's promise: **conversion destroys
 * nothing**, in the direction that is easy to get wrong.
 *
 * Values are put into a ring port, the port is turned into a static
 * and back, and those values must still be served. The tempting
 * implementation frees the slots when a port stops being a buffer —
 * it looks like tidying up, and the buffer is sized to exactly the
 * type the port carries so it looks wasteful to keep. What it
 * actually does is throw away values a producer already handed over,
 * silently, and the person who converted the port finds out never.
 *
 * The station is placed **by name** rather than by hand, because
 * binding a static needs the port's type from the registry and hand
 * placement is never given one.
 *
 * Port 1 is starved until after the round trip, so nothing can
 * consume port 0's backlog while it is being carried across.
 */
static void test_values_survive_a_round_trip_through_static(void)
{
    enum { WAITING = 6 };

    map_t *m = map_create(1);
    map_place_box(m, 0, "add", STATION_PLAIN);

    map_start(m, 2);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* Port 0 fills up. Port 1 is empty, so nothing fires and the
     * values simply wait. */
    for (int i = 0; i < WAITING; i++) {
        int a = 7;
        map_deliver_value(m, 0, 0, &a);
    }

    /* Away from a buffer and back. A static needs a value before it
     * can be one, so port 0 is given a constant first — which is also
     * the path that used to free the slots. */
    map_in_port_static_text(m, 0, 0, "1000");
    map_in_port_convert(m, 0, 0, IN_PORT_RING);

    /* Now let it run. If the round trip lost the backlog, fewer than
     * WAITING pairs can ever form. */
    for (int i = 0; i < WAITING; i++) {
        int b = 3;
        map_deliver_value(m, 0, 1, &b);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    long runs = atomic_load(&map_station(m, 0)->runs);
    if (runs != WAITING) {
        fprintf(stderr, "a round trip through static lost values: %ld of %d "
                        "runs\n", runs, WAITING);
        exit(1);
    }

    map_destroy(m);
    printf("  %d values waited out a round trip through static and were "
           "all served\n", WAITING);
}
/* }}} */

/* {{{ test_cycling_a_tag_under_load */
/*
 * A port cycled through all three tags while a station is being fed
 * throughout, which is the case the mechanism has to survive rather
 * than merely permit.
 *
 * What it watches for is tearing: a readiness walk seeing a port
 * mid-change, or a claim dispatching on one tag while the storage
 * belongs to another. Conversion takes the station's mutex and so do
 * both of those, which is the argument — this is the test that the
 * argument holds when the two actually interleave, thousands of times.
 *
 * The feeder only ever touches port 1. Delivering into the port being
 * converted is refused outright while it is not a buffer, and a test
 * that raced against its own abort would be testing the harness.
 *
 * What must hold exactly: every run consumed one real value from each
 * port, so the number of runs can never exceed what was fed. Nothing
 * is asserted about *how many* runs happen, because that depends on
 * which tag port 0 happened to be wearing at each moment — and
 * pinning it down would be asserting a schedule rather than a
 * property.
 */
static _Atomic int cycle_stop;
static map_t      *cycle_map;

static void *cycle_feeder(void *arg)
{
    (void)arg;
    long fed = 0;
    pool_submitter_register(cycle_map->pool);
    while (!atomic_load(&cycle_stop)) {
        int b = 3;
        map_deliver_value(cycle_map, 0, 1, &b);
        fed++;
    }
    pool_submitter_unregister(cycle_map->pool);
    return (void *)(intptr_t)fed;
}

static void test_cycling_a_tag_under_load(void)
{
    enum { ROUNDS = 2000 };

    cycle_map = map_create(1);
    map_place_box(cycle_map, 0, "add", STATION_PLAIN);
    /* Port 0 starts as a static so that every tag in the cycle is
     * reachable from the one before it. */
    map_in_port_static_text(cycle_map, 0, 0, "7");

    /* The feeder floods port 1 while port 0 spends most of its time
     * unusable, so port 1 builds exactly the backlog the buffer
     * report exists to shout about. Saying so first is the difference
     * between a diagnostic doing its job and a line somebody has to
     * go and investigate. */
    printf("  (the growth warning below is what this scene is provoking)\n");
    fflush(stdout);

    map_start(cycle_map, 3);
    cycle_stop = 0;
    pool_submitter_register(cycle_map->pool);
    pool_release(cycle_map->pool);

    pthread_t feeder;
    pthread_create(&feeder, NULL, cycle_feeder, NULL);

    for (int i = 0; i < ROUNDS; i++) {
        map_in_port_convert(cycle_map, 0, 0, IN_PORT_NONE);
        map_in_port_convert(cycle_map, 0, 0, IN_PORT_RING);
        map_in_port_convert(cycle_map, 0, 0, IN_PORT_STATIC);
    }

    atomic_store(&cycle_stop, 1);
    void *fed_p = NULL;
    pthread_join(feeder, &fed_p);
    long fed = (long)(intptr_t)fed_p;
    pool_submitter_unregister(cycle_map->pool);
    pool_join(cycle_map->pool);

    long runs = atomic_load(&map_station(cycle_map, 0)->runs);
    if (runs > fed) {
        fprintf(stderr, "cycling a tag under load invented values: %ld runs "
                        "from %ld deliveries\n", runs, fed);
        exit(1);
    }
    /* And it has to have actually run. A station wedged by the
     * cycling would satisfy the check above trivially, which would
     * make this a test that passes by doing nothing. */
    if (runs == 0) {
        fprintf(stderr, "cycling a tag under load wedged the station: "
                        "%ld deliveries and not one run\n", fed);
        exit(1);
    }

    map_destroy(cycle_map);
    printf("  a port cycled through three tags %d times under load; "
           "%ld runs from %ld deliveries, none invented\n",
           ROUNDS * 3, runs, fed);
}
/* }}} */

/* {{{ test_every_parameter_needs_a_source */
/*
 * The check that a finished program has no parameter left facing
 * nothing (issue 210g).
 *
 * A port with no source is the ordinary state of a station somebody
 * has not finished wiring, so this is not an error while a program is
 * being assembled — it is an error the moment somebody says the
 * program is finished. Catching it then is what lets the complaint
 * name the station and the port. Left uncaught, the station simply
 * never becomes ready, and a program that quietly does less than it
 * was asked to is a bad way to learn about a typo.
 *
 * **Every one of them, not the first.** Somebody fixing a new program
 * wants the whole list.
 */
static void test_every_parameter_needs_a_source(void)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "add", STATION_PLAIN);

    /* Fully wired by default: every port starts expecting arrows. */
    if (map_check_sources(m) != NULL) {
        fprintf(stderr, "a freshly placed program was called incomplete: %s\n",
                map_check_sources(m));
        exit(1);
    }

    /* Two ports on two different stations, so the answer has to name
     * both rather than stopping at the first. */
    map_in_port_convert(m, 0, 1, IN_PORT_NONE);
    map_in_port_convert(m, 1, 0, IN_PORT_NONE);

    const char *said = map_check_sources(m);
    if (!said) {
        fprintf(stderr, "two unsourced ports went unnoticed\n");
        exit(1);
    }
    if (!strstr(said, "0.1") || !strstr(said, "1.0")) {
        fprintf(stderr, "the complaint did not name both ports: %s\n", said);
        exit(1);
    }
    if (!strstr(said, "2 ports")) {
        fprintf(stderr, "the complaint did not count them: %s\n", said);
        exit(1);
    }

    /* Giving one back a source leaves the other still named. */
    map_in_port_convert(m, 0, 1, IN_PORT_RING);
    said = map_check_sources(m);
    if (!said || strstr(said, "0.1")) {
        fprintf(stderr, "a port given a source was still complained about: "
                        "%s\n", said ? said : "(nothing)");
        exit(1);
    }

    /* And giving the last one back leaves nothing to say. */
    map_in_port_convert(m, 1, 0, IN_PORT_RING);
    if (map_check_sources(m) != NULL) {
        fprintf(stderr, "a fully wired program was still called incomplete\n");
        exit(1);
    }

    map_destroy(m);
    printf("  every parameter needs a source; two missing were both named "
           "and counted\n");
}
/* }}} */

int main(void)
{
    test_every_arrival_order();
    test_hammer();
    test_unconfigured_port_never_ready();
    test_values_survive_a_round_trip_through_static();
    test_cycling_a_tag_under_load();
    test_every_parameter_needs_a_source();
    return 0;
}
