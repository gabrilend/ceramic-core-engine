/*
 * 025-phase-2-graph-demo.c — a graph that runs itself.
 *
 * What this is: the phase 2 demonstration. A map is described, values
 * are dropped in, and the machine fills every core on its own —
 * nobody scheduled any of this. Each scene measures one claim the
 * design makes: occupancy without arrangement, safe overlap of one
 * station, buffers absorbing mismatched speeds, the price of fan-out,
 * and a live look at values pooling behind a slow station.
 *
 * How it does it, in general terms: maps are built with the phase 2
 * construction calls and hand shims (both scaffolding, both marked
 * for replacement), seeded before the workers are released, and read
 * afterwards through the pool's and the slots' own counters. Boxes
 * that need to take time burn arithmetic rather than sleeping,
 * because nothing in this engine is allowed to block.
 *
 * Reuses phase 1's pool wholesale — same queue, same workers, same
 * termination — which is the point of a phase demo: the old tool
 * doing new work.
 */
#include "018-station.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *report;

/* {{{ say() */
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
/* }}} */

/* {{{ now_seconds() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ burn() — takes time without blocking */
/*
 * Roughly `rounds` microseconds of honest arithmetic. Calibrated
 * loosely; the demo compares relative numbers, not absolute ones.
 */
static int burn(int rounds)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < rounds * 150; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    return (int)(x & 0x7fffffff);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: occupancy across a wide map.                            */
/* ------------------------------------------------------------------ */

static _Atomic int busy_now;
static _Atomic int busy_peak;
static _Atomic long wide_done;

/* {{{ wide_work and shim — hand shim, deleted by issue 302 */
static int wide_work(int x)
{
    return x + burn(80);
}

static void wide_work__call(task_t *t)
{
    int entered = ++busy_now;
    int peak = busy_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&busy_peak, &peak, entered))
        ;
    int x = *(int *)t->in[0];
    int r = wide_work(x);
    wide_done++;
    busy_now--;
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

