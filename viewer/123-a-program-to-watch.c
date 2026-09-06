/*
 * 123-a-program-to-watch.c — a program that keeps going, so there is
 * something to point the viewer at.
 *
 * What this is: the engine's own example graph, opened with a trail and
 * fed at a human pace instead of as fast as the machine will go. A
 * program that finishes in four milliseconds is correct and useless to
 * watch; this one comes up, works steadily, occasionally lets a buffer
 * fall behind on purpose, and waits to be interrupted.
 *
 * It exists for the viewer and is not part of the engine.
 *
 * usage: a-program-to-watch --trail=<ring> [--pace=ms]
 */
#include "cera.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* {{{ static volatile sig_atomic_t stopping */
static volatile sig_atomic_t stopping = 0;
static void note_stop(int signo) { (void)signo; stopping = 1; }
/* }}} */

/* {{{ static void must_take(const char *refusal, const char *what) */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "  refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ int main(int argc, char **argv) */
int main(int argc, char **argv)
{
    const char *trail = NULL;
    int pace_ms = 120;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--trail=", 8) == 0) trail = argv[i] + 8;
        else if (strncmp(argv[i], "--pace=", 7) == 0) pace_ms = atoi(argv[i] + 7);
    }
    if (!trail) {
        fprintf(stderr, "usage: a-program-to-watch --trail=<ring> [--pace=ms]\n");
        return 2;
    }
    if (!cera_watch_compiled_in()) {
        fprintf(stderr, "built without CERA_WATCH, so there would be nothing "
                        "to watch\n");
        return 1;
    }

    signal(SIGINT, note_stop);
    signal(SIGTERM, note_stop);

    /*
     * The trail is opened on an empty program, before anything is
     * placed, so a watcher sees the graph being built as well as run.
     */
    cera_map_t *m = cera_map_create_empty();
    must_take(cera_watch_open(m, trail), "the trail");

    int in = cera_map_add_station(m);
    cera_map_place_box(m, in, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, in, "in"), "a name");
    must_take(cera_map_designate_input(m, in), "an entrance");

    int twice = cera_map_add_station(m);
    cera_map_place_box(m, twice, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, twice, "twice"), "a name");

    int plus = cera_map_add_station(m);
    cera_map_place_box(m, plus, "add", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, plus, "plus"), "a name");
    cera_map_in_port_static_text(m, plus, 1, "10");

    int total = cera_map_add_station(m);
    cera_map_place_box(m, total, "add", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, total, "total"), "a name");
    must_take(cera_map_designate_output(m, total), "a result");

    must_take(cera_map_wire(m, in, 0, twice, 0), "a wire");
    must_take(cera_map_wire(m, in, 0, plus, 0), "a wire");
    must_take(cera_map_wire(m, twice, 0, total, 0), "a wire");
    must_take(cera_map_wire(m, plus, 0, total, 1), "a wire");

    cera_map_start(m, 4);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");
    cera_pool_release(m->pool);

    printf("watch it with:\n");
    printf("  viewer --trail=%s --map=<a map of this shape>\n", trail);
    printf("feeding one value every %dms — interrupt to stop\n", pace_ms);
    fflush(stdout);

    struct timespec gap = { pace_ms / 1000, (long)(pace_ms % 1000) * 1000000L };
    int value = 0, drained = 0;

    while (!stopping) {
        value++;
        must_take(cera_map_deliver_argument(m, in, 0, &value, sizeof value),
                  "an argument");

        /*
         * Every twentieth round the results are left to pile up for a
         * while, so a watcher sees a backlog form and drain rather than
         * a picture that is always the same.
         */
        if (value % 20 != 0) {
            int got = 0;
            while (cera_map_output_take(m, total, &got, sizeof got))
                drained++;
        }
        nanosleep(&gap, NULL);
    }

    printf("\nstopping after %d values, %d results collected\n", value, drained);
    cera_pool_submitter_unregister(m->pool);
    cera_map_destroy(m);
    return 0;
}
/* }}} */
