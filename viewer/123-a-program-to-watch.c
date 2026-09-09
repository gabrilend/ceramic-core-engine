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
 * usage: a-program-to-watch --trail=<ring> [--pace=ms] [--view[=port]] [--listen=all]
 *
 * `--view` starts the viewer as a child, so watching is one command
 * rather than two terminals. It stays a separate process on purpose:
 * putting the server inside would give the program a thread, a socket
 * and clients, which is exactly what "the program never waits for a
 * reader and never learns one is there" refuses.
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
    int pace_ms = 120;
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
    must_take(cera_map_designate_argument(m, in, 0, 0), "an entrance");

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
    must_take(cera_map_designate_result(m, total, 0, 0), "a result");

    must_take(cera_map_wire(m, in, 0, twice, 0), "a wire");
    must_take(cera_map_wire(m, in, 0, plus, 0), "a wire");
    must_take(cera_map_wire(m, twice, 0, total, 0), "a wire");
    must_take(cera_map_wire(m, plus, 0, total, 1), "a wire");

    cera_map_start(m, 4);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");
    /* Registered before the workers are let go: a result that arrives
     * before somebody has said where to put it is discarded like any
     * other unwired value. */
    static int kept[KEPT];
    must_take(cera_map_collect(m, total, 0, kept, KEPT, (int)sizeof kept[0]),
              "somewhere to put the results");

    cera_pool_release(m->pool);

    /*
     * The viewer, started as a child if asked for. It is handed the
     * trail this program is writing and a map of the same shape; it
     * reads both and can reach neither this program nor anything else.
     */
    pid_t viewer = -1;
    if (view_port > 0) {
        /*
         * **The program draws itself.** A trail carries station
         * indices, and the page turns an index into a box by counting
         * down the map file — so a map of a different shape, or the
         * same shape in another order, would draw a confident lie.
         * Dumping this program's own graph removes the question: what
         * the viewer reads is what is running, by construction.
         */
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

        char port_arg[32], trail_arg[600], map_arg[600], root_arg[600];
        char self[600], listen_arg[64];
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
        /* Let it bind before saying where to look. */
        struct timespec settle = { 0, 300000000L };
        nanosleep(&settle, NULL);
    }

    if (view_port > 0)
        printf("watching at http://localhost:%d/\n", view_port);
    else {
        printf("watch it with:\n");
        printf("  119-viewer --trail=%s --map=<a map of this shape>\n", trail);
    }
    printf("feeding one value every %dms — interrupt to stop\n", pace_ms);
    fflush(stdout);

    struct timespec gap = { pace_ms / 1000, (long)(pace_ms % 1000) * 1000000L };
    int value = 0, drained = 0;

    while (!stopping) {
        value++;
        must_take(cera_map_deliver_argument(m, in, 0, &value, sizeof value),
                  "an argument");

        /* Nothing piles up any more — a value reaching a result goes
         * straight into the caller's array — so this is a reading of
         * how many have arrived rather than an act of taking them
         * away. */
        drained = cera_map_collected(m, total, 0);
        nanosleep(&gap, NULL);
    }

    printf("\nstopping after %d values, %d results collected\n", value, drained);
    if (viewer > 0) {
        kill(viewer, SIGTERM);
        waitpid(viewer, NULL, 0);
    }
    cera_pool_submitter_unregister(m->pool);
    cera_map_destroy(m);
    return 0;
}
/* }}} */
