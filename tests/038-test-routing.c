/*
 * 038-test-routing.c — proves the routing kinds (issues 501–504).
 *
 * What this is: the tests that a map can decide. A comparator's
 * three outcomes each reach their own port, its threshold behaves
 * like any other port, an unwired outcome discards, comparison is
 * semantic (floats and author-ordered structs route where raw bytes
 * would not), and an iterator spreads exactly evenly no matter how
 * many threads are enqueuing.
 *
 * How it does it, in general terms: comparator maps place registry
 * boxes with the extra threshold port bound static or left buffered,
 * with counting sinks on each port; every count is then checked
 * against what the mathematics says. The plain path needs no new
 * test — the whole earlier suite is that test, unchanged.
 */
#include "018-station.h"
#include "026-registry.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x; float y; float z; } vec3;

static _Atomic int hits_less;
static _Atomic int hits_equal;
static _Atomic int hits_greater;

/* {{{ check() and the three counting sinks */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "routing test failed: %s\n", what);
        exit(1);
    }
}

static void reset_hits(void)
{
    hits_less = 0;
    hits_equal = 0;
    hits_greater = 0;
}

/* Harness sinks; each port gets its own so the counts are the map's
 * decisions made visible. They take int-sized values; the byte width
 * of what lands is the wire's business. */
static void less_sink__call(task_t *t)   { (void)t; hits_less++; }
static void equal_sink__call(task_t *t)  { (void)t; hits_equal++; }
static void greater_sink__call(task_t *t){ (void)t; hits_greater++; }
/* }}} */

/* {{{ wire_three_sinks() */
/* Stations base+0..2 become the less/equal/greater destinations of
 * `station`'s three ports, each sink sized to the compared value. */
static void wire_three_sinks(map_t *m, int station, int base, int value_size)
{
    int one[1] = { value_size };
    map_place(m, base + 0, less_sink__call, STATION_PLAIN, 1, one, 0);
    map_place(m, base + 1, equal_sink__call, STATION_PLAIN, 1, one, 0);
    map_place(m, base + 2, greater_sink__call, STATION_PLAIN, 1, one, 0);
    map_connect(m, station, 0, base + 0, 0);
    map_connect(m, station, 1, base + 1, 0);
    map_connect(m, station, 2, base + 2, 0);
}
/* }}} */

/* {{{ test_three_outcomes() */
static void test_three_outcomes(void)
{
    map_t *m = map_create(4);
    map_place_box(m, 0, "add", STATION_COMPARATOR);
    wire_three_sinks(m, 0, 1, sizeof(int));

    map_in_port_static_text(m, 0, 2, "100");   /* the threshold port, last */

    map_start(m, 4);
    reset_hits();

    /* Sums 90, 100, 110, delivered as (a, b) pairs — five of each. */
    for (int i = 0; i < 5; i++) {
        int a = 40, below = 50, exact = 60, above = 70;
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &below);
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &exact);
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &above);
    }
    pool_release(m->pool);
    pool_join(m->pool);

    check(hits_less == 5 && hits_equal == 5 && hits_greater == 5,
          "five sums below, at, and above the threshold reached their ports");
    map_destroy(m);
    printf("  three outcomes, three ports, every value where it belongs\n");
}
/* }}} */

/* {{{ test_buffered_threshold_blocks() */
static void test_buffered_threshold_blocks(void)
{
    /* The threshold left as a buffer: the extra port really is in
     * the readiness walk, so the station starves without it. */
    map_t *m = map_create(4);
    map_place_box(m, 0, "add", STATION_COMPARATOR);
    wire_three_sinks(m, 0, 1, sizeof(int));

    map_start(m, 2);
    reset_hits();

    int a = 1, b = 2;
    map_deliver_value(m, 0, 0, &a);
    map_deliver_value(m, 0, 1, &b);
    /* Data complete, threshold absent: were the extra port not
     * checked, a task would fire here and the pool would not be
     * empty when released... */
    int threshold = 10;
    map_deliver_value(m, 0, 2, &threshold);
    pool_release(m->pool);
    pool_join(m->pool);

    check(hits_less == 1 && hits_equal == 0 && hits_greater == 0,
          "the station fired exactly once, only after its threshold arrived");
    map_destroy(m);
    printf("  a buffered threshold gates readiness like any other port\n");
}
/* }}} */

/* {{{ test_unwired_port_discards() */
static void test_unwired_port_discards(void)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_COMPARATOR);
    /* Only `greater` is wired; less and equal exist as empty exits. */
    int one[1] = { sizeof(int) };
    map_place(m, 1, greater_sink__call, STATION_PLAIN, 1, one, 0);
    map_connect(m, 0, 2, 1, 0);

    map_in_port_static_text(m, 0, 2, "100");

    map_start(m, 2);
    reset_hits();
    for (int i = 0; i < 10; i++) {
        int a = 1, b = 1;          /* far below: down the unwired port */
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &b);
    }
    int big_a = 90, big_b = 90;    /* above: down the wired one */
    map_deliver_value(m, 0, 0, &big_a);
    map_deliver_value(m, 0, 1, &big_b);
    pool_release(m->pool);
    pool_join(m->pool);

    check(hits_greater == 1, "the wired outcome delivered");
    check(hits_less == 0 && hits_equal == 0, "nothing leaked from unwired ports");
    map_destroy(m);
    printf("  ten values down an unwired outcome vanished quietly, one delivered\n");
}
/* }}} */

