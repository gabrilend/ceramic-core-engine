/*
 * 039-phase-5-decide-demo.c — a map that decides.
 *
 * What this is: the phase 5 demonstration. Every earlier map could
 * only transform; these maps choose, and every choice is visible in
 * the wiring rather than buried in a function. A sorting network
 * splits a stream into buckets through three comparators. One
 * comparator's three arrows produce all six comparison operators —
 * and then a shape no operator has a name for. An iterator deals
 * work across consumers of wildly uneven speed, exactly evenly, in
 * thoroughly unfair order. And a comparison that raw bytes would
 * have gotten backwards is shown beside the lie.
 *
 * How it does it, in general terms: registry boxes placed as
 * comparators and iterators, statics for thresholds, counting sinks
 * on every port, and arithmetic checked against what the fed values
 * demand. The bucket view redraws as the network fills.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *report;

/* {{{ say() / now_seconds() */
static void say(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (report) {
        va_start(args, format);
        vfprintf(report, format, args);
        va_end(args);
    }
    fflush(stdout);
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: the sorting network.                                    */
/* ------------------------------------------------------------------ */

static _Atomic int buckets[4];

static void bucket0__call(task_t *t) { (void)t; buckets[0]++; }
static void bucket1__call(task_t *t) { (void)t; buckets[1]++; }
static void bucket2__call(task_t *t) { (void)t; buckets[2]++; }
static void bucket3__call(task_t *t) { (void)t; buckets[3]++; }

/* {{{ draw_buckets() */
static void draw_buckets(void)
{
    static const char *const labels[4] = {
        "0..24 ", "25..49", "50..74", "75..99",
    };
    for (int b = 0; b < 4; b++) {
        say("    %s %4d  ", labels[b], (int)buckets[b]);
        for (int h = 0; h < buckets[b] / 4; h++)
            say("#");
        say("\n");
    }
}
/* }}} */

/* {{{ scene_sorting_network() */
static void scene_sorting_network(void)
{
    enum { VALUES = 400 };

    /* Three deciders, four buckets:
     *   C50 splits the world at fifty; its low side meets C25, its
     *   equal-and-high sides meet C75. Equal and greater arrows
     *   landing on one destination is itself the point — ">=" is a
     *   wiring, not a setting. */
    map_t *m = map_create(7);
    map_place_box(m, 0, "keep", STATION_COMPARATOR); /* vs 50 */
    map_place_box(m, 1, "keep", STATION_COMPARATOR); /* vs 25 */
    map_place_box(m, 2, "keep", STATION_COMPARATOR); /* vs 75 */
    int one_int[1] = { sizeof(int) };
    map_place(m, 3, bucket0__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 4, bucket1__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 5, bucket2__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 6, bucket3__call, STATION_PLAIN, 1, one_int, 0);

    map_connect(m, 0, 0, 1, 0);   /* below fifty -> the 25 decider  */
    map_connect(m, 0, 1, 2, 0);   /* exactly fifty -> the 75 decider */
    map_connect(m, 0, 2, 2, 0);   /* above fifty  -> the 75 decider */
    map_connect(m, 1, 0, 3, 0);   /* below 25 -> bucket 0 */
    map_connect(m, 1, 1, 4, 0);   /* at or above 25 -> bucket 1 */
    map_connect(m, 1, 2, 4, 0);
    map_connect(m, 2, 0, 5, 0);   /* below 75 -> bucket 2 */
    map_connect(m, 2, 1, 6, 0);   /* at or above 75 -> bucket 3 */
    map_connect(m, 2, 2, 6, 0);

    map_statics_alloc(m, 3);
    map_static_set_text(m, 0, "50");
    map_static_set_text(m, 1, "25");
    map_static_set_text(m, 2, "75");
    map_slot_static(m, 0, 1, 0);
    map_slot_static(m, 1, 1, 1);
    map_slot_static(m, 2, 1, 2);

    /* What the arithmetic demands of the buckets. */
    int expected[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < VALUES; i++) {
        int v = (i * 7) % 100;
        expected[v < 25 ? 0 : v < 50 ? 1 : v < 75 ? 2 : 3]++;
    }

    map_start(m, 0);
    memset((void *)buckets, 0, sizeof buckets);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    say("scene 1 — a sorting network, filling\n");
    for (int burst = 0; burst < 4; burst++) {
        for (int i = burst * (VALUES / 4); i < (burst + 1) * (VALUES / 4); i++) {
            int v = (i * 7) % 100;
            map_deliver_value(m, 0, 0, &v);
        }
        usleep(30000);
        say("  after %d values:\n", (burst + 1) * (VALUES / 4));
        draw_buckets();
    }
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    int all_right = buckets[0] == expected[0] && buckets[1] == expected[1]
                 && buckets[2] == expected[2] && buckets[3] == expected[3];
    say("  distribution %d/%d/%d/%d against expected %d/%d/%d/%d — %s\n",
        (int)buckets[0], (int)buckets[1], (int)buckets[2], (int)buckets[3],
        expected[0], expected[1], expected[2], expected[3],
        all_right ? "exact" : "WRONG");
    say("  the branching is three stations and nine arrows; no function\n");
    say("  anywhere contains an if about buckets\n\n");
    if (!all_right)
        exit(1);
    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: six operators, and a seventh shape with no name.        */
/* ------------------------------------------------------------------ */

static _Atomic int op_hits;
static _Atomic int shape_equal;
static _Atomic int shape_greater;

static void op_sink__call(task_t *t)     { (void)t; op_hits++; }
static void shape_eq__call(task_t *t)    { (void)t; shape_equal++; }
static void shape_gt__call(task_t *t)    { (void)t; shape_greater++; }

/* {{{ run_operator() */
/*
 * One comparator against fifty, with the named outcome ports wired
 * to a single counter. Twenty values below, twenty at, twenty above:
 * the count that arrives is the operator's truth table in numbers.
 */
static int run_operator(const int *ports, int n_ports)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "keep", STATION_COMPARATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, op_sink__call, STATION_PLAIN, 1, one_int, 0);
    for (int i = 0; i < n_ports; i++)
        map_connect(m, 0, ports[i], 1, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "50");
    map_slot_static(m, 0, 1, 0);

    map_start(m, 2);
    op_hits = 0;
    for (int i = 0; i < 20; i++) {
        int below = 10, at = 50, above = 90;
        map_deliver_value(m, 0, 0, &below);
        map_deliver_value(m, 0, 0, &at);
        map_deliver_value(m, 0, 0, &above);
    }
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    return op_hits;
}
/* }}} */

