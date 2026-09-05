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
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* {{{ static void the_command_line_is_an_argument_list() */
/*
 * **A program run from a shell gets its arguments through the same
 * door as everything else** (issue 213).
 *
 * There is no new mechanism here and that is the claim. The engine
 * already turns text into correctly laid-out bytes — it is how a
 * constant written in a map file becomes a value — so pointing that
 * reader at an argument list is the same code, the same
 * compiler-computed offsets, and the same messages naming the field
 * that was wrong. **Struct arguments in brace syntax come along for
 * free**, which is what this scene exists to show: nothing was built
 * to make them work.
 *
 * A program's arguments are the input ports of the stations it
 * declared as entrances, taken in station order and then port order.
 * Two entrances of one port each is a program that takes two
 * arguments.
 */
static void the_command_line_is_an_argument_list(void)
{
    map_t *m = map_create_empty();

    /* One entrance taking a struct, which is the interesting half. */
    int shape = map_add_station(m);
    map_place_box(m, shape, "magnitude_squared", STATION_PLAIN); /* (vec3) */
    must_take(map_name_station(m, shape, "shape"), "a name");
    must_take(map_designate_input(m, shape), "an entrance");

    /* One entrance taking a plain number. */
    int count = map_add_station(m);
    map_place_box(m, count, "double_it", STATION_PLAIN);         /* (int) */
    must_take(map_name_station(m, count, "count"), "a name");
    must_take(map_designate_input(m, count), "a second entrance");

    int shape_out = map_add_station(m);
    map_place_box(m, shape_out, "swallow", STATION_PLAIN);
    must_take(map_name_station(m, shape_out, "shape_out"), "a name");
    must_take(map_wire(m, shape, 0, shape_out, 0), "a wire");

    int answer = map_add_station(m);
    map_place_box(m, answer, "keep", STATION_PLAIN);
    must_take(map_name_station(m, answer, "answer"), "a name");
    must_take(map_designate_output(m, answer), "a way out");
    must_take(map_wire(m, count, 0, answer, 0), "a wire");

    map_start(m, 2);
    must_take(map_bring_up(m), "the program");

    /*
     * **The promise before the gate**, which is the rule anything
     * outside the workers has always had to follow. This program
     * seeds nothing — every station waits for an argument — so
     * without it the last sleeper decides the program is over between
     * the release and the first delivery, and every argument becomes
     * a task nobody runs.
     */
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* A wrong count is refused rather than half delivered, and says
     * how many the program wanted. */
    char *too_few[] = { "prog", "21" };
    const char *no = map_deliver_command_line(m, 2, too_few);
    check(no != NULL && strstr(no, "takes 2 arguments") != NULL,
          "a command line of the wrong length is refused, saying how "
          "many the program wanted");

    /* The real thing: a struct in brace syntax and a number. */
    char *argv[] = { "prog", "{ 1.0, 2.0, 2.0 }", "21" };
    must_take(map_deliver_command_line(m, 3, argv), "the command line");

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    int got = 0;
    check(map_output_take(m, answer, &got, sizeof got) && got == 42,
          "the number argument arrived and was doubled");
    check(atomic_load(&map_station(m, shape)->runs) == 1,
          "and the struct argument arrived as a struct, in brace syntax "
          "nobody had to build support for");

    map_destroy(m);
    printf("  a command line became arguments, struct in braces and all\n");
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

    /*
     * And the doors survive being written down.
     *
     * A program whose declarations were lost on the way to disk could
     * not be composed after a round trip, which is most of what
     * naming them was for — the parent wires to the doors, so a
     * reloaded program with no doors is a program a parent can no
     * longer reach.
     */
    char dir[256], path[320];
    snprintf(dir, sizeof dir, "%s/doors-%d", late_source_dir(),
             (int)getpid());
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p %s", dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "cannot make %s\n", dir);
        return 1;
    }
    snprintf(path, sizeof path, "%s/doors.map", dir);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fputs("station gate keep p entry\n"
          "  out 0 - answer.0\n"
          "station answer double_it p result\n", f);
    fclose(f);

    map_t *loaded = map_load_file(path, 2);
    if (map_station(loaded, 0)->door != DOOR_IN
        || map_station(loaded, 1)->door != DOOR_OUT) {
        fprintf(stderr, "the doors did not survive being read\n");
        return 1;
    }

    char dumped[320];
    snprintf(dumped, sizeof dumped, "%s/doors-dump.map", dir);
    f = fopen(dumped, "w");
    map_dump(loaded, f);
    fclose(f);

    map_t *again = map_load_file(dumped, 2);
    if (map_station(again, 0)->door != DOOR_IN
        || map_station(again, 1)->door != DOOR_OUT) {
        fprintf(stderr, "the doors did not survive being written down\n");
        return 1;
    }
    printf("  both doors survived a round trip through a file\n");

    /* And a word that is neither is refused rather than ignored. */
    char bad[320];
    snprintf(bad, sizeof bad, "%s/bad.map", dir);
    f = fopen(bad, "w");
    fputs("station gate keep p sideways\n", f);
    fclose(f);

    pool_release(loaded->pool);
    pool_join(loaded->pool);
    pool_release(again->pool);
    pool_join(again->pool);
    map_destroy(loaded);
    map_destroy(again);

    the_command_line_is_an_argument_list();
    if (failures)
        return 1;
    return 0;
}