/* {{{ scene_occupancy() */
static void scene_occupancy(void)
{
    enum { WIDTH = 16, VALUES_PER = 60 };
    /* One row of independent stations — a map that is parallel
     * because it is wide, not because anyone asked. */
    map_t *m = map_create(WIDTH);
    int one_int[1] = { sizeof(int) };
    for (int i = 0; i < WIDTH; i++)
        map_place(m, i, wide_work__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_start(m, 0); /* 0 = one worker per online processor */

    busy_now = 0;
    busy_peak = 0;
    wide_done = 0;

    for (int v = 0; v < VALUES_PER; v++)
        for (int s = 0; s < WIDTH; s++)
            map_deliver_value(m, s, 0, &v);

    double before = now_seconds();
    pool_release(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;

    int workers = pool_worker_count(m->pool);
    say("scene 1 — occupancy across a wide map\n");
    say("  %d stations wide, %d values each, %d workers available\n",
        WIDTH, VALUES_PER, workers);
    say("  peak concurrent boxes   %d of %d possible\n", (int)busy_peak, workers);
    say("  tasks completed         %ld in %.3f s (%.0f/s)\n",
        (long)wide_done, elapsed, wide_done / elapsed);
    say("  nobody arranged this parallelism; the map is simply wide\n\n");

    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: concurrent invocations of one station.                  */
/* ------------------------------------------------------------------ */

static _Atomic int station_now;
static _Atomic int station_peak;
static _Atomic long station_sum;

/* {{{ crowd_work and shim — hand shim, deleted by issue 302 */
static int crowd_work(int x)
{
    return x + (burn(120) & 1);
}

static void crowd_work__call(task_t *t)
{
    int entered = ++station_now;
    int peak = station_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&station_peak, &peak, entered))
        ;
    int x = *(int *)t->in[0];
    int r = crowd_work(x);
    station_sum += r;
    station_now--;
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

/* {{{ scene_one_station_crowd() */
static void scene_one_station_crowd(void)
{
    enum { VALUES = 500 };
    map_t *m = map_create(1);
    int one_int[1] = { sizeof(int) };
    map_place(m, 0, crowd_work__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_start(m, 0);

    station_now = 0;
    station_peak = 0;
    station_sum = 0;

    for (int v = 0; v < VALUES; v++)
        map_deliver_value(m, 0, 0, &v);

    pool_release(m->pool);
    pool_join(m->pool);

    long expected_floor = (long)VALUES * (VALUES - 1) / 2;
    say("scene 2 — one station, many bodies inside it\n");
    say("  %d values fed to a single station\n", VALUES);
    say("  peak simultaneous invocations  %d\n", (int)station_peak);
    say("  checksum intact                %s\n",
        (station_sum >= expected_floor) ? "yes" : "NO — values were lost");
    say("  overlap is safe because claimed values are copies; the box\n");
    say("  paid for it by being forbidden to remember anything\n\n");

    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: buffer growth under mismatched rates.                 */
/* ------------------------------------------------------------------ */

static _Atomic long slow_consumed;

/* {{{ fast_relay / slow_eater and shims — hand shims, deleted by 302 */
static int fast_relay(int x)
{
    return x + burn(2);
}

static void fast_relay__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = fast_relay(x);
    memcpy(t->out, &r, sizeof r);
}

static void slow_eater(int x)
{
    (void)x;
    burn(200);
    slow_consumed++;
}

static void slow_eater__call(task_t *t)
{
    slow_eater(*(int *)t->in[0]);
}
/* }}} */

/* {{{ pair_count and the pairing box — hand shim, deleted by 302 */
static _Atomic long pairs_made;

static int pair_up(int value, int ticket)
{
    (void)ticket;
    pairs_made++;
    return value;
}

static void pair_up__call(task_t *t)
{
    int value = *(int *)t->in[0];
    int ticket = *(int *)t->in[1];
    int r = pair_up(value, ticket);
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

/* {{{ scene_growth_mismatch() */
/*
 * A discovery made while building this scene, recorded in the
 * first-pass report: a single-input station can never accumulate a
 * backlog in its slot, because every write completes its input set
 * and is claimed on the spot — its backlog piles up as tasks in the
 * pool's ring instead. Slot buffers absorb a different mismatch:
 * a multi-input station fed unevenly, values on one side waiting
 * for their siblings on the other. This scene shows both piles,
 * each where it actually forms.
 */
static void scene_growth_mismatch(void)
{
    enum { VALUES = 400 };
    int one_int[1] = { sizeof(int) };

    /* First: the slow single-input consumer. The backlog lands in
     * the pool's task ring, and the slot stays shallow. */
    map_t *m = map_create(2);
    map_place(m, 0, fast_relay__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 1, slow_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_start(m, 0);

    slow_consumed = 0;
    for (int v = 0; v < VALUES; v++)
        map_deliver_value(m, 0, 0, &v);

    pool_release(m->pool);
    pool_join(m->pool);

    int q_capacity, q_high, q_growths;
    pool_queue_stats(m->pool, &q_capacity, &q_high, &q_growths);
    slot_t *slow_slot = &m->stations[1].slots[0];

    say("scene 3 — a producer outrunning its consumer\n");
    say("  %d values through a fast relay into a slow single-input eater:\n",
        VALUES);
    say("    slot high water   %d  (a one-input slot claims every write at once)\n",
        slow_slot->high_water);
    say("    pool ring         %d growths, high water %d, %d cells at the end\n",
        q_growths, q_high, q_capacity);
    say("  the backlog of a slow single-input station lives in the task\n");
    say("  ring, not the slot — a finding the design docs did not state\n");
    if (slow_consumed != VALUES) {
        fprintf(stderr, "the slow eater consumed %ld of %d\n",
                (long)slow_consumed, VALUES);
        exit(1);
    }
    map_destroy(m);

    /* Second: the mismatch slots do absorb — a pairing station fed
     * all its values on one side before any tickets arrive on the
     * other. The waiting side must hold everything. */
    map_t *m2 = map_create(1);
    int two_ints[2] = { sizeof(int), sizeof(int) };
    map_place(m2, 0, pair_up__call, STATION_PLAIN, 2, two_ints, sizeof(int));
    map_start(m2, 0);

    pairs_made = 0;
    for (int v = 0; v < VALUES; v++)
        map_deliver_value(m2, 0, 0, &v);

    slot_t *waiting = &m2->stations[0].slots[0];
    say("  %d values delivered to one side of a pairing station, none yet\n",
        VALUES);
    say("  to the other:\n");
    say("    waiting slot      %d growths, capacity %d, high water %d\n",
        waiting->growths, waiting->capacity, waiting->high_water);

    for (int v = 0; v < VALUES; v++)
        map_deliver_value(m2, 0, 1, &v);
    pool_release(m2->pool);
    pool_join(m2->pool);

    if (pairs_made != VALUES) {
        fprintf(stderr, "pairing made %ld of %d pairs\n",
                (long)pairs_made, VALUES);
        exit(1);
    }
    say("  memory absorbed the imbalance; phase 7 will say this out loud\n\n");
    map_destroy(m2);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the cost of fan-out.                                   */
/* ------------------------------------------------------------------ */

static _Atomic long fan_received;

/* {{{ fan boxes and shims — hand shims, deleted by issue 302 */
static int fan_source(int x)
{
    return x + burn(20);
}

static void fan_source__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = fan_source(x);
    memcpy(t->out, &r, sizeof r);
}

static void fan_sink(int x)
{
    (void)x;
    fan_received++;
}

static void fan_sink__call(task_t *t)
{
    fan_sink(*(int *)t->in[0]);
}
/* }}} */

/* {{{ scene_fan_cost() */
static void scene_fan_cost(void)
{
    enum { VALUES = 300 };
    static const int widths[] = { 1, 10, 100 };

    say("scene 4 — what one worker pays to fan out\n");
    say("  %d values through one source, wired to N counting sinks\n", VALUES);
    say("  N      total time    per source-value\n");

    for (unsigned i = 0; i < sizeof widths / sizeof widths[0]; i++) {
        int n = widths[i];
        map_t *m = map_create(1 + n);
        int one_int[1] = { sizeof(int) };
        map_place(m, 0, fan_source__call, STATION_PLAIN, 1, one_int, sizeof(int));
        for (int s = 0; s < n; s++) {
            map_place(m, 1 + s, fan_sink__call, STATION_PLAIN, 1, one_int, 0);
            map_connect(m, 0, 0, 1 + s, 0);
        }
        map_start(m, 0);

        fan_received = 0;
        for (int v = 0; v < VALUES; v++)
            map_deliver_value(m, 0, 0, &v);

        double before = now_seconds();
        pool_release(m->pool);
        pool_join(m->pool);
        double elapsed = now_seconds() - before;

        if (fan_received != (long)VALUES * n) {
            fprintf(stderr, "fan of %d delivered %ld of %ld\n",
                    n, (long)fan_received, (long)VALUES * n);
            exit(1);
        }
        say("  %-5d  %8.3f ms   %8.3f us\n",
            n, elapsed * 1e3, elapsed * 1e6 / VALUES);
        map_destroy(m);
    }
    say("  the delivering worker does every lock-write-check itself;\n");
    say("  each one may unblock a station, so the time buys parallelism\n\n");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene five: watching values pool behind a slow station.            */
/* ------------------------------------------------------------------ */

/* {{{ pipeline boxes and shims — hand shims, deleted by issue 302 */
static int stage_quick(int x)
{
    return x + burn(30);
}

static void stage_quick__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = stage_quick(x);
    memcpy(t->out, &r, sizeof r);
}

/* The gate pairs each value with a ticket; values wait for tickets. */
static int gate(int value, int ticket)
{
    (void)ticket;
    return value + burn(50);
}

static void gate__call(task_t *t)
{
    int value = *(int *)t->in[0];
    int ticket = *(int *)t->in[1];
    int r = gate(value, ticket);
    memcpy(t->out, &r, sizeof r);
}

static _Atomic long drain_count;

static void stage_drain(int x)
{
    (void)x;
    drain_count++;
}

static void stage_drain__call(task_t *t)
{
    stage_drain(*(int *)t->in[0]);
}
/* }}} */

/* {{{ scene_watch_backpressure() */
/*
 * The visible pile: a quick stage floods the gate's value side, and
 * tickets arrive from outside at a metered rate. Values queue in the
 * gate's slot — the slot-buffer kind of backpressure — and the pile
 * shrinks on camera as tickets admit them through.
 */
static void scene_watch_backpressure(void)
{
    enum { VALUES = 600, SAMPLES = 12, TICKETS_PER_SAMPLE = VALUES / SAMPLES };
    map_t *m = map_create(3);
    int one_int[1] = { sizeof(int) };
    int two_ints[2] = { sizeof(int), sizeof(int) };
    map_place(m, 0, stage_quick__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 1, gate__call, STATION_PLAIN, 2, two_ints, sizeof(int));
    map_place(m, 2, stage_drain__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 1, 0, 2, 0);
    map_start(m, 0);

    drain_count = 0;
    for (int v = 0; v < VALUES; v++)
        map_deliver_value(m, 0, 0, &v);

    say("scene 5 — backpressure, watched\n");
    say("  quick -> GATE -> drain: %d values flood the gate, tickets\n", VALUES);
    say("  trickle in from outside; each # is twenty values waiting\n");

    pool_submitter_register(m->pool);
    pool_release(m->pool);

    for (int s = 0; s < SAMPLES; s++) {
        int depth = map_slot_depth(m, 1, 0);
        say("  t+%3dms  waiting %4d  ", s * 30, depth);
        for (int h = 0; h < depth / 20 && h < 40; h++)
            say("#");
        say("\n");
        for (int k = 0; k < TICKETS_PER_SAMPLE; k++) {
            int ticket = s * TICKETS_PER_SAMPLE + k;
            map_deliver_value(m, 1, 1, &ticket);
        }
        usleep(30000);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    slot_t *gate_slot = &m->stations[1].slots[0];
    say("  drained %ld of %d; the waiting side grew %d times to %d cells\n",
        (long)drain_count, VALUES, gate_slot->growths, gate_slot->capacity);
    say("  backpressure made visible — the same reading phase 7 automates\n\n");

    map_destroy(m);
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc > 1) {
        char path[4096];
        snprintf(path, sizeof path,
                 "%s/tmp/shared-memory/phase-2-graph-report.txt", argv[1]);
        report = fopen(path, "w");
        if (!report)
            fprintf(stderr, "could not open %s; reporting to screen only\n", path);
    }

    say("=== phase 2 demo: a graph that runs itself ===\n\n");
    scene_occupancy();
    scene_one_station_crowd();
    scene_growth_mismatch();
    scene_fan_cost();
    scene_watch_backpressure();
    say("=== nobody scheduled any of this; the wiring did ===\n");

    if (report)
        fclose(report);
    return 0;
}
