/*
 * 101-test-capture.c — a running program put down and picked up
 * again (issue 712).
 *
 * What this is: the proof that a dump is becoming an **image** and
 * not only a schematic. A schematic says what a program is shaped
 * like; an image says what it currently holds, so it can be revived
 * rather than merely rebuilt.
 *
 * How it does it, in general terms: fills a station's buffer with
 * work that cannot run yet — because a second port of the same
 * station is still empty — writes the program down, reads it back in
 * a fresh program, and checks the work is still there. Then finishes
 * the missing port on both and checks they produce the same answers.
 *
 * **Why the work has to be un-runnable to be captured**: a station
 * runs the instant every port holds a value, so anything that *could*
 * run already has. What waits in a buffer is precisely what is
 * waiting for something else, and that is the state worth writing
 * down.
 *
 * What this does not cover yet: draining a running pool before the
 * capture, and a program that grew boxes while it ran. Both are steps
 * of their own on the issue.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* The box types this test delivers, spelled again here rather than
 * included: a test links against the engine and the generated file,
 * not against the box sources, and the widths are what has to agree —
 * a wire is legal on width alone. */
typedef struct { float x; float y; float z; } vec3;

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

static char work_dir[128];

/* {{{ static void write_text() */
static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}
/* }}} */

/* {{{ static void dump_to() */
static void dump_to(cera_map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    cera_map_dump(m, f);
    fclose(f);
}
/* }}} */

/* {{{ static char *slurp() */
static char *slurp(const char *path)
{
    static char buf[4][8192];
    static int which;
    char *out = buf[which++ % 4];
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    size_t n = fread(out, 1, 8191, f);
    out[n] = '\0';
    fclose(f);
    return out;
}
/* }}} */

/* {{{ static void work_in_flight_survives() */
/*
 * The whole claim in one program. `add` takes two numbers; its second
 * port is left with no source, so however many values arrive on the
 * first, nothing can run. Those values are the work in flight.
 */
static void work_in_flight_survives(void)
{
    char map_path[512], dump_path[512], again_path[512];
    snprintf(map_path, sizeof map_path, "%s/waiting.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/captured.map", work_dir);
    snprintf(again_path, sizeof again_path, "%s/recaptured.map", work_dir);

    /* The gate exists so the program has a declared entrance and is
     * therefore allowed to sit waiting; the work is put straight into
     * the adder, which is the station being captured. */
    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - adder.0\n"
        "\n"
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n"
        "  in 1 -\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* Three values into the port that has a source, none of which can
     * make the station ready, because its other port has none. */
    for (int v = 1; v <= 3; v++) {
        int value = v * 100;
        /* What comes back says whether a *task* became due, which is
         * a fact about the station rather than about this value — and
         * here nothing can become due, because the adder's other port
         * is empty. The buffer depth below is what says it landed. */
        cera_map_deliver_value(m, 1, 0, &value);
    }
    check(atomic_load(&cera_map_station(m, 1)->in_ports[0].held) == 3,
          "three values are waiting and none of them could run");
    check(atomic_load(&cera_map_station(m, 1)->runs) == 0,
          "and the station has not run, because its other port is empty");

    dump_to(m, dump_path);

    /* The captured text says what is waiting, in brackets. */
    const char *captured = slurp(dump_path);
    check(strstr(captured, "[") != NULL,
          "the capture wrote the waiting values in brackets");
    check(strstr(captured, "100") && strstr(captured, "200")
          && strstr(captured, "300"),
          "and all three of them are in it");

    /* Read back into a fresh program, which never saw the first. */
    cera_map_t *revived = cera_map_load_file(dump_path, 2);
    check(atomic_load(&cera_map_station(revived, 1)->in_ports[0].held) == 3,
          "the revived program has the same three values waiting");
    check(atomic_load(&cera_map_station(revived, 1)->runs) == 0,
          "and has not run either, so nothing was consumed on the way");

    /* Captured twice is the same text, which is the round trip the
     * dump has always promised, now covering what a program holds and
     * not only what it is. */
    dump_to(revived, again_path);
    check(strcmp(slurp(dump_path), slurp(again_path)) == 0,
          "capturing the revived program produced the same text again");

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    cera_map_destroy(m);
    cera_pool_release(revived->pool);
    cera_pool_join(revived->pool);
    cera_map_destroy(revived);

    printf("  work waiting in a buffer was written down and picked up "
           "again\n");
}
/* }}} */

