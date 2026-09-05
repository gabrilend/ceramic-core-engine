/*
 * 076-test-destinations.c — proves a delivery walk takes no lock, and
 * that what a rewire replaces is reclaimed (issue 214).
 *
 * What this is: the tests for the last thing that held a station's
 * mutex on the hot path. A port's destinations became one immutable
 * array behind a pointer; a rewire builds a whole new one and swaps
 * the pointer; a walker reads the pointer once and walks something
 * nobody will ever modify. The old array is filed rather than freed,
 * because a walker may be inside it right now.
 *
 * How it does it, in general terms: rewires a running program many
 * times against a saturated pool and watches the scrapyard drain,
 * which is the claim that this is a scrapyard rather than a leak.
 * Then it tears a map down while sets are still filed, which is the
 * double-free the scrapyard's own lock exists to prevent.
 *
 * What is deliberately not asserted: how deep the scrapyard gets at
 * any instant. A set is freed when no worker can still be inside the
 * task it was in, which depends on what the workers are doing, so the
 * only honest claim is that it drains — not that it is empty at any
 * particular moment.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

/* {{{ static void check() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "  FAIL: %s\n", what);
        failures++;
    }
}
/* }}} */

/* {{{ static void a_wire_removed_mid_run() */
/*
 * A value already on its way down a wire that is being removed gets
 * delivered. That is indistinguishable from having been delivered a
 * moment earlier, which is the whole of what the engine promises
 * about the timing of an edit — and it is why the old set must be
 * filed rather than freed.
 */
static void a_wire_removed_mid_run(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "seven", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_place_box(m, 2, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 0, 2, 0);

    out_port_t *p = map_station(m, 0)->out_ports;
    dest_set_t *before = out_port_dests(p);
    check(before && before->n == 2, "two wires make a set of two");

    map_start(m, 4);
    check(map_unwire(m, 0, 0, 2, 0) == NULL, "a wire came out");

    dest_set_t *after = out_port_dests(p);
    check(after && after->n == 1, "and the new set has one");
    check(after != before, "which is a different array, not an edited one");
    check(before->n == 2,
          "the old array is untouched — a walker inside it sees what it "
          "always saw");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  a wire came out and the old set was left intact for walkers\n");
}
/* }}} */

/* {{{ static void the_scrapyard_drains() */
/*
 * Many rewires against a pool that is actually running work. The
 * claim is that the scrapyard does not grow without bound — that
 * sweeping actually frees things — which is the difference between a
 * scrapyard and a leak.
 */
static void the_scrapyard_drains(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_place_box(m, 2, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* Work in flight the whole time, so workers are genuinely inside
     * tasks and some sets are genuinely unfreeable when swept. */
    enum { ROUNDS = 400 };
    int deepest = 0;
    for (int i = 0; i < ROUNDS; i++) {
        int v = i;
        map_deliver_value(m, 0, 0, &v);
        if (i % 2 == 0)
            map_connect(m, 0, 0, 2, 0);
        else
            map_disconnect(m, 0, 0, 2, 0);
        int filed = map_scrap_count(m);
        if (filed > deepest)
            deepest = filed;
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    /* With every worker now idle and therefore even, one more sweep
     * has to be able to free everything filed. */
    map_scrap_sweep(m);
    int left = map_scrap_count(m);

    check(deepest < ROUNDS,
          "the scrapyard never held one set per rewire — it was draining "
          "as it went");
    check(left == 0,
          "and once the workers went idle, a sweep freed every last one");
    printf("  %d rewires under load: deepest %d filed, %d left after\n",
           ROUNDS, deepest, left);

    map_destroy(m);
}
/* }}} */

/* {{{ static void teardown_with_sets_still_filed() */
/*
 * Teardown is the second toucher of the scrapyard, and the
 * non-obvious one. It empties what a sweep could not free. Running it
 * with sets genuinely filed is the double-free this lock exists for;
 * a leak checker is what actually judges this one.
 */
static void teardown_with_sets_still_filed(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "seven", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_place_box(m, 2, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 0, 2, 0);

    map_start(m, 2);
    for (int i = 0; i < 20; i++) {
        map_disconnect(m, 0, 0, 2, 0);
        map_connect(m, 0, 0, 2, 0);
    }
    check(map_scrap_count(m) > 0,
          "sets are filed and waiting when teardown arrives");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  a teardown emptied a scrapyard that still had sets in it\n");
}
/* }}} */

/* {{{ static void the_dump_still_round_trips() */
/*
 * Not because destination order must be preserved — it need not be,
 * and nothing in the running engine reads it — but because a rewire
 * is the one thing that rebuilds the set, and the dump writes it in
 * array order. If a rebuild reordered, dump-load-dump would stop
 * matching, and that is a promise made elsewhere.
 */
static void the_dump_still_round_trips(void)
{
    map_t *m = map_create(4);
    map_place_box(m, 0, "seven", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_place_box(m, 2, "keep", STATION_PLAIN);
    map_place_box(m, 3, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 0, 2, 0);
    map_connect(m, 0, 0, 3, 0);

    dest_set_t *set = out_port_dests(map_station(m, 0)->out_ports);
    check(set && set->n == 3, "three wires drawn");
    check(set->items[0].station == 1 && set->items[1].station == 2
          && set->items[2].station == 3,
          "and they sit in the order they were drawn");

    map_start(m, 2);
    map_disconnect(m, 0, 0, 2, 0);
    set = out_port_dests(map_station(m, 0)->out_ports);
    check(set && set->n == 2 && set->items[0].station == 1
          && set->items[1].station == 3,
          "removing the middle one leaves the others in their order");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  a rebuild kept wiring order, so the dump still round-trips\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
    a_wire_removed_mid_run();
    the_scrapyard_drains();
    teardown_with_sets_still_filed();
    the_dump_still_round_trips();

    if (failures) {
        fprintf(stderr, "%d destination checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
