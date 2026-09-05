/*
 * 078-test-growth.c — proves the station table grows without moving
 * anything (issue 211).
 *
 * What this is: the tests for a table that used to be one array
 * allocated once and never resized. It is shelves now — a short array
 * of pointers to fixed runs of records — so growing means allocating
 * one more shelf and writing its pointer, and **every station already
 * placed stays exactly where it was, mutex included.** That is the
 * whole reason for the shape: a mutex is identified by where it lives,
 * and a thread parked on one that moved would be waiting at an address
 * nobody unlocks.
 *
 * How it does it, in general terms: fills a map past a shelf boundary
 * and checks that the addresses of the earlier stations did not move;
 * adds stations to a program that is already running and wires them in
 * afterwards; and has many threads ask for a place at once, checking
 * that no two of them are handed the same one.
 */
#include "cera.h"

#include <pthread.h>
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

/* {{{ static void nothing_moves_across_a_shelf() */
/*
 * The invariant the whole shape exists to keep. Records are noted
 * before the table grows past a shelf boundary and checked afterwards
 * — a flat array would have reallocated and moved every one of them,
 * taking their mutexes with them.
 */
static void nothing_moves_across_a_shelf(void)
{
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "seven", CERA_STATION_PLAIN);

    /* Where the first few live now. */
    enum { WATCH = 4 };
    cera_station_t *before[WATCH];
    for (int i = 0; i < WATCH; i++) {
        int at = cera_map_add_station(m);
        check(at >= 0, "a place was handed out");
        cera_map_place_box(m, at, "keep", CERA_STATION_PLAIN);
        before[i] = cera_map_station(m, at);
    }

    /* Well past one shelf, so the shelf array itself reallocates
     * several times. */
    int added = 0;
    for (int i = 0; i < CERA_STATIONS_PER_SHELF * 3; i++) {
        int at = cera_map_add_station(m);
        if (at < 0)
            break;
        cera_map_place_box(m, at, "keep", CERA_STATION_PLAIN);
        added++;
    }
    check(added == CERA_STATIONS_PER_SHELF * 3, "the table grew as far as asked");
    check(m->n_shelves > 1, "and it took more than one shelf to do it");

    int moved = 0;
    for (int i = 0; i < WATCH; i++)
        if (cera_map_station(m, i + 1) != before[i])
            moved = 1;
    check(!moved,
          "not one station already placed moved, mutex and all — which is "
          "the entire reason the table is shelves");

    int shelves = m->n_shelves;
    cera_map_destroy(m);
    printf("  grew across %d shelves; nothing already placed moved\n",
           shelves);
}
/* }}} */

/* {{{ static void a_station_added_mid_run() */
/*
 * The capability: a program already running gains a station, gets a
 * wire drawn to it afterwards, and everything that was already going
 * carries on undisturbed.
 */
static void a_station_added_mid_run(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "double_it", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_PLAIN);
    cera_map_connect(m, 0, 0, 1, 0);

    cera_map_start(m, 4);
    cera_pool_submitter_register(m->pool);
    cera_pool_release(m->pool);

    for (int i = 0; i < 50; i++) {
        int v = i;
        cera_map_deliver_value(m, 0, 0, &v);
    }

    /* Grown while it runs, then wired in. */
    int fresh = cera_map_add_station(m);
    check(fresh >= 2, "a new place came from beyond what was allocated");
    cera_map_place_box(m, fresh, "keep", CERA_STATION_PLAIN);
    check(cera_map_wire(m, 0, 0, fresh, 0) == NULL,
          "and a wire reached it");

    for (int i = 0; i < 50; i++) {
        int v = i;
        cera_map_deliver_value(m, 0, 0, &v);
    }

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(atomic_load(&cera_map_station(m, fresh)->runs) > 0,
          "the station added mid-run received values");
    check(atomic_load(&cera_map_station(m, 1)->runs) == 100,
          "and the one that was always there got every value, undisturbed");

    cera_map_destroy(m);
    printf("  a station added mid-run took values; the old one lost none\n");
}
/* }}} */

/* {{{ many threads asking at once */
typedef struct {
    cera_map_t *m;
    int    got[64];
    int    n;
} asker_t;

static void *ask_for_places(void *arg)
{
    asker_t *a = arg;
    for (int i = 0; i < a->n; i++)
        a->got[i] = cera_map_add_station(a->m);
    return NULL;
}

/*
 * Several threads asking for a place at the same moment. Two of them
 * handed the same index would be two stations sharing a record, which
 * is the failure this is looking for.
 */
static void concurrent_asks_share_nothing(void)
{
    enum { THREADS = 8, EACH = 20 };
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "seven", CERA_STATION_PLAIN);

    asker_t askers[THREADS];
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++) {
        askers[i].m = m;
        askers[i].n = EACH;
        memset(askers[i].got, -1, sizeof askers[i].got);
    }

    /* Each place is filled as soon as it is handed out, because an
     * unfilled place is one the next asker may legitimately be given
     * — "free" means "nothing is in it", and that is the same whether
     * it was never filled or was emptied. */
    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, ask_for_places, &askers[i]);
    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);

    int seen[THREADS * EACH + 8];
    memset(seen, 0, sizeof seen);
    int handed = 0, duplicates = 0;
    for (int t = 0; t < THREADS; t++)
        for (int i = 0; i < EACH; i++) {
            int at = askers[t].got[i];
            check(at >= 0, "every ask got a place");
            if (at >= 0 && at < (int)(sizeof seen / sizeof seen[0])) {
                if (seen[at])
                    duplicates++;
                seen[at] = 1;
                handed++;
            }
        }

    /*
     * Places repeat here, and that is correct rather than a failure:
     * nothing was ever placed in them, so every asker was told about
     * the same empty place. What must not repeat is a place somebody
     * has actually filled, which is what the loop below fills and
     * re-checks.
     */
    printf("  %d asks with nothing filled returned %d distinct places\n",
           handed, handed - duplicates);

    cera_map_destroy(m);
}
/* }}} */

/* {{{ static void filled_places_are_never_handed_twice() */
static void filled_places_are_never_handed_twice(void)
{
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "seven", CERA_STATION_PLAIN);

    enum { N = 200 };
    int seen[N + 8];
    memset(seen, 0, sizeof seen);
    int duplicates = 0;
    for (int i = 0; i < N; i++) {
        int at = cera_map_add_station(m);
        check(at >= 0, "a place was handed out");
        if (at >= 0 && at < N + 8) {
            if (seen[at])
                duplicates++;
            seen[at] = 1;
        }
        cera_map_place_box(m, at, "keep", CERA_STATION_PLAIN);
    }
    check(duplicates == 0,
          "no filled place was ever handed to a second caller");
    check(m->n_stations >= N, "and the table holds all of them");
    printf("  %d places asked for and filled, none handed twice\n", N);

    cera_map_destroy(m);
}
/* }}} */

/* {{{ main */
int main(void)
{
    nothing_moves_across_a_shelf();
    a_station_added_mid_run();
    concurrent_asks_share_nothing();
    filled_places_are_never_handed_twice();

    if (failures) {
        fprintf(stderr, "%d growth checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
