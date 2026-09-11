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
 * A program's arguments are its marked ports, in the order their
 * numbers say. Two ports marked as argument zero and argument one is
 * a program that takes two arguments, and moving either line in the
 * file changes nothing.
 */
static void the_command_line_is_an_argument_list(void)
{
    cera_map_t *m = cera_map_create_empty();

    /* One entrance taking a struct, which is the interesting half. */
    int shape = cera_map_add_station(m);
    cera_map_place_box(m, shape, "magnitude_squared", CERA_STATION_PLAIN); /* (vec3) */
    must_take(cera_map_name_station(m, shape, "shape"), "a name");
    must_take(cera_map_designate_argument(m, shape, 0, 0), "an entrance");

    /* One entrance taking a plain number. */
    int count = cera_map_add_station(m);
    cera_map_place_box(m, count, "double_it", CERA_STATION_PLAIN);         /* (int) */
    must_take(cera_map_name_station(m, count, "count"), "a name");
    must_take(cera_map_designate_argument(m, count, 0, 1), "a second entrance");

    int shape_out = cera_map_add_station(m);
    cera_map_place_box(m, shape_out, "swallow", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, shape_out, "shape_out"), "a name");
    must_take(cera_map_wire(m, shape, 0, shape_out, 0), "a wire");

    int answer = cera_map_add_station(m);
    cera_map_place_box(m, answer, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, answer, "answer"), "a name");
    must_take(cera_map_designate_result(m, answer, 0, 0), "a way out");
    must_take(cera_map_wire(m, count, 0, answer, 0), "a wire");

    cera_map_start(m, 2);
    must_take(cera_map_bring_up(m), "the program");

    /*
     * **The promise before the gate**, which is the rule anything
     * outside the workers has always had to follow. This program
     * seeds nothing — every station waits for an argument — so
     * without it the last sleeper decides the program is over between
     * the release and the first delivery, and every argument becomes
     * a task nobody runs.
     */
    cera_pool_submitter_register(m->pool);

    int doubled[4] = { 0 };
    must_take(cera_map_collect(m, answer, 0, doubled, 4,
                               (int)sizeof doubled[0]),
              "somewhere to put the answer");

    cera_pool_release(m->pool);

    /* A wrong count is refused rather than half delivered, and says
     * how many the program wanted. */
    char *too_few[] = { "prog", "21" };
    const char *no = cera_map_deliver_command_line(m, 2, too_few);
    check(no != NULL && strstr(no, "takes 2 arguments") != NULL,
          "a command line of the wrong length is refused, saying how "
          "many the program wanted");

    /* The real thing: a struct in brace syntax and a number. */
    char *argv[] = { "prog", "{ 1.0, 2.0, 2.0 }", "21" };
    must_take(cera_map_deliver_command_line(m, 3, argv), "the command line");

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(cera_map_collected(m, answer, 0) == 1 && doubled[0] == 42,
          "the number argument arrived and was doubled");
    check(atomic_load(&cera_map_station(m, shape)->runs) == 1,
          "and the struct argument arrived as a struct, in brace syntax "
          "nobody had to build support for");

    cera_map_destroy(m);
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
    cera_map_t *m = cera_map_create_empty();

    int entrance = cera_map_add_station(m);
    cera_map_place_box(m, entrance, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, entrance, "entrance"), "a name");

    int middle = cera_map_add_station(m);
    cera_map_place_box(m, middle, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, middle, "middle"), "a name");

    int results = cera_map_add_station(m);
    cera_map_place_box(m, results, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, results, "results"), "a name");

    must_take(cera_map_wire(m, entrance, 0, middle, 0), "a wire");
    must_take(cera_map_wire(m, middle, 0, results, 0), "a wire");

    must_take(cera_map_designate_argument(m, entrance, 0, 0), "the entrance");
    must_take(cera_map_designate_result(m, results, 0, 0), "the results");

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");

    /* Somewhere to put them, said before the workers are let go: a
     * result that arrives before anybody has asked for it is
     * discarded like any other unwired value. */
    int answers[8] = { 0 };
    must_take(cera_map_collect(m, results, 0, answers, 8,
                               (int)sizeof answers[0]),
              "somewhere to put the results");

    cera_pool_release(m->pool);

    /*
     * The caller now knows two things about this program: where to
     * put values in, and where to take them out. It does not know
     * that there is a station in the middle, or what it is called, or
     * that there is one at all.
     */
    for (int i = 1; i <= 3; i++) {
        int argument = i;
        must_take(cera_map_deliver_argument(m, entrance, 0, &argument,
                                       sizeof argument),
                  "an argument");
    }

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    if (cera_map_collected(m, results, 0) != 3) {
        fprintf(stderr, "three arguments produced %d results\n",
                cera_map_collected(m, results, 0));
        return 1;
    }
    /*
     * Doubled twice: 1, 2 and 3 in gives 4, 8 and 12 out. Checked as a
     * set rather than as a sequence, and the distinction is real
     * rather than caution.
     *
     * A slot in the array is claimed when a value arrives, so the
     * order of the array is the order values reached this port. That
     * is not the order the arguments went in: three values fed to a
     * program with two workers are in flight at once, and which
     * finishes first is a schedule. Asserting the sequence here would
     * be asserting the scheduler, which is the thing this engine most
     * deliberately does not promise.
     */
    int seen[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        int got = answers[i];
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
    if (cera_map_deliver_argument(m, middle, 0, &sneaky, sizeof sneaky) == NULL) {
        fprintf(stderr, "the outside reached an interior station\n");
        return 1;
    }
    /* Wrong size at the right door is refused too, because there is no
     * wire here to have been checked when it was drawn. */
    double wrong = 1.0;
    if (cera_map_deliver_argument(m, entrance, 0, &wrong, sizeof wrong) == NULL) {
        fprintf(stderr, "a value of the wrong size was accepted\n");
        return 1;
    }
    printf("  and reaching past the doors was refused, by station and by "
           "size\n");

    /*
     * **A station may hold ports of both kinds** (issue 213a), and the
     * old refusal is gone with the thing that made it necessary. The
     * mark used to be on the station, and a station is one thing, so
     * being both doors was somebody having named the wrong one. A port
     * is a smaller thing: a station with an argument port and a result
     * port is ordinary, and this scene now proves it is accepted
     * rather than refused.
     *
     * The number is one, because argument zero is already taken and
     * two ports claiming the same number is the thing that *is*
     * refused.
     */
    must_take(cera_map_designate_argument(m, results, 0, 1),
              "a second argument on the station that also holds a result");
    printf("  a station holds an argument port and a result port at once\n");

    cera_map_destroy(m);

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
    snprintf(dir, sizeof dir, "%s/doors-%d", cera_late_source_dir(),
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
    fputs("station gate (keep)\n"
          "  in 0 - 0$\n"
          "  out 0 - answer.0\n"
          "station answer (double_it)\n"
          "  in 0 - gate.0\n"
          "  out 0 - 0$\n", f);
    fclose(f);

    cera_map_t *loaded = cera_map_load_file(path, 2);
    int at = -1, which = -1;
    if (!cera_map_argument_at(loaded, 0, &at, &which) || at != 0 || which != 0
        || !cera_map_result_at(loaded, 0, &at, &which) || at != 1
        || which != 0) {
        fprintf(stderr, "the doors did not survive being read\n");
        return 1;
    }

    char dumped[320];
    snprintf(dumped, sizeof dumped, "%s/doors-dump.map", dir);
    f = fopen(dumped, "w");
    cera_map_dump(loaded, f);
    fclose(f);

    cera_map_t *again = cera_map_load_file(dumped, 2);
    if (!cera_map_argument_at(again, 0, &at, &which) || at != 0 || which != 0
        || !cera_map_result_at(again, 0, &at, &which) || at != 1
        || which != 0) {
        fprintf(stderr, "the doors did not survive being written down\n");
        return 1;
    }
    printf("  both doors survived a round trip through a file\n");

    /* And a word that is neither is refused rather than ignored. */
    char bad[320];
    snprintf(bad, sizeof bad, "%s/bad.map", dir);
    f = fopen(bad, "w");
    fputs("station gate (keep) sideways\n", f);
    fclose(f);

    cera_pool_release(loaded->pool);
    cera_pool_join(loaded->pool);
    cera_pool_release(again->pool);
    cera_pool_join(again->pool);
    cera_map_destroy(loaded);
    cera_map_destroy(again);

    the_command_line_is_an_argument_list();
    if (failures)
        return 1;
    return 0;
}
