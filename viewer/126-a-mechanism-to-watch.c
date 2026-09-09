/*
 * 126-a-mechanism-to-watch.c — a program built to show its own workings.
 *
 * What this is: a graph chosen so that the thing the engine is actually
 * doing is visible in the viewer rather than merely happening. Where
 * `123-a-program-to-watch.c` shows a program running, this one shows
 * **why a station waits**.
 *
 * The shape:
 *
 *     feed ──> gate ─┬─ less    ──────────────> collect.0
 *              (c)   ├─ equal   ──────────────> collect.1
 *                    └─ greater ──> spread ─┬─> collect.2
 *                                     (i)   └─> drain
 *
 * `feed` counts, so `gate` sees 0, 1, 2, … 29, 0, 1, … A comparator
 * routes by comparing its box's result against the threshold on its
 * last port, which is ten here. So of every thirty values, ten go left,
 * exactly one goes down the middle, and nineteen go right — and the
 * iterator halves the right-hand stream again by dealing alternate
 * values to a drain.
 *
 * `collect` takes three inputs and cannot run until all three hold a
 * value. It therefore runs at the rate of its slowest feed, which is
 * one in thirty, while the other two buffers grow without bound. That
 * is the whole demonstration: **memory quietly absorbing an imbalance**
 * is what a ring buffer does, and this is what it looks like.
 *
 * usage: a-mechanism-to-watch --trail=<ring> [--pace=ms] [--view[=port]] [--listen=all]
 */
#include "cera.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* {{{ static volatile sig_atomic_t stopping */
static volatile sig_atomic_t stopping = 0;
static void note_stop(int signo) { (void)signo; stopping = 1; }
/* }}} */

/* How many results a run of this is willing to remember. A watcher
 * cares about the shape of a program rather than its output, so this
 * is a window on the stream and not a record of it. */