/* {{{ static void the_revived_program_finishes_the_work() */
/*
 * A capture is only worth having if the program can go on from it. So
 * the missing port is finished on the revived program and the work
 * that was in flight actually runs.
 */
static void the_revived_program_finishes_the_work(void)
{
    char map_path[512], dump_path[512];
    snprintf(map_path, sizeof map_path, "%s/unfinished.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/unfinished-captured.map",
             work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - adder.0\n"
        "\n"
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n"
        "  in 1 -\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);
    for (int v = 1; v <= 3; v++) {
        int value = v * 100;
        cera_map_deliver_value(m, 1, 0, &value);
    }
    dump_to(m, dump_path);
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    cera_map_destroy(m);

    cera_map_t *revived = cera_map_load_file(dump_path, 2);

    /* Finish the port that was never wired, which is what the program
     * was waiting for all along. */
    check(cera_map_configure_port(revived, 1, 1, CERA_IN_PORT_STATIC, "7") == NULL,
          "the missing port was finished on the revived program");
    check(cera_map_bring_up(revived) == NULL,
          "and the revived program came up");

    /* Somewhere to put the answers, said before the workers are let
     * go — a result that arrives before anybody has asked for it is
     * discarded like any other unwired value. */
    int seen[3] = { 0, 0, 0 };
    check(cera_map_collect(revived, 1, 0, seen, 3, (int)sizeof seen[0]) == NULL,
          "somewhere to put the revived program's answers was accepted");

    cera_pool_release(revived->pool);
    cera_pool_join(revived->pool);

    check(atomic_load(&cera_map_station(revived, 1)->runs) == 3,
          "all three pieces of work in flight ran after revival");

    /* And the answers are the ones the first program would have
     * produced: each value plus seven. */
    check(cera_map_collected(revived, 1, 0) == 3,
          "and produced three results");
    int total = seen[0] + seen[1] + seen[2];
    check(total == (100 + 7) + (200 + 7) + (300 + 7),
          "which are the answers the captured work was waiting to give");

    cera_map_destroy(revived);
    printf("  and the revived program finished the work it was carrying\n");
}
/* }}} */

/* {{{ static void a_struct_queue_survives() */
/*
 * The awkward case, because a struct value has commas inside it and
 * the list separating values is commas. Only reading one value at a
 * time can tell an outer comma from an inner one, and this is what
 * proves it does.
 */
static void a_struct_queue_survives(void)
{
    char map_path[512], dump_path[512], again_path[512];
    snprintf(map_path, sizeof map_path, "%s/structs.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/structs-captured.map",
             work_dir);
    snprintf(again_path, sizeof again_path, "%s/structs-again.map", work_dir);

    write_text(map_path,
        /* The gate is here only so the program has a declared
         * entrance and is allowed to sit waiting. It feeds nothing:
         * the work goes straight into the shifter's buffer, which is
         * the state being captured. */
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "\n"
        "station shifter (nudge)\n"
        "  out 0 - 0$\n"
        "  in 1 -\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* Two vec3 values, each holding commas of its own. */
    check(cera_map_in_port_queue_text(m, 1, 0,
              "{ 1.5, 2.5, 3.5 }, { 4.5, 5.5, 6.5 }") == NULL,
          "two struct values were put into the buffer");
    check(atomic_load(&cera_map_station(m, 1)->in_ports[0].held) == 2,
          "and both of them are waiting");

    dump_to(m, dump_path);
    cera_map_t *revived = cera_map_load_file(dump_path, 2);
    check(atomic_load(&cera_map_station(revived, 1)->in_ports[0].held) == 2,
          "the revived program has both struct values waiting");

    dump_to(revived, again_path);
    check(strcmp(slurp(dump_path), slurp(again_path)) == 0,
          "and writing it down again produced the same text, commas and "
          "all");

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    cera_map_destroy(m);
    cera_pool_release(revived->pool);
    cera_pool_join(revived->pool);
    cera_map_destroy(revived);

    printf("  a queue of struct values survived, inner commas and all\n");
}
/* }}} */

/* {{{ static void a_deep_buffer_drains_when_the_constant_arrives() */
/*
 * **Thirty values on one port and nothing on the other.** When the
 * other port finally holds something, all thirty are due — the one
 * rule says a station runs whenever every port holds a value, and it
 * says nothing about how many times in a row.
 *
 * Asking once was enough while values arrived one at a time, because
 * each arrival asked again. It stopped being enough the moment a port
 * could fill *all at once*: a constant being bound, a constant being
 * written, or a revived program putting a captured queue back. Then
 * the station is ready thirty times over and a single ask starts one.
 *
 * This is the scene that says which of those three doors is covered,
 * because they are three different call paths into the same question.
 */
static void a_deep_buffer_drains_when_the_constant_arrives(void)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/deep.map", work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "\n"
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 x64\n"
        "  in 1 -\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    for (int v = 0; v < 30; v++) {
        int value = v;
        cera_map_deliver_value(m, 1, 0, &value);
    }
    check(atomic_load(&cera_map_station(m, 1)->in_ports[0].held) == 30,
          "thirty values are waiting on one port");
    check(atomic_load(&cera_map_station(m, 1)->runs) == 0,
          "and none of them can run, because the other port is empty");

    /* Door one: a constant bound to the empty port. */
    check(cera_map_configure_port(m, 1, 1, CERA_IN_PORT_STATIC, "1") == NULL,
          "a constant was bound to the port that was empty");
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    check(atomic_load(&cera_map_station(m, 1)->runs) == 30,
          "and all thirty ran, not one");

    cera_map_destroy(m);
    printf("  thirty values waiting on one port all ran when the other "
           "port was filled\n");
}
/* }}} */

