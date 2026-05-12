/* src/008-pool-runner.c — phase 3 runner entry point.
 *
 * What it is, in a sentence: the C binary that loads a map directory,
 * stands up the thread pool, lets the dispatch layer run every box
 * to quiescence, and writes the JSONL run log on the way out.
 *
 * Designed in issue 301. Current state: scaffold. The eventual main
 * body, in pseudocode (see docs/HTML/001-architecture.html):
 *
 *   1.  parse map path from argv
 *   2.  load and validate graph    (issue 305 graph loader)
 *   3.  determine languages used   (issue 305)
 *   4.  allocate per-port slots    (issue 302 slot store)
 *   5.  pool = pool_create(N)      (issue 301 pool lifecycle)
 *   6.  init per-worker handles    (issue 303 spec registry)
 *   7.  pool_init_barrier(pool)    (issue 301)
 *   8.  push literal input values  (issue 304 dispatch)
 *   9.  spawn entry-box tasks      (issue 304)
 *   10. pool_wait_quiescent(pool)  (issue 301)
 *   11. write last-run.jsonl       (issue 311)
 *   12. pool_destroy(pool)         (issue 301)
 *
 * At this scaffolding stage we just parse argv, print a banner so
 * the build link is verified end-to-end, and exit 0.
 */

#include <stdio.h>
#include <string.h>

/* {{{ usage() */
static void usage(const char *progname)
{
    fprintf(stderr,
        "usage: %s <map-directory>\n"
        "\n"
        "Runs a SoraMech map. The map directory must contain meta.json\n"
        "and a boxes/ subdirectory.\n"
        "\n"
        "Phase 3 runtime is still under construction; this binary is\n"
        "currently a scaffold and prints what it would do.\n",
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

    fprintf(stderr, "soramech-pool: scaffold build.\n");
    fprintf(stderr, "  map directory: %s\n", map_dir);
    fprintf(stderr, "  runtime: not yet implemented (issues 301–311 pending).\n");
    fprintf(stderr, "  link verification: ok.\n");

    return 0;
}
/* }}} */
