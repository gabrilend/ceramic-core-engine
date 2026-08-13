/*
 * 022-test-slots.c — proves ring-buffer slots (issues 202, 203).
 *
 * What this is: the test that values of any size go into a slot and
 * come back out byte-identical, and that none is lost or doubled,
 * across buffer growth that begins from a wrapped ring — the state
 * where the unwrap copy would be wrong first.
 *
 * How it does it, in general terms: a station pairing a large struct
 * with a small integer is fed through the public delivery call. The
 * ring is first wrapped by running a few pairs through, then flooded
 * on one side only so it must grow while wrapped, then completed on
 * the other side so every pair fires. The box checks that each struct
 * is internally consistent — its three fields were manufactured from
 * one seed, so a byte moved in the copy makes them disagree with each
 * other — and the tallies afterwards check that every value delivered
 * on each side arrived exactly once.
 *
 * **This test used to check the pairing too**, and no longer does.
 * It asserted that serial *i* met parcel *i*: first in, first out,
 * across every doubling. That promise is withdrawn — see the
 * ordering entry in docs/058-guarantees.md, which explains why an
 * order that was arbitrary to begin with was not worth the cost of
 * keeping. A reader is about to scan for a usable cell rather than
 * compute where the oldest one must be (issue 210d), and positional
 * pairing goes with it.
 *
 * What is left is stronger than what went, and it is worth being
 * clear about which is which. Tearing is the failure that matters:
 * a value assembled from two different writes is silent corruption,
 * and the struct's self-consistency catches it. Losing or doubling a
 * value is the other one, and the tallies catch that. Neither of
 * those was ever the ordering.
 */
#include "018-station.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bigger than a machine word, with padding, on purpose. The label is
 * sized to hold "parcel-" plus any int the compiler can imagine, so
 * the format-truncation check stays quiet honestly. */
typedef struct parcel {
    double  weight;
    char    label[24];
    int     serial;
} parcel_t;

enum { WARMUP = 3, FLOOD = 300, TOTAL = WARMUP + FLOOD };

static _Atomic int pairs_checked;
static _Atomic int pairs_wrong;
/* Which parcel serials and which integers came out, so that losing a
 * value or serving one twice is caught without anything being said
 * about which met which. */
static _Atomic int parcels_seen[TOTAL];
static _Atomic int integers_seen[TOTAL];

/* {{{ inspect() and its hand shim */
/*
 * The box: confirms a parcel is internally consistent, and records
 * both of the values it was handed.
 *
 * All three of a parcel's fields are manufactured from one seed, so
 * they agree with each other or the engine tore the struct — which is
 * the failure worth catching, and the one that stays catchable now
 * that the parcel need not correspond to the integer beside it.
 * Hand shim in generator shape; deleted by issue 302.
 */
static int inspect(parcel_t p, int serial)
{
    char expect[24];
    snprintf(expect, sizeof expect, "parcel-%d", p.serial);
    int good = p.serial >= 0 && p.serial < TOTAL
            && p.weight == p.serial * 1.5
            && strcmp(p.label, expect) == 0;
    pairs_checked++;
    if (!good) {
        pairs_wrong++;
        return good;
    }
    parcels_seen[p.serial]++;
    if (serial >= 0 && serial < TOTAL)
        integers_seen[serial]++;
    else
        pairs_wrong++;
    return good;
}

static void inspect__call(task_t *t)
{
    parcel_t p;
    memcpy(&p, t->in[0], sizeof p);
    int serial = *(int *)t->in[1];
    int r = inspect(p, serial);
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

/* {{{ make_parcel() */
static parcel_t make_parcel(int serial)
{
    parcel_t p;
    memset(&p, 0, sizeof p);
    p.weight = serial * 1.5;
    snprintf(p.label, sizeof p.label, "parcel-%d", serial);
    p.serial = serial;
    return p;
}
/* }}} */

int main(void)
{
    map_t *m = map_create(1);
    int sizes[2] = { sizeof(parcel_t), sizeof(int) };
    map_place(m, 0, inspect__call, STATION_PLAIN, 2, sizes, sizeof(int));
    map_start(m, 4);

    /* Warm-up pairs, delivered before release, wrap the ring: head
     * and tail advance together away from zero. */
    for (int i = 0; i < WARMUP; i++) {
        parcel_t p = make_parcel(i);
        map_deliver_value(m, 0, 0, &p);
        map_deliver_value(m, 0, 1, &i);
    }

    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* Flood the parcel side only: the buffer must grow, starting
     * from its wrapped state, while the serial side stays empty. */
    for (int i = 0; i < FLOOD; i++) {
        parcel_t p = make_parcel(WARMUP + i);
        map_deliver_value(m, 0, 0, &p);
    }
    if (m->stations[0].slots[0].growths < 3) {
        fprintf(stderr, "expected several growths from the flood, saw %d\n",
                m->stations[0].slots[0].growths);
        exit(1);
    }

    /* Now complete the pairs. Which serial meets which parcel is not
     * promised and is not checked; that every one of them is served
     * exactly once is both. */
    for (int i = 0; i < FLOOD; i++) {
        int serial = WARMUP + i;
        map_deliver_value(m, 0, 1, &serial);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    if (pairs_checked != TOTAL) {
        fprintf(stderr, "%d pairs fired, expected %d\n",
                (int)pairs_checked, TOTAL);
        exit(1);
    }
    if (pairs_wrong != 0) {
        fprintf(stderr, "%d values came through torn\n", (int)pairs_wrong);
        exit(1);
    }
    for (int i = 0; i < TOTAL; i++) {
        if (parcels_seen[i] != 1) {
            fprintf(stderr, "parcel %d came out %d times\n",
                    i, (int)parcels_seen[i]);
            exit(1);
        }
        if (integers_seen[i] != 1) {
            fprintf(stderr, "integer %d came out %d times\n",
                    i, (int)integers_seen[i]);
            exit(1);
        }
    }

    int growths = m->stations[0].slots[0].growths;
    int high_water = m->stations[0].slots[0].high_water;
    map_destroy(m);
    printf("  %d struct+int pairs, none torn and none lost, across %d "
           "wrapped growths (high water %d)\n", TOTAL, growths, high_water);
    return 0;
}