/* {{{ scene_six_operators() */
static void scene_six_operators(void)
{
    /* Which ports mean which operator: the entire "no operator
     * setting" argument as a dispatch table. */
    struct operator_wiring {
        const char *name;
        int         ports[2];
        int         n;
        int         expected;
    };
    static const struct operator_wiring table[6] = {
        { "<",  { 0 },    1, 20 },
        { "<=", { 0, 1 }, 2, 40 },
        { "==", { 1 },    1, 20 },
        { "!=", { 0, 2 }, 2, 40 },
        { ">=", { 1, 2 }, 2, 40 },
        { ">",  { 2 },    1, 20 },
    };

    say("scene 2 — the same comparison, six ways\n");
    say("  one station, sixty values (20 below, 20 at, 20 above 50);\n");
    say("  the operator is only which arrows exist:\n");
    for (int i = 0; i < 6; i++) {
        int got = run_operator(table[i].ports, table[i].n);
        say("    %-2s  wired ports %d%s%s  received %2d  (expected %2d)  %s\n",
            table[i].name,
            table[i].ports[0],
            table[i].n > 1 ? "," : " ",
            table[i].n > 1 ? (char[]){ (char)('0' + table[i].ports[1]), 0 } : " ",
            got, table[i].expected,
            got == table[i].expected ? "" : "WRONG");
        if (got != table[i].expected)
            exit(1);
    }

    /* The seventh shape: equal goes one way, greater another, less
     * nowhere. No comparison operator can say this; three arrows
     * say it easily. */
    map_t *m = map_create(3);
    map_place_box(m, 0, "keep", STATION_COMPARATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, shape_eq__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 2, shape_gt__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 1, 1, 0);
    map_connect(m, 0, 2, 2, 0);
    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "50");
    map_slot_static(m, 0, 1, 0);
    map_start(m, 2);
    shape_equal = 0;
    shape_greater = 0;
    for (int i = 0; i < 20; i++) {
        int below = 10, at = 50, above = 90;
        map_deliver_value(m, 0, 0, &below);
        map_deliver_value(m, 0, 0, &at);
        map_deliver_value(m, 0, 0, &above);
    }
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    say("  and one no operator can name: equal one way, greater another,\n");
    say("  less discarded — %d equals here, %d greaters there, 20 gone\n\n",
        (int)shape_equal, (int)shape_greater);
    if (shape_equal != 20 || shape_greater != 20)
        exit(1);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: the spreader under uneven load.                       */
/* ------------------------------------------------------------------ */

static _Atomic int spread_counts[3];
static char arrival_order[64];
static _Atomic int arrivals;

/* {{{ the three consumers, deliberately mismatched */
static void note_arrival(int port)
{
    int i = arrivals++;
    if (i < (int)sizeof arrival_order - 1)
        arrival_order[i] = (char)('0' + port);
    spread_counts[port]++;
}

