/*
 * 128-watch-a-map.c — run any map this binary was built with, and watch it.
 *
 * What this is: the general form of the two hand-built programs beside
 * it. Every map file under maps/ is compiled into this binary, so this
 * takes a map's name, builds it, opens a trail, finds its entrance, and
 * feeds it at a watchable pace.
 *
 * How it finds where to feed: a map declares one station its entrance,
 * and that mark is on the station rather than in a table, so the
 * program looks for the station wearing it. A map with no entrance is
 * refused here rather than fed at random.
 *
 * usage: watch-a-map --map=<name.map> --trail=<ring> [--pace=ms] [--view[=port]] [--listen=all]
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

/* {{{ static void must_take(const char *refusal, const char *what) */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "  refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* How many results a run of this is willing to remember. A watcher
 * cares about the shape of a program rather than its output, so this
 * is a window on the stream and not a record of it. */
#define KEPT 4096

/* {{{ int main(int argc, char **argv) */
int main(int argc, char **argv)
{
    const char *which = NULL, *trail = NULL;
    int pace_ms = 120, view_port = 0;
    const char *listen_where = "local";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--map=", 6) == 0)        which = argv[i] + 6;
        else if (strncmp(argv[i], "--trail=", 8) == 0) trail = argv[i] + 8;
        else if (strncmp(argv[i], "--pace=", 7) == 0)  pace_ms = atoi(argv[i] + 7);
        else if (strncmp(argv[i], "--view=", 7) == 0)  view_port = atoi(argv[i] + 7);
        else if (strcmp(argv[i], "--view") == 0)       view_port = 8723;
        else if (strncmp(argv[i], "--listen=", 9) == 0) listen_where = argv[i] + 9;
    }
    if (!which || !trail) {
        fprintf(stderr, "usage: watch-a-map --map=<name.map> --trail=<ring> "
                        "[--pace=ms] [--view[=port]]\n");
        return 2;
    }
    if (!cera_watch_compiled_in()) {
        fprintf(stderr, "built without CERA_WATCH, so there would be nothing "
                        "to watch\n");
        return 1;
    }

    const cera_map_build_t *built = cera_map_build_find(which);
    if (!built) {
        fprintf(stderr, "this binary was not built with %s\n", which);
        return 1;
    }

    signal(SIGINT, note_stop);
    signal(SIGTERM, note_stop);

    /* The trail is opened before the map is built, so a watcher sees
     * the graph coming into existence as well as running. */
    cera_map_t *m = cera_map_create_empty();
    must_take(cera_watch_open(m, trail), "the trail");
    built->build(m, NULL, 0);

    cera_map_start(m, 4);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), which);

    int way_in = -1, in_port = 0;
    int way_out = -1, out_port = 0;
    if (!cera_map_argument_at(m, 0, &way_in, &in_port))
        way_in = -1;
    if (!cera_map_result_at(m, 0, &way_out, &out_port))
        way_out = -1;

    /* Registered before the workers are let go, because a result that
     * arrives before somebody has said where to put it is discarded
     * like any other unwired value. */
    static int kept[KEPT];
    if (way_out >= 0)
        must_take(cera_map_collect(m, way_out, out_port, kept, KEPT,
                                   (int)sizeof kept[0]),
                  "somewhere to put the results");

    cera_pool_release(m->pool);
    if (way_in < 0) {
        fprintf(stderr, "%s declares no entrance, so there is nowhere to "
                        "feed it\n", which);
        return 1;
    }

    pid_t viewer = -1;
    if (view_port > 0) {
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
        printf("watching %s at http://localhost:%d/\n", which, view_port);
    }

    printf("feeding one value every %dms — interrupt to stop\n", pace_ms);
    fflush(stdout);

    struct timespec gap = { pace_ms / 1000, (long)(pace_ms % 1000) * 1000000L };
    int value = 0, results = 0;

    while (!stopping) {
        must_take(cera_map_deliver_argument(m, way_in, in_port, &value,
                                            sizeof value),
                  "an argument");
        value++;

        if (way_out >= 0)
            results = cera_map_collected(m, way_out, out_port);
        nanosleep(&gap, NULL);
    }

    printf("\nstopping after %d values; %d results collected\n", value, results);
    if (viewer > 0) {
        kill(viewer, SIGTERM);
        waitpid(viewer, NULL, 0);
    }
    cera_pool_submitter_unregister(m->pool);
    cera_map_destroy(m);
    return 0;
}
/* }}} */