/* {{{ static void writing_a_constant_drains_what_was_waiting() */
/*
 * Door two: the constant already exists and is **written** while the
 * program runs. The write and the first readiness check happen inside
 * one lock hold on purpose, so there is no gap between the value
 * changing and the question being asked — and then the asking has to
 * continue, for the same reason as above.
 */
static void writing_a_constant_drains_what_was_waiting(void)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/written.map", work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "\n"
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 x64\n"
        "  in 1 = 1\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* The constant is already there, so each delivery starts one task
     * as it lands — which is the ordinary path and needs no help.
     * Deliver with the workers parked so the work piles up instead. */
    for (int v = 0; v < 30; v++) {
        int value = v;
        cera_map_deliver_value(m, 1, 0, &value);
    }

    /* Now change the constant. Nothing is waiting by this point,
     * because every delivery started its own task, so what this
     * proves is the narrower half: writing does not strand anything
     * and does not start a station twice for one change. */
    long before = atomic_load(&cera_map_station(m, 1)->runs);
    int fresh = 5;
    cera_map_in_port_static_write(m, 1, 1, &fresh, (int)sizeof fresh);

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    check(atomic_load(&cera_map_station(m, 1)->runs) == 30,
          "thirty deliveries produced thirty runs and no more");
    check(before <= 30, "and none of them ran twice");

    cera_map_destroy(m);
    printf("  writing a constant left nothing stranded and started "
           "nothing twice\n");
}
/* }}} */

/* {{{ static void a_station_of_only_constants_runs_once_per_change() */
/*
 * **A station with no buffer at all is ready forever**, because a
 * constant is never consumed. So it is asked once when it becomes
 * complete, and once more each time a constant on it changes — which
 * is what "run again because something changed" has to mean where
 * there is nothing to drain.
 *
 * Looping on such a station would never stop, which is why the drain
 * declines to touch it rather than relying on a count.
 */
static void a_station_of_only_constants_runs_once_per_change(void)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/constants.map", work_dir);

    write_text(map_path,
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 = 2\n"
        "  in 1 = 3\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    check(atomic_load(&cera_map_station(m, 0)->runs) == 1,
          "a station of only constants ran exactly once");

    cera_map_t *again = cera_map_load_file(map_path, 2);
    int fresh = 10;
    cera_map_in_port_static_write(again, 0, 0, &fresh, (int)sizeof fresh);
    cera_map_in_port_static_write(again, 0, 1, &fresh, (int)sizeof fresh);
    cera_pool_release(again->pool);
    cera_pool_join(again->pool);
    check(atomic_load(&cera_map_station(again, 0)->runs) == 3,
          "and once more for each change to a constant on it — never "
          "looping, which it would do forever");

    cera_map_destroy(m);
    cera_map_destroy(again);
    printf("  a station of only constants ran once, and once again per "
           "change\n");
}
/* }}} */

