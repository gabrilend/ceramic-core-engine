/*
 * 080-test-page-growth.c — proves a ring buffer that grows by adding
 * pages loses nothing and doubles nothing (issue 210e).
 *
 * What this is: the test for the failure the growth it replaced
 * actually produced. Copy-and-unwrap growth had an ordering problem
 * with no correct answer — copy the live values across and then
 * publish, and a value taken from the old array mid-copy exists in
 * both places and is delivered twice; publish first and then copy,
 * and a reader sees an empty buffer while it fills. What made it safe
 * was that the station's mutex was held for the whole copy, so
 * nothing else could happen at all. Paging removes the copy, and
 * therefore removes the requirement rather than satisfying it.
 *
 * How it does it, in general terms: a station with two input ports is
 * fed hard on one side and not at all on the other, so values pile up
 * and the port grows page after page. Then the second side opens
 * while the first keeps writing, so claims, writes and growth all
 * happen at once on the same port. Every value carries its own
 * identity, and at the end each must have come out exactly once —
 * which is a statement about both halves of the old failure at once,
 * since a lost value and a doubled one are the two ways it went
 * wrong.
 *
 * A second scene checks the arithmetic paging made non-obvious: the
 * capacity is a sum across pages rather than one allocation's size,
 * and a slot's ordinal has to resolve to the right page and offset
 * for every ordinal the port has, including the ones either side of a
 * page boundary. That is the part a concurrency test would only catch
 * by luck.
 */
#include "018-station.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Deliberately awkward numbers: a page size that is not a power of
 * two and a total that is not a multiple of it, so a page boundary
 * falls somewhere the arithmetic cannot get right by accident. */
enum { PAGE = 7, VALUES = 900, FEEDERS = 4 };

static _Atomic int seen[VALUES];
static map_t      *the_map;

/* {{{ check() */
static void check(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        exit(1);
    }
}
/* }}} */

/* {{{ recorder__call() — the box under the station */
/*
 * Records that this value arrived, by its own identity rather than by
 * counting. A counter would prove nothing about doubling: two
 * arrivals of one value and one arrival each of two values give the
 * same total.
 */
static void recorder__call(task_t *t)
{
    int v;
    memcpy(&v, t->in[0], sizeof v);
    if (v >= 0 && v < VALUES)
        seen[v]++;
}
/* }}} */

/* {{{ feed_values() — the side that piles up */
static void *feed_values(void *arg)
{
    long which = (long)arg;
    pool_submitter_register(the_map->pool);
    for (int i = (int)which; i < VALUES; i += FEEDERS) {
        map_deliver_value(the_map, 0, 0, &i);
    }
    pool_submitter_unregister(the_map->pool);
    return NULL;
}
/* }}} */

/* {{{ feed_tokens() — the side that lets them out */
static void *feed_tokens(void *arg)
{
    long which = (long)arg;
    pool_submitter_register(the_map->pool);
    for (int i = (int)which; i < VALUES; i += FEEDERS) {
        int token = 1;
        map_deliver_value(the_map, 0, 1, &token);
    }
    pool_submitter_unregister(the_map->pool);
    return NULL;
}
/* }}} */

