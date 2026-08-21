/*
 * 085-test-output-station.c — where a program's results come from
 * (issue 209).
 *
 * What this is: the proof that designating a station changes exactly
 * one thing, and that the one thing is the right one.
 *
 * A program had no way to say what it produced. Values moved between
 * stations and stopped; a station wired nowhere discarded, which is
 * deliberate and correct for an unwired comparator branch and wrong
 * for the one case where the values are the point of the whole
 * program. Nothing was missing mechanically — a box can already print
 * or append to a file — what was missing was a **name**: no station
 * was designated as the place results come from, so a program to be
 * composed inside another had nowhere for the parent to wire from.
 *
 * How it does it, in general terms: the same program is run twice,
 * once with its last station designated and once without, and the
 * difference is asserted in both directions. Undesignated, the
 * results are dropped, and that is not a bug being tolerated — it is
 * the rule this designation is an exception to, so a test that only
 * checked the holding half would not have shown that the exception is
 * narrow.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ static map_t *a_program() */
/*
 * Two stations: one produces a value, the other doubles it and is
 * wired nowhere. Whether that second station is designated is what
 * the two scenes differ by.
 */
static map_t *a_program(int designate)
{
    map_t *m = map_create_empty();
    int src = map_add_station(m);
    map_place_box(m, src, "seven", STATION_PLAIN);
    must_take(map_name_station(m, src, "source"), "a name");

    int out = map_add_station(m);
    map_place_box(m, out, "double_it", STATION_PLAIN);
    must_take(map_name_station(m, out, "result"), "a name");

    must_take(map_wire(m, src, 0, out, 0), "a wire");

    /*
     * **Every program declares a way out, so the scenes differ by
     * *which* station is one rather than by whether any is** (issue
     * 209). A program that has never said where its results come from
     * is refused when it is brought up, and that is the requirement
     * this file's first scene exists to be the happy side of.
     *
     * So the undesignated case marks the *source* instead. It changes
     * nothing anybody here observes — the source's output is wired to
     * the second station, and the designation only adds a rule for a
     * port wired nowhere — and it leaves station one undesignated,
     * which is the whole of what this scene is about.
     */
    must_take(map_designate_output(m, designate ? out : src),
              "the designation");

    map_start(m, 2);
    must_take(map_bring_up(m), "the program");
    pool_release(m->pool);
    pool_join(m->pool);
    return m;
}
/* }}} */

int main(void)
{
    /* Designated: the value is waiting to be taken. */
    map_t *kept = a_program(1);
    if (map_output_waiting(kept, 1) != 1) {
        fprintf(stderr, "a designated output held %d results, not 1\n",
                map_output_waiting(kept, 1));
        return 1;
    }
    int got = 0;
    if (!map_output_take(kept, 1, &got, sizeof got)) {
        fprintf(stderr, "nothing came back from a station holding one\n");
        return 1;
    }
    if (got != 14) {
        fprintf(stderr, "the result was %d, not 14\n", got);
        return 1;
    }
    if (map_output_waiting(kept, 1) != 0) {
        fprintf(stderr, "taking a result left it there\n");
        return 1;
    }
    if (map_output_take(kept, 1, &got, sizeof got)) {
        fprintf(stderr, "an empty output handed something back\n");
        return 1;
    }
    printf("  a designated station held its result until it was taken\n");

    /*
     * Undesignated: dropped. This is the rule, not an oversight — an
     * unwired port discards on purpose, because an unwired comparator
     * branch is the ordinary case. Asserting it is what shows the
     * designation to be a narrow exception rather than a change to
     * how ports behave.
     */
    map_t *dropped = a_program(0);
    if (map_output_waiting(dropped, 1) != 0) {
        fprintf(stderr, "an undesignated station held %d results — the "
                        "designation is supposed to be what makes the "
                        "difference\n", map_output_waiting(dropped, 1));
        return 1;
    }
    if (atomic_load(&map_station(dropped, 1)->runs) != 1) {
        fprintf(stderr, "the undesignated station did not run at all, so "
                        "this proves nothing about discarding\n");
        return 1;
    }
    printf("  and an undesignated one ran, produced, and dropped it — "
           "which is the rule this is an exception to\n");

    /* A station that returns nothing cannot be a source of results,
     * and the refusal says so rather than accepting something
     * useless. */
    map_t *sink = map_create_empty();
    int only = map_add_station(sink);
    map_place_box(sink, only, "write_int_file", STATION_PLAIN);
    if (map_designate_output(sink, only) == NULL) {
        fprintf(stderr, "a station returning nothing was accepted as an "
                        "output\n");
        return 1;
    }
    printf("  a station that returns nothing was refused as an output\n");

    map_destroy(kept);
    map_destroy(dropped);
    map_destroy(sink);
    return 0;
}