/* {{{ static void writing_the_same_value_still_counts() */
/*
 * **Writing the value a port already holds is still a write.**
 *
 * A write is a statement — the value is now this — rather than a
 * report of a difference. In a graph of stations wired through
 * constants, "recompute with this" is what the caller asked for, and
 * whether the bytes happen to match what was there is a fact about the
 * previous value, which the caller said nothing about.
 *
 * This scene exists because skipping an identical write is exactly the
 * kind of thing somebody adds later as an optimization, and it would
 * make the same call do two different things depending on history.
 *
 * **What it catches is the comparison placed before the readiness
 * check**, which is where it would really be written — and checked by
 * putting it there and watching this fail. A comparison inside the
 * copy itself does *not* fail this scene, because the check runs
 * either way and the station still starts. That is worth knowing: the
 * thing being protected is the asking, not the copying.
 *
 * A comparison would not even be reliable: a struct arrives as raw
 * bytes with its padding, so two writes meaning one value can differ
 * where nobody wrote anything, and a genuine change that left the
 * compared bytes alone would be missed.
 */
static void writing_the_same_value_still_counts(void)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/unchanged.map", work_dir);

    write_text(map_path,
        "station adder (add)\n"
        "  out 0 - 0$\n"
        "  in 0 = 2\n"
        "  in 1 = 3\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* Three writes of the value already there. */
    int same = 3;
    for (int i = 0; i < 3; i++)
        cera_map_in_port_static_write(m, 0, 1, &same, (int)sizeof same);

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    /* One for becoming complete, one for each write. */
    check(atomic_load(&cera_map_station(m, 0)->runs) == 4,
          "writing the value a port already held ran the station again, "
          "every time");

    cera_map_destroy(m);
    printf("  writing a constant its own value over again counted every "
           "time\n");
}
/* }}} */

/* {{{ static void a_write_drains_whatever_is_waiting() */
/*
 * The other half of the same rule: after a write, whatever is waiting
 * runs — **and only if every port has something**, which is the one
 * rule and is not relaxed here.
 *
 * The station has two buffers and a constant. Values stack up on one
 * buffer while the other stays empty, so nothing can run however many
 * times the constant is written. When the second buffer finally gets
 * values, the pairs run and the surplus stays waiting, because the
 * rule was never about the constant.
 */
static void a_write_drains_whatever_is_waiting(void)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/stalled.map", work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "\n"
        "station maker (stamp_record)\n"
        "  out 0 - 0$\n"
        "  in 0 x64\n"
        "  in 1 x64\n"
        "  in 2 = 7\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* Ten on the first buffer, nothing on the second. */
    for (int v = 0; v < 10; v++) {
        int value = v;
        cera_map_deliver_value(m, 1, 0, &value);
    }

    /* Writing the constant cannot start anything: a port is empty, and
     * that is the whole of the rule. */
    unsigned long seven = 7;   /* stamp_record's third parameter */
    cera_map_in_port_static_write(m, 1, 2, &seven, (int)sizeof seven);
    check(atomic_load(&cera_map_station(m, 1)->runs) == 0,
          "writing a constant started nothing while a port was empty");
    check(atomic_load(&cera_map_station(m, 1)->in_ports[0].held) == 10,
          "and left all ten waiting");

    /* Four on the second buffer: four pairs are due, six wait on. */
    for (int v = 0; v < 4; v++) {
        vec3 where = { (float)v, 0.0f, 0.0f };
        cera_map_deliver_value(m, 1, 1, &where);
    }
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    check(atomic_load(&cera_map_station(m, 1)->runs) == 4,
          "four values on the second buffer paired with four of the ten");
    check(atomic_load(&cera_map_station(m, 1)->in_ports[0].held) == 6,
          "and the other six are still waiting, because the rule is every "
          "port and not merely the constant");

    cera_map_destroy(m);
    printf("  a write started nothing while a port was empty, and the "
           "surplus stayed waiting\n");
}
/* }}} */