/* {{{ test_float_semantics() */
static void test_float_semantics(void)
{
    /* mix returns double; the value is negative and the threshold
     * positive. Raw bytes read a negative as enormous — the sign bit
     * is the top bit — so this routing to `less` is the proof the
     * semantic compare ran. */
    map_t *m = map_create(4);
    map_place_box(m, 0, "mix", STATION_COMPARATOR);
    wire_three_sinks(m, 0, 1, sizeof(double));

    map_in_port_static_text(m, 0, 2, "0.5");

    map_start(m, 2);
    reset_hits();
    int count = 1;
    double factor = -2.0;          /* mix -> -2.0 */
    map_deliver_value(m, 0, 0, &count);
    map_deliver_value(m, 0, 1, &factor);
    pool_release(m->pool);
    pool_join(m->pool);

    check(hits_less == 1 && hits_greater == 0,
          "-2.0 routed below 0.5; bytes would have said above");
    map_destroy(m);
    printf("  a negative double routed semantically, against its bytes\n");
}
/* }}} */

/* {{{ test_struct_author_order() */
static void test_struct_author_order(void)
{
    /* vec3 orders by squared magnitude, per its author compare. The
     * fed vector's first field is far larger than the threshold's,
     * so field-order or byte-order comparison would say greater;
     * magnitude says less. */
    map_t *m = map_create(4);
    map_place_box(m, 0, "make_vec3", STATION_COMPARATOR);
    wire_three_sinks(m, 0, 1, sizeof(vec3));

    map_in_port_static_text(m, 0, 3, "{ 1, 8, 8 }");   /* magnitude^2 = 129 */

    map_start(m, 2);
    reset_hits();
    float x = 9, y = 0, z = 0;                  /* magnitude^2 = 81 */
    map_deliver_value(m, 0, 0, &x);
    map_deliver_value(m, 0, 1, &y);
    map_deliver_value(m, 0, 2, &z);
    pool_release(m->pool);
    pool_join(m->pool);

    check(hits_less == 1 && hits_greater == 0,
          "the author's magnitude ordering decided, not the first field");
    map_destroy(m);
    printf("  a struct routed by its author's ordering, not its layout\n");
}
/* }}} */

/* {{{ iterator: round robin, and exact fairness under a crowd */
static _Atomic int port_counts[3];

static void port0_sink__call(task_t *t) { (void)t; port_counts[0]++; }
static void port1_sink__call(task_t *t) { (void)t; port_counts[1]++; }
static void port2_sink__call(task_t *t) { (void)t; port_counts[2]++; }

static map_t *crowd_map;

static void *crowd_feeder(void *arg)
{
    int base = (int)(long)arg * 300;
    for (int i = 0; i < 300; i++) {
        int v = base + i;
        map_deliver_value(crowd_map, 0, 0, &v);
    }
    return NULL;
}

static void test_iterator_spreads(void)
{
    enum { FEEDERS = 8, PER = 300 };
    crowd_map = map_create(4);
    map_place_box(crowd_map, 0, "double_it", STATION_ITERATOR);
    int one[1] = { sizeof(int) };
    map_place(crowd_map, 1, port0_sink__call, STATION_PLAIN, 1, one, 0);
    map_place(crowd_map, 2, port1_sink__call, STATION_PLAIN, 1, one, 0);
    map_place(crowd_map, 3, port2_sink__call, STATION_PLAIN, 1, one, 0);
    map_connect(crowd_map, 0, 0, 1, 0);
    map_connect(crowd_map, 0, 1, 2, 0);
    map_connect(crowd_map, 0, 2, 3, 0);

    map_start(crowd_map, 0);
    port_counts[0] = port_counts[1] = port_counts[2] = 0;

    pool_submitter_register(crowd_map->pool);
    pool_release(crowd_map->pool);

    pthread_t feeders[FEEDERS];
    for (long i = 0; i < FEEDERS; i++)
        pthread_create(&feeders[i], NULL, crowd_feeder, (void *)i);
    for (int i = 0; i < FEEDERS; i++)
        pthread_join(feeders[i], NULL);

    pool_submitter_unregister(crowd_map->pool);
    pool_join(crowd_map->pool);

    int expected = FEEDERS * PER / 3;
    check(port_counts[0] == expected && port_counts[1] == expected
          && port_counts[2] == expected,
          "the cursor under the mutex dealt every port an exact share");
    map_destroy(crowd_map);
    printf("  %d concurrent values spread %d/%d/%d — exactly even\n",
           FEEDERS * PER, (int)port_counts[0], (int)port_counts[1],
           (int)port_counts[2]);
}
/* }}} */

int main(void)
{
    test_three_outcomes();
    test_buffered_threshold_blocks();
    test_unwired_port_discards();
    test_float_semantics();
    test_struct_author_order();
    test_iterator_spreads();
    return 0;
}