static void burn_us(int rounds)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < rounds * 150; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    if (x == 0) abort();
}

static void quick_eater__call(task_t *t)  { (void)t; burn_us(10); note_arrival(0); }
static void middle_eater__call(task_t *t) { (void)t; burn_us(200); note_arrival(1); }
static void slow_eater__call(task_t *t)   { (void)t; burn_us(800); note_arrival(2); }
/* }}} */

/* {{{ scene_spreader() */
static void scene_spreader(void)
{
    enum { VALUES = 90 };
    map_t *m = map_create(4);
    map_place_box(m, 0, "keep", STATION_ITERATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, quick_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 2, middle_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 3, slow_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 1, 2, 0);
    map_connect(m, 0, 2, 3, 0);

    map_start(m, 0);
    memset((void *)spread_counts, 0, sizeof spread_counts);
    memset(arrival_order, 0, sizeof arrival_order);
    arrivals = 0;

    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    say("scene 3 — even spreading, uneven finishing\n");
    say("  three consumers burning ~10, ~200, and ~800 units each\n");
    say("  counts     %d / %d / %d — exactly even\n",
        (int)spread_counts[0], (int)spread_counts[1], (int)spread_counts[2]);
    say("  first arrivals, by consumer: %.48s...\n", arrival_order);
    say("  assignment is round-robin at enqueue; arrival is whenever each\n");
    say("  one finishes. a spreader, not a funnel — both halves on view\n\n");
    if (spread_counts[0] != VALUES / 3 || spread_counts[1] != VALUES / 3
        || spread_counts[2] != VALUES / 3)
        exit(1);
    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the comparison that would have been wrong.             */
/* ------------------------------------------------------------------ */

static _Atomic int wrong_scene_less;
static _Atomic int wrong_scene_greater;

static void ws_less__call(task_t *t)    { (void)t; wrong_scene_less++; }
static void ws_greater__call(task_t *t) { (void)t; wrong_scene_greater++; }

/* {{{ scene_byte_lie() */
static void scene_byte_lie(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "mix", STATION_COMPARATOR);
    int one_double[1] = { sizeof(double) };
    map_place(m, 1, ws_less__call, STATION_PLAIN, 1, one_double, 0);
    map_place(m, 2, ws_greater__call, STATION_PLAIN, 1, one_double, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 2, 2, 0);
    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "0.5");
    map_slot_static(m, 0, 2, 0);

    map_start(m, 2);
    wrong_scene_less = 0;
    wrong_scene_greater = 0;
    int count = 1;
    double factor = -2.0;
    map_deliver_value(m, 0, 0, &count);
    map_deliver_value(m, 0, 1, &factor);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    /* The raw-bytes verdict: both doubles read as unsigned words.
     * The sign bit is the most significant bit, so a negative number
     * reads as enormous — this is the exact wrong answer a naive
     * byte comparison routes on. (memcmp on a little-endian machine
     * lies differently, starting from the low byte; the unsigned
     * read is the canonical form of the mistake.) */
    double a = -2.0, b = 0.5;
    unsigned long long abits, bbits;
    memcpy(&abits, &a, sizeof abits);
    memcpy(&bbits, &b, sizeof bbits);

    say("scene 4 — the comparison that would have been wrong\n");
    say("  -2.0 against a threshold of 0.5:\n");
    say("    the engine routed it      %s\n",
        wrong_scene_less ? "less — correct" : "GREATER — wrong");
    say("    the bytes as an unsigned  0x%016llx vs 0x%016llx: %s\n",
        abits, bbits,
        abits > bbits ? "\"greater\" — the sign bit read as magnitude"
                      : "\"less\"");
    say("  the disagreement is the entire reason compare functions exist\n\n");
    if (!wrong_scene_less || wrong_scene_greater)
        exit(1);
    if (abits <= bbits) {
        fprintf(stderr, "the byte lie failed to lie — scene needs rethinking\n");
        exit(1);
    }
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc > 1) {
        char path[4096];
        snprintf(path, sizeof path,
                 "%s/tmp/shared-memory/phase-5-decide-report.txt", argv[1]);
        report = fopen(path, "w");
    }

    double before = now_seconds();
    say("=== phase 5 demo: a map that decides ===\n\n");
    scene_sorting_network();
    scene_six_operators();
    scene_spreader();
    scene_byte_lie();
    say("=== branching lives in the wiring, visible, with no name needed ===\n");
    say("(whole demo: %.2f s on the same pool as every phase before)\n",
        now_seconds() - before);

    if (report)
        fclose(report);
    return 0;
}