/* {{{ static void an_iterator_remembers_where_it_was() */
/*
 * **The one memory a station keeps.** An iterator takes its exits in
 * turn, and which one is next is the only thing about a station that
 * is not either its shape or the values sitting on it.
 *
 * A capture that reset it would produce a program of the right shape
 * that sent the next value to an exit it was never going to — right
 * schematic, wrong behaviour, which is the worst way for a capture to
 * be wrong, because nothing about the file looks incorrect.
 */
static void an_iterator_remembers_where_it_was(void)
{
    char map_path[512], dump_path[512], again_path[512];
    snprintf(map_path, sizeof map_path, "%s/spread.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/spread-captured.map", work_dir);
    snprintf(again_path, sizeof again_path, "%s/spread-again.map", work_dir);

    /* One iterator with three exits, each landing somewhere that
     * keeps what it is given. */
    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "\n"
        "iterator spread (double_it)\n"
        "  out 0 - first.0\n"
        "  out 1 - second.0\n"
        "  out 2 - third.0\n"
        "\n"
        "station first (keep)\n"
        "  out 0 - 0$\n"
        "  in 0 - spread.0\n"
        "station second (keep)\n"
        "  in 0 - spread.1\n"
        "station third (keep)\n"
        "  in 0 - spread.2\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);

    /* Two values through it, so the next exit is the third. */
    for (int v = 0; v < 2; v++) {
        int value = v;
        cera_map_deliver_value(m, 1, 0, &value);
    }
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);
    check(cera_map_station(m, 1)->cursor == 2,
          "after two values the iterator is pointing at its third exit");

    dump_to(m, dump_path);
    check(strstr(slurp(dump_path), "@2") != NULL,
          "and the capture wrote where it had got to");

    cera_map_t *revived = cera_map_load_file(dump_path, 2);
    check(cera_map_station(revived, 1)->cursor == 2,
          "the revived program's iterator is pointing at the same exit");

    dump_to(revived, again_path);
    check(strcmp(slurp(dump_path), slurp(again_path)) == 0,
          "and writing it down again produced the same text");

    cera_map_destroy(m);
    cera_pool_release(revived->pool);
    cera_pool_join(revived->pool);
    cera_map_destroy(revived);

    printf("  an iterator was revived pointing where it had got to\n");
}
/* }}} */

/* {{{ static void draining_produces_a_complete_capture() */
/*
 * **The polite capture.** Shut the entrance, let everything in flight
 * finish and deliver, wait for the workers to go home, then write.
 * What comes out is complete by construction — no task was running
 * when it was written, so nothing could have been lost.
 *
 * There is no bound on the wait and that is deliberate: quiet is
 * decidable exactly, so the only case that never returns is a box that
 * never returns, and nothing inside the process can tell that from a
 * box that is merely slow. Whoever asked already has a clock.
 */