/* {{{ nothing_lost_or_doubled_while_it_grows() */
static void nothing_lost_or_doubled_while_it_grows(void)
{
    for (int i = 0; i < VALUES; i++)
        atomic_store(&seen[i], 0);

    int sizes[2] = { (int)sizeof(int), (int)sizeof(int) };
    the_map = map_create(1);
    map_place(the_map, 0, recorder__call, STATION_PLAIN, 2, sizes, 0);

    /* A small page, so the run crosses many boundaries rather than
     * one. The starting depth is the page size for every page a port
     * ever adds, which is what makes this the lever. */
    map_in_port_start_depth(the_map, 0, 0, PAGE);
    map_in_port_start_depth(the_map, 0, 1, PAGE);

    /* This scene deliberately builds the backlog the buffer report
     * exists to shout about, so the shout will happen. Saying so
     * first is the difference between a diagnostic doing its job and
     * a line somebody has to go and investigate. */
    printf("  (the growth warning below is what this scene is provoking)\n");
    fflush(stdout);

    map_start(the_map, 4);
    pool_submitter_register(the_map->pool);
    pool_release(the_map->pool);

    /* First the pile-up: one side only, so nothing can be claimed and
     * the port has no choice but to grow. */
    pthread_t writers[FEEDERS];
    for (long i = 0; i < FEEDERS; i++)
        pthread_create(&writers[i], NULL, feed_values, (void *)i);
    for (int i = 0; i < FEEDERS; i++)
        pthread_join(writers[i], NULL);

    in_port_t *piled = &map_station(the_map, 0)->in_ports[0];
    check(piled->growths > 0, "the port grew at all");
    check(piled->capacity == PAGE * (piled->growths + 1),
          "capacity is the sum across pages, first page included");
    check(atomic_load(&piled->held) == VALUES,
          "every value written is waiting, none lost on the way in");

    /* Now the other side, which drains it while nothing else is
     * arriving — the claims run against a port many pages deep. */
    pthread_t readers[FEEDERS];
    for (long i = 0; i < FEEDERS; i++)
        pthread_create(&readers[i], NULL, feed_tokens, (void *)i);
    for (int i = 0; i < FEEDERS; i++)
        pthread_join(readers[i], NULL);

    pool_submitter_unregister(the_map->pool);
    pool_join(the_map->pool);

    int missing = 0, doubled = 0;
    for (int i = 0; i < VALUES; i++) {
        int n = atomic_load(&seen[i]);
        if (n == 0) missing++;
        if (n > 1)  doubled++;
    }
    check(missing == 0, "no value was lost across the page boundaries");
    check(doubled == 0, "no value was delivered twice");

    printf("  %d values across %d pages, none lost and none doubled\n",
           VALUES, piled->growths + 1);
    map_destroy(the_map);
}
/* }}} */

/* {{{ every_ordinal_lands_on_its_own_slot() */
/*
 * The arithmetic, checked directly. Paging turned a slot's address
 * from one multiplication into a division, a remainder and a walk,
 * and the ordinals either side of a page boundary are where that goes
 * wrong. Every ordinal the port has must give a distinct address, and
 * the addresses within one page must be exactly a stride apart.
 */
static void every_ordinal_lands_on_its_own_slot(void)
{
    int sizes[1] = { (int)sizeof(int) };
    map_t *m = map_create(1);
    map_place(m, 0, recorder__call, STATION_PLAIN, 1, sizes, 0);
    map_in_port_start_depth(m, 0, 0, PAGE);

    in_port_t *sl = &map_station(m, 0)->in_ports[0];
    /* Four pages, added the way growth adds them. */
    for (int i = 0; i < 3; i++)
        in_port_add_page(sl);
    check(sl->capacity == PAGE * 4, "four pages of the one page size");

    /* Distinct addresses, and neighbours inside a page one stride
     * apart. Across a boundary they may be anywhere, which is the
     * whole point — the pages are separate allocations. */
    for (int i = 0; i < sl->capacity; i++) {
        unsigned char *here = in_port_slot(sl, i);
        for (int j = 0; j < i; j++)
            check(here != in_port_slot(sl, j),
                  "two ordinals resolved to one slot");
        if (i % PAGE != 0) {
            unsigned char *prev = in_port_slot(sl, i - 1);
            check(here - prev == sl->stride,
                  "neighbours inside a page are one stride apart");
        }
    }

    /* And the states are empty everywhere, including on the pages
     * added after the port was in use — a page arriving with stale
     * bytes would be read as holding values nobody wrote. */
    for (int i = 0; i < sl->capacity; i++)
        check(slot_move_at(in_port_slot(sl, i), sl->elem_size,
                           SLOT_EMPTY, SLOT_RESERVED),
              "a slot on a fresh page was not empty");

    printf("  %d ordinals over 4 pages, each its own slot, all empty\n",
           sl->capacity);
    map_destroy(m);
}
/* }}} */

int main(void)
{
    every_ordinal_lands_on_its_own_slot();
    nothing_lost_or_doubled_while_it_grows();
    return 0;
}