#define KEPT 4096

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
    int pace_ms = 140;
    int view_port = 0;
    const char *listen_where = "local";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--trail=", 8) == 0) trail = argv[i] + 8;
        else if (strncmp(argv[i], "--pace=", 7) == 0) pace_ms = atoi(argv[i] + 7);
        else if (strncmp(argv[i], "--view=", 7) == 0) view_port = atoi(argv[i] + 7);
        else if (strcmp(argv[i], "--view") == 0) view_port = 8723;
        else if (strncmp(argv[i], "--listen=", 9) == 0) listen_where = argv[i] + 9;
    }
    if (!trail) {
        fprintf(stderr, "usage: a-mechanism-to-watch --trail=<ring> "
                        "[--pace=ms] [--view[=port]]\n");
        return 2;
    }
    if (!cera_watch_compiled_in()) {
        fprintf(stderr, "built without CERA_WATCH, so there would be nothing "
                        "to watch\n");
        return 1;
    }

    signal(SIGINT, note_stop);
    signal(SIGTERM, note_stop);

    cera_map_t *m = cera_map_create_empty();
    must_take(cera_watch_open(m, trail), "the trail");

    int feed = cera_map_add_station(m);
    cera_map_place_box(m, feed, "cycle_thirty", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, feed, "feed"), "a name");
    must_take(cera_map_designate_argument(m, feed, 0, 0), "an entrance");

    /* The comparator's own port count is its box's parameters plus one:
     * the threshold sits on the last port, and is set like any other
     * constant. */
    int gate = cera_map_add_station(m);
    cera_map_place_box(m, gate, "keep", CERA_STATION_COMPARATOR);
    must_take(cera_map_name_station(m, gate, "gate"), "a name");
    cera_map_in_port_static_text(m, gate, 1, "10");

    int spread = cera_map_add_station(m);
    cera_map_place_box(m, spread, "keep", CERA_STATION_ITERATOR);
    must_take(cera_map_name_station(m, spread, "spread"), "a name");

    int collect = cera_map_add_station(m);
    cera_map_place_box(m, collect, "three_way", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, collect, "collect"), "a name");
    must_take(cera_map_designate_result(m, collect, 0, 0), "a result");

    int drain = cera_map_add_station(m);
    cera_map_place_box(m, drain, "swallow", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, drain, "drain"), "a name");

    must_take(cera_map_wire(m, feed, 0, gate, 0), "a wire");
    must_take(cera_map_wire(m, gate, 0, collect, 0), "the less-than wire");
    must_take(cera_map_wire(m, gate, 1, collect, 1), "the equal wire");
    must_take(cera_map_wire(m, gate, 2, spread, 0), "the greater-than wire");
    must_take(cera_map_wire(m, spread, 0, collect, 2), "the iterator's first exit");
    must_take(cera_map_wire(m, spread, 1, drain, 0), "the iterator's second exit");

    cera_map_start(m, 4);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");
    /* Registered before the workers are let go: a result that arrives
     * before somebody has said where to put it is discarded like any
     * other unwired value. */
    static int kept[KEPT];
    must_take(cera_map_collect(m, collect, 0, kept, KEPT, (int)sizeof kept[0]),
              "somewhere to put the results");

    cera_pool_release(m->pool);

    pid_t viewer = -1;
    if (view_port > 0) {
        /* The program draws itself: what the viewer reads is this
         * program's own live graph, so an index in an event and a box
         * on the page cannot disagree. */
        char drawn[512];
        snprintf(drawn, sizeof drawn, "%s.map", trail);
        FILE *shape = fopen(drawn, "w");
        if (!shape) {
            fprintf(stderr, "cannot write the shape to %s: %s\n",
                    drawn, strerror(errno));
            return 1;
        }
        cera_map_dump(m, shape);
        fclose(shape);

        char port_arg[32], trail_arg[600], map_arg[600], root_arg[600], self[600];
        char listen_arg[64];
        snprintf(port_arg, sizeof port_arg, "--port=%d", view_port);
        snprintf(listen_arg, sizeof listen_arg, "--listen=%s", listen_where);
        snprintf(trail_arg, sizeof trail_arg, "--trail=%s", trail);
        snprintf(map_arg, sizeof map_arg, "--map=%s", drawn);
        snprintf(root_arg, sizeof root_arg, "--root=%s/viewer", CERA_ROOT);
        snprintf(self, sizeof self, "%s/tmp/build/119-viewer", CERA_ROOT);

        viewer = fork();
        if (viewer == 0) {
            execl(self, self, trail_arg, map_arg, root_arg, port_arg,
                  listen_arg, (char *)NULL);
            fprintf(stderr, "could not start the viewer at %s: %s\n",
                    self, strerror(errno));
            _exit(1);
        }
        struct timespec settle = { 0, 300000000L };
        nanosleep(&settle, NULL);
        printf("watching at http://localhost:%d/\n", view_port);
    }

    printf("of every thirty values: ten go left, one goes down the middle,\n"
           "and nineteen go right — where an iterator gives every other one\n"
           "to a drain. collect needs all three, so it runs about once every\n"
           "thirty while two of its buffers grow. interrupt to stop.\n");
    fflush(stdout);

    struct timespec gap = { pace_ms / 1000, (long)(pace_ms % 1000) * 1000000L };
    int value = 0, results = 0;

    while (!stopping) {
        must_take(cera_map_deliver_argument(m, feed, 0, &value, sizeof value),
                  "an argument");
        value++;

        results = cera_map_collected(m, collect, 0);

        nanosleep(&gap, NULL);
    }

    printf("\nstopping after %d values; collect ran %d times\n", value, results);
    if (viewer > 0) {
        kill(viewer, SIGTERM);
        waitpid(viewer, NULL, 0);
    }
    cera_pool_submitter_unregister(m->pool);
    cera_map_destroy(m);
    return 0;
}
/* }}} */
