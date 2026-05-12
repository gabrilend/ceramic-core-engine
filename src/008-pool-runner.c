/* src/008-pool-runner.c — phase 3 runner entry point.
 *
 * What it is, in a sentence: the C binary that loads a map directory,
 * stands up the thread pool, lets the dispatch layer run every box
 * to quiescence, and writes the JSONL run log on the way out.
 *
 * Current state: loads the graph (issue 305) and stands up the pool
 * (issue 301); the dispatch layer (issue 304) and the JSONL writer
 * (issue 311) still need to land before the runner actually runs
 * anything. Spawning + quiescence work — verifiable by running the
 * binary against tests/maps/hello.
 */

#include "010-graph-loader.h"
#include "pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ usage() */
static void usage(const char *progname)
{
    fprintf(stderr,
        "usage: %s <map-directory>\n"
        "\n"
        "Runs a SoraMech map. The map directory must contain meta.json\n"
        "and a boxes/ subdirectory.\n",
        progname);
}
/* }}} */

/* {{{ main() */
int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(argv[0]);
        return (argc < 2) ? 2 : 0;
    }

    const char *map_dir = argv[1];

    char *err = NULL;
    graph_t *g = graph_load(map_dir, &err);
    if (!g) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "load failed");
        free(err);
        return 1;
    }

    pool_t *p = pool_create(0);   /* default n_workers */
    if (!p) {
        fprintf(stderr, "soramech-pool: cannot create thread pool\n");
        graph_destroy(g);
        return 1;
    }
    pool_init_barrier(p);

    fprintf(stderr,
        "soramech-pool: '%s' loaded — %d box(es), entry %s, %d worker(s)\n",
        graph_name(g),
        graph_n_boxes(g),
        graph_entry_box_id(g) ? graph_entry_box_id(g) : "(none)",
        pool_n_workers(p));
    fprintf(stderr,
        "soramech-pool: dispatch layer not yet wired (issue 304); "
        "quiescing with no tasks.\n");

    pool_wait_quiescent(p);
    pool_destroy(p);
    graph_destroy(g);
    return 0;
}
/* }}} */