static void draining_produces_a_complete_capture(void)
{
    char map_path[512], dump_path[512];
    snprintf(map_path, sizeof map_path, "%s/flowing.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/flowing-captured.map",
             work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - twice.0\n"
        "\n"
        "station twice (double_it)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);
    for (int v = 0; v < 5; v++) {
        int value = v;
        cera_map_deliver_argument(m, 0, 0, &value, (int)sizeof value);
    }

    check(cera_capture(m, dump_path) == 0, "the program was captured");

    const char *text = slurp(dump_path);
    check(strstr(text, "INCOMPLETE") == NULL,
          "and the capture says nothing about being incomplete, because "
          "it drained first");
    check(atomic_load(&cera_map_station(m, 1)->runs) == 5,
          "everything in flight finished before it was written");

    /* And it reads back through the ordinary door, which an
     * incomplete one would not. */
    cera_map_t *revived = cera_map_load_file(dump_path, 2);
    check(revived != NULL, "and a complete capture reads back plainly");
    cera_pool_release(revived->pool);
    cera_pool_join(revived->pool);
    cera_map_destroy(revived);

    cera_map_destroy(m);
    printf("  draining before writing produced a capture with nothing "
           "missing\n");
}
/* }}} */

/* {{{ static void an_incomplete_capture_says_so_and_is_refused() */
/*
 * **The other half, and the one that matters more.** A program that
 * cannot drain is exactly when a capture is worth most — so the
 * writing happens anyway, and the artifact states what it lost rather
 * than leaving it to be discovered.
 *
 * The wedged box never returns, which is the one condition this engine
 * cannot detect from inside. It runs in a forked child because a
 * worker stuck in it stays stuck for the life of the process, and this
 * test has more to do afterwards.
 */
static void an_incomplete_capture_says_so_and_is_refused(void)
{
    char map_path[512], dump_path[512];
    snprintf(map_path, sizeof map_path, "%s/wedged.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/wedged-captured.map",
             work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - stuck.0\n"
        "\n"
        "station stuck (wedge)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n");

    pid_t child = fork();
    if (child == 0) {
        cera_map_t *m = cera_map_load_file(map_path, 1);
        int value = 1;
        cera_map_deliver_argument(m, 0, 0, &value, (int)sizeof value);
        cera_pool_release(m->pool);

        /* Wait until the one worker is actually inside the wedge,
         * rather than guessing. Nothing here invents a clock: it asks
         * the pool what its worker is doing until the answer is the
         * station that never returns. */
        for (;;) {
            int at = cera_pool_worker_station(m->pool, 0);
            if (at == 1)
                break;
        }

        /* Not the polite one: draining would never return, which is
         * the whole reason this door exists. */
        _exit(cera_capture_now(m, dump_path) == 0 ? 0 : 1);
    }
    check(child > 0, "a child was forked to hold the wedged worker");
    if (child <= 0)
        return;

    int status = 0;
    waitpid(child, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "and it wrote its capture while a worker was stuck");

    const char *text = slurp(dump_path);
    check(strstr(text, "# INCOMPLETE CAPTURE") != NULL,
          "the artifact says at the top that it is incomplete");
    check(strstr(text, "stuck") != NULL,
          "and names the station whose work was lost, by the name its "
          "author gave it");

    printf("  a program that could not drain was captured anyway, and "
           "said what it lost\n");
}
/* }}} */

/* {{{ static void reviving_a_lossy_capture_is_refused() */
/*
 * **Refused through the ordinary door, allowed through a door with a
 * different name.** A program picked up from an incomplete capture is
 * quietly missing results somebody computed, and quietly is the part
 * this engine refuses everywhere.
 *
 * The refusal is fatal, so it is proven in a child; the salvage is
 * ordinary and is proven here.
 */
static void reviving_a_lossy_capture_is_refused(const char *self)
{
    char lossy_path[512];
    snprintf(lossy_path, sizeof lossy_path, "%s/lossy.map", work_dir);

    /* Written by hand rather than captured, so what is being tested is
     * the reading and not the writing. */
    write_text(lossy_path,
        "# INCOMPLETE CAPTURE\n"
        "# 1 task was still running and did not finish:\n"
        "#   stuck  (station 1)\n"
        "\n"
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - twice.0\n"
        "\n"
        "station twice (double_it)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n");

    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s --load-lossy %s", self, lossy_path);
    int rc = system(cmd);
    check(rc != 0, "reading an incomplete capture the ordinary way was "
                   "refused");

    /* And salvaging it works, having said so out loud. */
    cera_map_t *salvaged = cera_map_load_salvage(lossy_path, 2);
    check(salvaged != NULL, "and salvaging the same file worked");
    if (salvaged) {
        cera_pool_release(salvaged->pool);
        cera_pool_join(salvaged->pool);
        cera_map_destroy(salvaged);
    }

    printf("  an incomplete capture was refused by the ordinary door and "
           "let through the one that names the risk\n");
}
/* }}} */

/* {{{ static void a_grown_program_captures_whole() */
/*
 * **A program that grew boxes cannot be captured as a description
 * alone**, and this is the scene that says why in one place.
 *
 * Such a program is made of more than its build compiled. Its
 * description names a box whose source exists nowhere on the machine
 * that reads it — so the description is a perfectly good file naming a
 * function nobody has.
 *
 * The whole capture is therefore a **directory**: the description, and
 * beside it every source the program is made of, at the paths the
 * description addresses them by. Building that needs the engine, which
 * is what building anything with this engine needs; the binary that
 * comes out needs no toolchain of its own, because by then every box
 * is compiled in like any other.
 */
static void a_grown_program_captures_whole(void)
{
    static const char source[] =
        "int quadruple(int x)\n"
        "{\n"
        "    return x * 4;\n"
        "}\n";

    check(cera_late_compile_source(source) == 1,
          "a box arrived after the program started");

    char map_path[512], out_dir[192], described[512], probe[1024];
    snprintf(map_path, sizeof map_path, "%s/grown.map", work_dir);
    snprintf(out_dir, sizeof out_dir, "%s/whole", work_dir);

    write_text(map_path,
        "station gate (keep)\n"
        "  in 0 - 0$\n"
        "  out 0 - four.0\n"
        "\n"
        "station four (quadruple)\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n");

    cera_map_t *m = cera_map_load_file(map_path, 2);
    int value = 3;
    cera_map_deliver_argument(m, 0, 0, &value, (int)sizeof value);

    check(cera_capture_whole(m, out_dir) == 0,
          "the grown program was captured whole");

    /* The description is there. */
    snprintf(described, sizeof described, "%s/program.map", out_dir);
    FILE *f = fopen(described, "r");
    check(f != NULL, "and the directory holds its description");
    if (f)
        fclose(f);

    /* And so is the source of the box the build never saw. Found by
     * asking the running program what that box is filed under, rather
     * than by knowing where the compiler happened to put it. */
    const cera_box_place_t *row = cera_box_place_find("quadruple");
    check(row != NULL, "the late box is placeable by name");
    if (row) {
        const char *colon = strrchr(row->address, ':');
        check(colon != NULL, "and its address names a file");
        if (colon) {
            snprintf(probe, sizeof probe, "%s/%.*s", out_dir,
                     (int)(colon - row->address), row->address);
            FILE *g = fopen(probe, "r");
            check(g != NULL,
                  "and that file was written into the capture, so the "
                  "description does not name a function nobody has");
            if (g)
                fclose(g);
        }
    }

    /* The sources the build compiled in are there too, because a
     * capture that stands alone cannot assume which half of itself
     * somebody already has. */
    snprintf(probe, sizeof probe, "%s/src/boxes/029-demo-boxes.c", out_dir);
    FILE *h = fopen(probe, "r");
    check(h != NULL, "along with the sources the build compiled in");
    if (h)
        fclose(h);

    /*
     * And the report a person reads, beside the description rather
     * than inside it. Nothing here is measured for the report: every
     * number in it was already being kept.
     */
    snprintf(probe, sizeof probe, "%s/report.txt", out_dir);
    const char *report = slurp(probe);
    check(strstr(report, "quadruple") != NULL,
          "the report names the box that arrived while it ran");
    check(strstr(report, "boxes that arrived while it ran: 1") != NULL,
          "and counts it, which is the half of what a program is made of "
          "that no build knows about");
    check(strstr(report, "four (station 1)") != NULL,
          "and says what each station did, by the name its author gave it");

    cera_map_destroy(m);
    printf("  a program that grew a box was captured whole, with a report "
           "of what it had done\n");
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    const char *self = argv[0];

    /* The child of the refusal test does one thing and dies trying. */
    if (argc == 3 && strcmp(argv[1], "--load-lossy") == 0) {
        cera_map_load_file(argv[2], 1);
        return 0;   /* not reached: the load is fatal */
    }

    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/capture-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot make %s\n", work_dir);
        return 1;
    }

    work_in_flight_survives();
    the_revived_program_finishes_the_work();
    a_struct_queue_survives();
    a_deep_buffer_drains_when_the_constant_arrives();
    writing_a_constant_drains_what_was_waiting();
    a_station_of_only_constants_runs_once_per_change();
    writing_the_same_value_still_counts();
    a_write_drains_whatever_is_waiting();
    an_iterator_remembers_where_it_was();
    draining_produces_a_complete_capture();
    an_incomplete_capture_says_so_and_is_refused();
    reviving_a_lossy_capture_is_refused(self);
    a_grown_program_captures_whole();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        fprintf(stderr, "could not clean up %s\n", work_dir);

    if (failures) {
        fprintf(stderr, "%d capture checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
