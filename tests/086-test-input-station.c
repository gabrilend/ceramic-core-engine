/*
 * 086-test-input-station.c — a program has a surface (issue 213).
 *
 * What this is: the other door, and the proof that having both makes
 * a program usable without knowing what is inside it.
 *
 * Before, a value got into a running program exactly one way:
 * somebody holding the program named a station and a port. That
 * works, and it means the caller has to know the program's insides —
 * rename an interior station and every caller breaks. That is not
 * encapsulation. The program has no surface, only internals that
 * happen to be reachable.
 *
 * How it does it, in general terms: a small program is built with one
 * station declared as its entrance and another as where its results
 * come from. A caller then feeds it and collects from it **naming
 * only those two**, and the interior station between them is never
 * mentioned. Then the same caller is refused when it tries to reach
 * an interior station directly, which is the half that makes the
 * first half mean something: a surface nobody has to respect is not a
 * surface.
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

int main(void)
{
    /*
     * entrance -> middle -> results.
     *
     * Every station runs an ordinary box. The two doors are ordinary
     * stations carrying a mark; nothing here is a new kind of thing.
     */
    map_t *m = map_create_empty();

    int entrance = map_add_station(m);
    map_place_box(m, entrance, "keep", STATION_PLAIN);
    must_take(map_name_station(m, entrance, "entrance"), "a name");

    int middle = map_add_station(m);
    map_place_box(m, middle, "double_it", STATION_PLAIN);
    must_take(map_name_station(m, middle, "middle"), "a name");

    int results = map_add_station(m);
    map_place_box(m, results, "double_it", STATION_PLAIN);
    must_take(map_name_station(m, results, "results"), "a name");

    must_take(map_wire(m, entrance, 0, middle, 0), "a wire");
    must_take(map_wire(m, middle, 0, results, 0), "a wire");

    must_take(map_designate_input(m, entrance), "the entrance");
    must_take(map_designate_output(m, results), "the results");

    map_start(m, 2);
    pool_submitter_register(m->pool);
    must_take(map_bring_up(m), "the program");
    pool_release(m->pool);

    /*
     * The caller now knows two things about this program: where to
     * put values in, and where to take them out. It does not know
     * that there is a station in the middle, or what it is called, or
     * that there is one at all.
     */
    for (int i = 1; i <= 3; i++) {
        int argument = i;
        must_take(map_deliver_argument(m, entrance, 0, &argument,
                                       sizeof argument),
                  "an argument");
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    if (map_output_waiting(m, results) != 3) {
        fprintf(stderr, "three arguments produced %d results\n",
                map_output_waiting(m, results));
        return 1;
    }
    /*
     * Doubled twice: 1, 2 and 3 in gives 4, 8 and 12 out. Checked as a
     * set rather than as a sequence, and the distinction is real
     * rather than caution.
     *
     * Results are *held* oldest-first, so the order they are taken in
     * is the order they arrived at this station. That is not the
     * order the arguments went in: three values fed to a program with
     * two workers are in flight at once, and which finishes first is
     * a schedule. Asserting the sequence here would be asserting the
     * scheduler, which is the thing this engine most deliberately
     * does not promise.
     */
    int seen[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        int got = 0;
        if (!map_output_take(m, results, &got, sizeof got)) {
            fprintf(stderr, "result %d was missing\n", i + 1);
            return 1;
        }
        int which = got / 4 - 1;
        if (got % 4 != 0 || which < 0 || which > 2 || seen[which]) {
            fprintf(stderr, "an unexpected or repeated result: %d\n", got);
            return 1;
        }
        seen[which] = 1;
    }
    printf("  a caller fed a program and collected from it, naming only "
           "its two doors\n");

    /*
     * And the surface is enforced. Reaching the interior station is
     * refused — which is what makes the first half a surface rather
     * than a convention.
     */
    int sneaky = 99;
    if (map_deliver_argument(m, middle, 0, &sneaky, sizeof sneaky) == NULL) {
        fprintf(stderr, "the outside reached an interior station\n");
        return 1;
    }
    /* Wrong size at the right door is refused too, because there is no
     * wire here to have been checked when it was drawn. */
    double wrong = 1.0;
    if (map_deliver_argument(m, entrance, 0, &wrong, sizeof wrong) == NULL) {
        fprintf(stderr, "a value of the wrong size was accepted\n");
        return 1;
    }
    printf("  and reaching past the doors was refused, by station and by "
           "size\n");

    /* A station cannot be both doors. */
    if (map_designate_input(m, results) == NULL) {
        fprintf(stderr, "one station was accepted as both doors\n");
        return 1;
    }
    printf("  a station was refused as both entrance and exit\n");

    map_destroy(m);
    return 0;
}
