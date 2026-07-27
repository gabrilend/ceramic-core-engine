/*
 * 022-test-slots.c — proves ring-buffer slots (issues 202, 203).
 *
 * What this is: the test that values of any size go into a slot and
 * come back out byte-identical and in order, across buffer growth
 * that begins from a wrapped ring — the state where the unwrap copy
 * would be wrong first.
 *
 * How it does it, in general terms: a station pairing a large struct
 * with a small integer is fed through the public delivery call. The
 * ring is first wrapped by running a few pairs through, then flooded
 * on one side only so it must grow while wrapped, then completed on
 * the other side so every pair fires. The box itself checks that the
 * struct's fields agree with the integer it was paired with — a pair
 * formed out of order or a byte moved in the copy fails the check.
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

static _Atomic int pairs_checked;
static _Atomic int pairs_wrong;

/* {{{ inspect() and its hand shim */
/*
 * The box: confirms a parcel and its companion serial agree. Both
 * values were manufactured from one seed, so any disagreement means
 * the engine tore, reordered, or mispaired them.
 * Hand shim in generator shape; deleted by issue 302.
 */
static int inspect(parcel_t p, int serial)
{
    char expect[24];
    snprintf(expect, sizeof expect, "parcel-%d", serial);
    int good = p.serial == serial
            && p.weight == serial * 1.5
            && strcmp(p.label, expect) == 0;
    pairs_checked++;
    if (!good)
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
    enum { WARMUP = 3, FLOOD = 300 };

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

    /* Now complete the pairs. Serial i must meet parcel i — first
     * in, first out, across every doubling. */
    for (int i = 0; i < FLOOD; i++) {
        int serial = WARMUP + i;
        map_deliver_value(m, 0, 1, &serial);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    if (pairs_checked != WARMUP + FLOOD) {
        fprintf(stderr, "%d pairs fired, expected %d\n",
                (int)pairs_checked, WARMUP + FLOOD);
        exit(1);
    }
    if (pairs_wrong != 0) {
        fprintf(stderr, "%d pairs came through torn or out of order\n",
                (int)pairs_wrong);
        exit(1);
    }

    int growths = m->stations[0].slots[0].growths;
    int high_water = m->stations[0].slots[0].high_water;
    map_destroy(m);
    printf("  %d struct+int pairs intact across %d wrapped growths (high water %d)\n",
           WARMUP + FLOOD, growths, high_water);
    return 0;
}
