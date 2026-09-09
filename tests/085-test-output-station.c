/*
 * 085-test-output-station.c — where a program's results go
 * (issues 209, 209a).
 *
 * What this is: the proof that **registering somewhere to put the
 * values is what makes a result collect**, and that a marked port with
 * nowhere registered behaves exactly like any other unwired output.
 *
 * What this replaced. A marked station used to *hold* every value it
 * produced, in an array that doubled whenever it filled and shouted
 * from the first growth that results were piling up and nobody was
 * taking them. That warning was written instead of a fix: a program
 * whose results nobody drained grew until memory ran out, having been
 * told so on the way. Now nothing is held unless a caller has said
 * where to put it, so the state the warning described cannot happen.
 *
 * How it does it, in general terms: the same program is run three
 * times — with a place to put the results, with a marked port and no
 * place, and with neither — and the difference is asserted in every
 * direction. The two dropping cases are not a bug being tolerated:
 * discarding is the rule, an unwired comparator branch is the ordinary
 * case for it, and a test that only checked the collecting half would
 * not have shown that registering is the whole of the exception.
 *
 * The bound is the other half. An array smaller than the run is filled
 * to its edge and no further, and the count keeps climbing past it —
 * which is how a caller tells "that was all there was" from "I stopped
 * it", and is why the count is not clamped at the reservation.
 */
#include "cera.h"

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

/* {{{ static cera_map_t *a_program() */
/*
 * Two stations: one produces seven, the other doubles it and is wired
 * nowhere. `mark` says whether the second station's output port is one
 * of the program's results; `into` and `room`, when given, say where
 * the values go. The three scenes differ only in those.
 */
static cera_map_t *a_program(int mark, int *into, int room)
{
    cera_map_t *m = cera_map_create_empty();
    int src = cera_map_add_station(m);
    cera_map_place_box(m, src, "seven", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, src, "source"), "a name");

    int out = cera_map_add_station(m);
    cera_map_place_box(m, out, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, out, "result"), "a name");

    must_take(cera_map_wire(m, src, 0, out, 0), "a wire");

    if (mark)
        must_take(cera_map_designate_result(m, out, 0, 0), "the mark");

    cera_map_start(m, 2);
    must_take(cera_map_bring_up(m), "the program");

    /* Before the workers are let go, because a value that arrives
     * before anybody has said where to put it is discarded like any
     * other unwired value. That ordering is the rule, not a
     * convenience — registering afterwards would lose whatever had
     * already been produced, silently. */
    if (into)
        must_take(cera_map_collect(m, out, 0, into, room, (int)sizeof into[0]),
                  "somewhere to put the results");

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    return m;
}
/* }}} */

int main(void)
{
    /* {{{ registered: the value lands in the caller's array */
    int landed[4] = { 0 };
    cera_map_t *kept = a_program(1, landed, 4);
    check(cera_map_collected(kept, 1, 0) == 1,
          "one value reached a registered result");
    check(landed[0] == 14,
          "and it is the value the program computed: seven, doubled");
    cera_map_destroy(kept);
    printf("  a registered result put its value in the caller's array\n");
    /* }}} */

    /* {{{ marked but unregistered: dropped, like any unwired output */
    /*
     * The half that shows registering to be the whole of the
     * exception. A mark says which result this is, so that somebody
     * outside has a number to ask for; it does not on its own make the
     * engine keep anything.
     */
    cera_map_t *marked = a_program(1, NULL, 0);
    check(cera_map_collected(marked, 1, 0) == 0,
          "a marked result with nowhere registered kept nothing");
    check(atomic_load(&cera_map_station(marked, 1)->runs) == 1,
          "though the station ran, so this is discarding rather than "
          "never happening");
    cera_map_destroy(marked);
    printf("  a marked result with nowhere to put values discarded them\n");
    /* }}} */

    /* {{{ neither marked nor registered: dropped, as always */
    cera_map_t *plain = a_program(0, NULL, 0);
    check(cera_map_collected(plain, 1, 0) == 0,
          "an unmarked output kept nothing either");
    cera_map_destroy(plain);
    printf("  an unmarked output discarded, which is the ordinary rule\n");
    /* }}} */

    /* {{{ the bound: an array smaller than the run */
    /*
     * **The reservation is what keeps the array in bounds**, not the
     * winding down. A worker takes the next index with one atomic add
     * and writes nothing when the index is at or past the room, so a
     * program that outruns its array overwrites nothing beyond it.
     *
     * The count keeps climbing past the room on purpose: comparing it
     * against the room is how a caller tells a program that filled the
     * array from one that ran dry, and clamping it would throw that
     * away.
     */
    cera_map_t *m = cera_map_create_empty();
    int feed = cera_map_add_station(m);
    cera_map_place_box(m, feed, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, feed, "feed"), "a name");
    must_take(cera_map_designate_argument(m, feed, 0, 0), "an entrance");
    must_take(cera_map_designate_result(m, feed, 0, 0), "a result");

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");

    /* Room for three, and ten are sent. */
    int narrow[3] = { -1, -1, -1 };
    must_take(cera_map_collect(m, feed, 0, narrow, 3, (int)sizeof narrow[0]),
              "a narrow array");
    cera_pool_release(m->pool);

    for (int i = 1; i <= 10; i++)
        must_take(cera_map_deliver_argument(m, feed, 0, &i, sizeof i),
                  "an argument");

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(cera_map_collected(m, feed, 0) == 3,
          "an array with room for three reports three, however many ran");
    for (int i = 0; i < 3; i++)
        check(narrow[i] >= 1 && narrow[i] <= 10,
              "and every slot holds a value the program actually produced");
    check(atomic_load(&cera_map_station(m, feed)->runs) == 10,
          "while the program itself ran ten times — the array filled, the "
          "program did not stop");
    cera_map_destroy(m);
    printf("  ten values into room for three filled it and wrote no "
           "further\n");
    /* }}} */

    if (failures) {
        fprintf(stderr, "%d output checks failed\n", failures);
        return 1;
    }
    return 0;
}
