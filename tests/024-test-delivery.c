/*
 * 024-test-delivery.c — proves the delivery walk and the task struct
 * (issues 205, 206).
 *
 * What this is: the test that a value travels — down a chain of
 * stations, out across a fan, into a sink that returns nothing —
 * and that what travels is always a private copy.
 *
 * How it does it, in general terms: three small maps. A chain map
 * increments a value twice and records it, proving propagation. A
 * fan map copies one output to twenty counters, proving one port
 * with many destinations. A parcel map sends a struct through a
 * relay into a void sink, proving both that sinks need no engine
 * support and that the bytes arrive identical after two hops of
 * copying — the task-struct-as-copy property in action.
 */
#include "018-station.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* All shims below are hand-written in the exact shape the generator
 * will emit, and are deleted by issue 302. Every cast in them is an
 * unchecked human promise — which is the reason phase 3 exists. */

/* {{{ chain: increment, increment, record */
static _Atomic int chain_middle_ran;
static _Atomic long chain_total;
static _Atomic int chain_records;

static int increment(int x) { return x + 1; }

static void increment__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = increment(x);
    chain_middle_ran++;
    memcpy(t->out, &r, sizeof r);
}

static void record(int x)
{
    chain_total += x;
    chain_records++;
}

static void record__call(task_t *t)
{
    record(*(int *)t->in[0]);
    /* A sink writes nothing: the box returns void, so there is no
     * out field to fill and delivery will skip this task. */
}

static void test_chain(void)
{
    enum { VALUES = 50 };
    map_t *m = map_create(3);
    int one_int[1] = { sizeof(int) };
    map_place(m, 0, increment__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 1, increment__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 2, record__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 1, 0, 2, 0);
    map_start(m, 4);

    chain_middle_ran = 0;
    chain_total = 0;
    chain_records = 0;

    /* Seed before release: the phase 2 stand-in for the seed sweep. */
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    long expected = 0;
    for (int i = 0; i < VALUES; i++)
        expected += i + 2;

    if (chain_records != VALUES) {
        fprintf(stderr, "chain end saw %d of %d values\n",
                (int)chain_records, VALUES);
        exit(1);
    }
    if (chain_total != expected) {
        fprintf(stderr, "chain total %ld, expected %ld\n",
                (long)chain_total, expected);
        exit(1);
    }
    if (chain_middle_ran != VALUES * 2) {
        fprintf(stderr, "the two increment stations ran %d times, expected %d\n",
                (int)chain_middle_ran, VALUES * 2);
        exit(1);
    }

    map_destroy(m);
    printf("  a three-station chain carried %d values end to end\n", VALUES);
}
/* }}} */

/* {{{ fan-out: one port, twenty destinations */
static _Atomic int fan_hits;

static void tally(int x)
{
    (void)x;
    fan_hits++;
}

static void tally__call(task_t *t)
{
    tally(*(int *)t->in[0]);
}

static void test_fan_out(void)
{
    enum { SINKS = 20, VALUES = 10 };
    map_t *m = map_create(1 + SINKS);
    int one_int[1] = { sizeof(int) };
    map_place(m, 0, increment__call, STATION_PLAIN, 1, one_int, sizeof(int));
    for (int i = 0; i < SINKS; i++) {
        map_place(m, 1 + i, tally__call, STATION_PLAIN, 1, one_int, 0);
        /* Same port every time: fan-out is one port with many
         * destinations, not many ports. */
        map_connect(m, 0, 0, 1 + i, 0);
    }
    map_start(m, 4);

    fan_hits = 0;
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    if (fan_hits != SINKS * VALUES) {
        fprintf(stderr, "fan-out delivered %d of %d copies\n",
                (int)fan_hits, SINKS * VALUES);
        exit(1);
    }

    map_destroy(m);
    printf("  one port fanned %d values out to %d destinations\n",
           VALUES, SINKS);
}
/* }}} */

/* {{{ parcels through a relay into a void sink */
typedef struct parcel {
    double weight;
    char   label[24];
    int    serial;
} parcel_t;

static _Atomic int parcels_seen;
static _Atomic int parcels_wrong;

static parcel_t relay(parcel_t p) { return p; }

static void relay__call(task_t *t)
{
    parcel_t p;
    memcpy(&p, t->in[0], sizeof p);
    parcel_t r = relay(p);
    memcpy(t->out, &r, sizeof r);
}

static void receive(parcel_t p)
{
    char expect[24];
    snprintf(expect, sizeof expect, "parcel-%d", p.serial);
    parcels_seen++;
    if (p.weight != p.serial * 2.5 || strcmp(p.label, expect) != 0)
        parcels_wrong++;
}

static void receive__call(task_t *t)
{
    parcel_t p;
    memcpy(&p, t->in[0], sizeof p);
    receive(p);
}

static void test_struct_through_sink(void)
{
    enum { PARCELS = 40 };
    map_t *m = map_create(2);
    int one_parcel[1] = { sizeof(parcel_t) };
    map_place(m, 0, relay__call, STATION_PLAIN, 1, one_parcel, sizeof(parcel_t));
    map_place(m, 1, receive__call, STATION_PLAIN, 1, one_parcel, 0);
    map_connect(m, 0, 0, 1, 0);
    map_start(m, 4);

    parcels_seen = 0;
    parcels_wrong = 0;
    for (int i = 0; i < PARCELS; i++) {
        parcel_t p;
        memset(&p, 0, sizeof p);
        p.weight = i * 2.5;
        snprintf(p.label, sizeof p.label, "parcel-%d", i);
        p.serial = i;
        map_deliver_value(m, 0, 0, &p);
    }
    pool_release(m->pool);
    pool_join(m->pool);

    if (parcels_seen != PARCELS) {
        fprintf(stderr, "sink received %d of %d parcels\n",
                (int)parcels_seen, PARCELS);
        exit(1);
    }
    if (parcels_wrong != 0) {
        fprintf(stderr, "%d parcels arrived altered\n", (int)parcels_wrong);
        exit(1);
    }

    map_destroy(m);
    printf("  %d structs crossed two hops byte-identical into a void sink\n",
           PARCELS);
}
/* }}} */

int main(void)
{
    test_chain();
    test_fan_out();
    test_struct_through_sink();
    return 0;
}
