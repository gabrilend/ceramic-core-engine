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
#include "018-station.h"
#include "026-emitted.h"
#include "040-mapfile.h"
#include "049-observe.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static char work_dir[256];

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
static void dump_to(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    map_dump(m, f);
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
        "station gate keep p entry\n"
        "  out 0 - adder.0\n"
        "\n"
        "station adder add p result\n"
        "  in 1 -\n");

    map_t *m = map_load_file(map_path, 2);

    /* Three values into the port that has a source, none of which can
     * make the station ready, because its other port has none. */
    for (int v = 1; v <= 3; v++) {
        int value = v * 100;
        /* What comes back says whether a *task* became due, which is
         * a fact about the station rather than about this value — and
         * here nothing can become due, because the adder's other port
         * is empty. The buffer depth below is what says it landed. */
        map_deliver_value(m, 1, 0, &value);
    }
    check(atomic_load(&map_station(m, 1)->in_ports[0].held) == 3,
          "three values are waiting and none of them could run");
    check(atomic_load(&map_station(m, 1)->runs) == 0,
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
    map_t *revived = map_load_file(dump_path, 2);
    check(atomic_load(&map_station(revived, 1)->in_ports[0].held) == 3,
          "the revived program has the same three values waiting");
    check(atomic_load(&map_station(revived, 1)->runs) == 0,
          "and has not run either, so nothing was consumed on the way");

    /* Captured twice is the same text, which is the round trip the
     * dump has always promised, now covering what a program holds and
     * not only what it is. */
    dump_to(revived, again_path);
    check(strcmp(slurp(dump_path), slurp(again_path)) == 0,
          "capturing the revived program produced the same text again");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    pool_release(revived->pool);
    pool_join(revived->pool);
    map_destroy(revived);

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
        "station gate keep p entry\n"
        "  out 0 - adder.0\n"
        "\n"
        "station adder add p result\n"
        "  in 1 -\n");

    map_t *m = map_load_file(map_path, 2);
    for (int v = 1; v <= 3; v++) {
        int value = v * 100;
        map_deliver_value(m, 1, 0, &value);
    }
    dump_to(m, dump_path);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    map_t *revived = map_load_file(dump_path, 2);

    /* Finish the port that was never wired, which is what the program
     * was waiting for all along. */
    check(map_configure_port(revived, 1, 1, IN_PORT_STATIC, "7") == NULL,
          "the missing port was finished on the revived program");
    check(map_bring_up(revived) == NULL,
          "and the revived program came up");

    pool_release(revived->pool);
    pool_join(revived->pool);

    check(atomic_load(&map_station(revived, 1)->runs) == 3,
          "all three pieces of work in flight ran after revival");

    /* And the answers are the ones the first program would have
     * produced: each value plus seven. */
    int seen[3] = { 0, 0, 0 }, got = 0;
    while (map_output_waiting(revived, 1) > 0 && got < 3) {
        int out = 0;
        map_output_take(revived, 1, &out, (int)sizeof out);
        seen[got++] = out;
    }
    check(got == 3, "and produced three results");
    int total = seen[0] + seen[1] + seen[2];
    check(total == (100 + 7) + (200 + 7) + (300 + 7),
          "which are the answers the captured work was waiting to give");

    map_destroy(revived);
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
        "station gate keep p entry\n"
        "\n"
        "station shifter nudge p result\n"
        "  in 1 -\n");

    map_t *m = map_load_file(map_path, 2);

    /* Two vec3 values, each holding commas of its own. */
    check(map_in_port_queue_text(m, 1, 0,
              "{ 1.5, 2.5, 3.5 }, { 4.5, 5.5, 6.5 }") == NULL,
          "two struct values were put into the buffer");
    check(atomic_load(&map_station(m, 1)->in_ports[0].held) == 2,
          "and both of them are waiting");

    dump_to(m, dump_path);
    map_t *revived = map_load_file(dump_path, 2);
    check(atomic_load(&map_station(revived, 1)->in_ports[0].held) == 2,
          "the revived program has both struct values waiting");

    dump_to(revived, again_path);
    check(strcmp(slurp(dump_path), slurp(again_path)) == 0,
          "and writing it down again produced the same text, commas and "
          "all");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    pool_release(revived->pool);
    pool_join(revived->pool);
    map_destroy(revived);

    printf("  a queue of struct values survived, inner commas and all\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
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
