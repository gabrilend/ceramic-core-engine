/*
 * 077-test-removal.c — proves a station can be taken out and its place
 * reused (issue 216), and that a box nobody places can be unloaded
 * (issue 310, step 8).
 *
 * What this is: the tests for the two things that were waiting on a
 * way to know whether a worker might still be inside something. That
 * way now exists — a per-worker counter, odd inside a task and even
 * outside it — and both of these are built on it.
 *
 * How it does it, in general terms: removes a station from a running
 * program with work in flight and checks that nothing tears, that
 * every wire naming it went with it, and that the next station placed
 * takes its empty place. The wires are the interesting part: cutting
 * them **first** is what makes reusing the place safe without a
 * version number on every wire, and this is where that claim is
 * checked.
 */
#include "018-station.h"
#include "026-registry.h"
#include "049-observe.h"
#include "073-latebox.h"

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

/* {{{ static int wires_naming() */
/* How many destination records anywhere in the map name a station. */
static int wires_naming(map_t *m, int station)
{
    int n = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        for (port_t *p = s->ports; p; p = p->next) {
            dest_set_t *set = port_dests(p);
            for (int d = 0; set && d < set->n; d++)
                if (set->items[d].station == station)
                    n++;
        }
    }
    return n;
}
/* }}} */

/* {{{ static void removal_cuts_every_wire_first() */
/*
 * Three stations feeding one, which is then removed. All three wires
 * have to go, and they have to go **before** the station does — that
 * ordering is the whole reason a reused place needs no version tag.
 */
static void removal_cuts_every_wire_first(void)
{
    map_t *m = map_create(4);
    map_place_box(m, 0, "seven", STATION_PLAIN);
    map_place_box(m, 1, "seven", STATION_PLAIN);
    map_place_box(m, 2, "seven", STATION_PLAIN);
    map_place_box(m, 3, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 3, 0);
    map_connect(m, 1, 0, 3, 0);
    map_connect(m, 2, 0, 3, 0);

    check(wires_naming(m, 3) == 3, "three wires name the doomed station");

    map_start(m, 4);
    check(map_remove_station(m, 3) == 0, "it came out");
    check(wires_naming(m, 3) == 0,
          "and every wire that named it went with it — nothing stale is "
          "left to be followed");
    /* Not free yet: its record still carries what a task being built
     * from it needs, and the sweep that clears them is what frees the
     * place. Once the workers are idle, one sweep does it. */
    pool_release(m->pool);
    pool_join(m->pool);
    map_scrap_sweep(m);
    check(map_station(m, 3)->call == NULL,
          "and once nothing could be inside it, the place came free");

    map_destroy(m);
    printf("  three wires named a station; removing it cut all three\n");
}
/* }}} */

/* {{{ static void the_place_is_reused() */
/*
 * The point of removing anything: a program that adds and removes
 * forever reaches a steady size rather than climbing. A place freed
 * is a place the next station takes, and because no wire can name it
 * any more, the new station receives only what is wired to *it*.
 */
static void the_place_is_reused(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_place_box(m, 2, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);

    map_start(m, 4);
    check(map_remove_station(m, 1) == 0, "the wired-to station came out");
    check(wires_naming(m, 1) == 0, "so nothing names its place");

    /* The place comes free when nobody can be inside a task built
     * from it. With nothing running yet, one sweep is enough. */
    map_scrap_sweep(m);
    check(map_station(m, 1)->call == NULL, "and the place came free");

    /* A different box takes the same place. Under a design that
     * reused places without cutting wires first, the old wire would
     * still point here and this station would receive values nobody
     * sent it. */
    map_place_box(m, 1, "keep", STATION_PLAIN);
    check(map_station(m, 1)->call != NULL, "and a new station took it");

    map_connect(m, 0, 0, 2, 0);
    int five = 5;
    map_deliver_value(m, 0, 0, &five);
    pool_release(m->pool);
    pool_join(m->pool);

    check(atomic_load(&map_station(m, 2)->runs) == 1,
          "the value went where it was wired");
    check(atomic_load(&map_station(m, 1)->runs) == 0,
          "and the reused place received nothing, because no wire named it");

    map_destroy(m);
    printf("  a freed place was reused and received only its own wires\n");
}
/* }}} */

/* {{{ static void removal_under_load() */
/*
 * Removing a station while the pool is genuinely busy. Its ports and
 * buffers go to the scrapyard rather than being freed, because a
 * worker may be running a task from it right now and will touch them
 * when it finishes. A leak checker is what really judges this one.
 */
static void removal_under_load(void)
{
    map_t *m = map_create(6);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    for (int i = 1; i < 6; i++)
        map_place_box(m, i, "keep", STATION_PLAIN);
    for (int i = 1; i < 6; i++)
        map_connect(m, 0, 0, i, 0);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /*
     * Remove and re-place under load. A removed place does not come
     * free until a sweep says nobody can be inside a task built from
     * it, so this sweeps before trying to re-place — which is the
     * honest rhythm for a program that reshapes itself while running.
     */
    int removed = 0, replaced = 0;
    for (int round = 0; round < 200; round++) {
        int v = round;
        map_deliver_value(m, 0, 0, &v);

        int victim = 1 + (round % 5);
        station_t *st = map_station(m, victim);
        if (st->call && !atomic_load(&st->removed)) {
            if (map_remove_station(m, victim) == 0)
                removed++;
        } else {
            map_scrap_sweep(m);
            if (!st->call) {
                map_place_box(m, victim, "keep", STATION_PLAIN);
                map_connect(m, 0, 0, victim, 0);
                replaced++;
            }
        }
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_scrap_sweep(m);

    check(removed > 0 && replaced > 0,
          "stations genuinely came out and went back in during the run");
    check(map_scrap_count(m) == 0,
          "with the workers idle, a sweep freed everything removal filed");
    printf("  %d removals and %d placements under load, scrapyard drained\n",
           removed, replaced);

    map_destroy(m);
}
/* }}} */

/* {{{ static void an_unplaced_box_unloads() */
/*
 * Issue 310's last step. A box compiled into a running program and
 * never placed can be unloaded, freeing the library it came in — and
 * it uses the same counter this file's other tests rest on, because
 * "might a worker be inside this code" and "might a worker be inside
 * this station" are one question wearing two hats.
 */
static void an_unplaced_box_unloads(void)
{
    static const char source[] =
        "int never_placed(int x)\n{\n    return x + 1;\n}\n";

    check(registry_compile_source(source) == 1, "a box was compiled in");
    check(registry_find("never_placed") != NULL, "and can be found");

    map_t *m = map_create(2);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);
    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* Work in flight, so the pool is saturated while the unload
     * happens — which is the case the counter exists for. */
    for (int i = 0; i < 100; i++) {
        int v = i;
        map_deliver_value(m, 0, 0, &v);
    }

    int rc = registry_unload_box(m, "never_placed");
    check(rc == 0, "a box no station places was unloaded");
    check(registry_find("never_placed") == NULL,
          "and can no longer be found by name");

    /* A box that IS placed must be refused, because unloading its
     * code while a station names it is exactly the crash this is
     * built to avoid. */
    check(registry_unload_box(m, "double_it") != 0,
          "a box a station places was refused");

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  an unplaced box unloaded while the pool was busy\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
    removal_cuts_every_wire_first();
    the_place_is_reused();
    removal_under_load();
    an_unplaced_box_unloads();

    if (failures) {
        fprintf(stderr, "%d removal checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
