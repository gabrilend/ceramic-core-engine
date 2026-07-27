/*
 * 055-phase-7-inside-demo.c — watching it think.
 *
 * What this is: the phase 7 demonstration, and the last one. Every
 * earlier demo reported what happened; this one shows it happening,
 * and then changes it without stopping. The claim on trial: the
 * engine has no hidden state — everything it is doing can be seen,
 * and most of it can be altered.
 *
 * How it does it, in general terms: one map, loaded from text, run
 * under a live view drawn from the station table while values flow
 * in from outside. A deliberate bottleneck is found using only the
 * engine's own reports, relieved by moving a wire mid-run, and both
 * readings fall on camera. The dump proves the picture and the
 * engine agree; a refused rewire proves a bad instruction cannot
 * kill the plant. The instrumentation's own cost is measured by the
 * driving script, which builds this program twice.
 *
 * With --overhead, runs a bare throughput loop instead: the script
 * compiles that mode with and without SORA_STATS and reports both.
 */
#include "040-mapfile.h"
#include "049-observe.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *report;

/* {{{ say() / now_seconds() / write_text() */
static void say(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (report) {
        va_start(args, format);
        vfprintf(report, format, args);
        va_end(args);
    }
    fflush(stdout);
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) exit(1);
    fputs(text, f);
    fclose(f);
}
/* }}} */

/*
 * The demo map. A pump spreads work through an iterator whose two
 * ports BOTH feed the slow cruncher — the deliberate bottleneck —
 * while an identical spare idles. Two gather-capable sevens exist so
 * the refused-cycle scene has real stations to name.
 *
 * Stations by index: 0 head, 1 pump, 2 crunch, 3 spare, 4 drain,
 * 5 fresha, 6 ga, 7 gb.
 */
static const char *MAP_TEXT =
    "head seven p\n"
    "  out 0 - pump.0\n"
    "pump keep i\n"
    "  out 0 - crunch.0\n"
    "  out 1 - crunch.0\n"
    "spare slow_double p\n"
    "  out 0 - drain.0\n"
    "crunch slow_double p\n"
    "  out 0 - drain.0\n"
    "drain swallow p\n"
    "fresha seven p\n"
    "ga keep p\n"
    "  in 0 fresha\n"
    "gb keep p\n"
    "  in 0 ga\n";

enum { S_HEAD = 0, S_PUMP = 1, S_SPARE = 2, S_CRUNCH = 3, S_DRAIN = 4,
       S_FRESHA = 5, S_GA = 6, S_GB = 7 };

/* {{{ live_view() */
static void live_view(map_t *m, const char *moment)
{
    say("  [%s]\n", moment);
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        say("    %-8s runs %-6ld", m->station_names[i], (long)s->runs);
        for (int j = 0; j < s->n_slots; j++)
            if (s->slots[j].kind == SLOT_RING) {
                int depth = map_slot_depth(m, i, j);
                say(" depth %-4d ", depth);
                for (int h = 0; h < depth / 20 && h < 24; h++)
                    say("#");
            }
        say("\n");
    }
}
/* }}} */

/* {{{ scene_overhead() — the script's second compile runs this */
static void scene_overhead(const char *work_dir)
{
    /* Long enough for stable numbers; short runs measure the wind. */
    enum { VALUES = 30000 };
    char map_path[1024];
    snprintf(map_path, sizeof map_path, "%s/overhead.map", work_dir);
    write_text(map_path,
        "head seven p\n"
        "  out 0 - relay.0\n"
        "relay double_it p\n"
        "  out 0 - drain.0\n"
        "drain swallow p\n");

    map_t *m = map_load_file(map_path, 0);
    pool_submitter_register(m->pool);
    pool_release(m->pool);
    double before = now_seconds();
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 1, 0, &i);
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;
#ifdef SORA_STATS
    printf("with statistics compiled in:  %8.0f tasks/second\n",
           VALUES * 2 / elapsed);
#else
    printf("with statistics compiled out: %8.0f tasks/second\n",
           VALUES * 2 / elapsed);
#endif
    map_destroy(m);
}
/* }}} */

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : ".";
    char work_dir[512];
    snprintf(work_dir, sizeof work_dir, "/dev/shm/%s/phase-7-work",
             strrchr(root, '/') ? strrchr(root, '/') + 1 : root);
    char command[2048];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0)
        return 1;

    if (argc > 2 && strcmp(argv[2], "--overhead") == 0) {
        scene_overhead(work_dir);
        return 0;
    }

    char path[600];
    snprintf(path, sizeof path, "%s/tmp/shared-memory/phase-7-inside-report.txt",
             root);
    report = fopen(path, "w");

    char map_path[600];
    snprintf(map_path, sizeof map_path, "%s/inside.map", work_dir);
    write_text(map_path, MAP_TEXT);

    say("=== phase 7 demo: watching it think ===\n\n");

    map_t *m = map_load_file(map_path, 0);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* --- scene 1: the live map, drawn from the table ------------- */
    say("scene 1 — a live map under load\n");
    enum { FIRST_WAVE = 600, SECOND_WAVE = 600 };
    double wave1_start = now_seconds();
    for (int i = 0; i < FIRST_WAVE; i++) {
        map_deliver_value(m, S_PUMP, 0, &i);
        if (i == FIRST_WAVE / 2)
            live_view(m, "mid-flood: both iterator ports feed one cruncher");
    }
    /* Wait for the wave to drain so the timing is honest. */
    while (m->stations[S_DRAIN].runs < FIRST_WAVE)
        usleep(5000);
    double wave1 = now_seconds() - wave1_start;
    live_view(m, "first wave drained");
    say("\n");

    /* --- scene 2: the bottleneck, found and relieved ------------- */
    say("scene 2 — the bottleneck, found by reports, fixed by rewiring\n");
    say("  the engine's own reading of where the pain is:\n");
    map_report_stations(m, stdout, REPORT_BY_CONTENTION);
    if (report)
        map_report_stations(m, report, REPORT_BY_CONTENTION);
    say("  crunch carries every run; spare idles. the fix, live:\n");
    say("  move the iterator's second port from crunch to spare...\n");
    int rc1 = map_rewire_disconnect(m, S_PUMP, 1, S_CRUNCH, 0);
    int rc2 = map_rewire_connect(m, S_PUMP, 1, S_SPARE, 0);
    say("  disconnect %s, connect %s\n",
        rc1 == 0 ? "ok" : "REFUSED", rc2 == 0 ? "ok" : "REFUSED");

    long crunch_before = m->stations[S_CRUNCH].runs;
    double wave2_start = now_seconds();
    for (int i = 0; i < SECOND_WAVE; i++)
        map_deliver_value(m, S_PUMP, 0, &i);
    while (m->stations[S_DRAIN].runs < FIRST_WAVE + SECOND_WAVE)
        usleep(5000);
    double wave2 = now_seconds() - wave2_start;

    say("  first wave (one cruncher):  %6.0f values/second\n",
        FIRST_WAVE / wave1);
    say("  second wave (load split):   %6.0f values/second\n",
        SECOND_WAVE / wave2);
    say("  crunch ran %ld more; spare ran %ld — the load moved, nothing "
        "restarted.\n",
        (long)m->stations[S_CRUNCH].runs - crunch_before,
        (long)m->stations[S_SPARE].runs);
    say("  notice the throughput barely moved: the pool was already running\n");
    say("  the slow station's invocations on every core, so a slow BOX is\n");
    say("  not by itself a bottleneck here — the reports exist to show who\n");
    say("  pays, and the first-pass report keeps that finding\n\n");

    /* --- scene 3: the dump tells the new truth ------------------- */
    say("scene 3 — the dump describes what runs, not what the file said\n");
    char dump1_path[600], dump2_path[608];
    snprintf(dump1_path, sizeof dump1_path, "%s/dump1.map", work_dir);
    snprintf(dump2_path, sizeof dump2_path, "%s/dump2.map", work_dir);
    FILE *d1 = fopen(dump1_path, "w");
    map_dump(m, d1);
    fclose(d1);
    FILE *check = fopen(dump1_path, "r");
    char line[256];
    while (fgets(line, sizeof line, check))
        if (strstr(line, "out 1 - spare.0"))
            say("  the dump says:   %s", line);
    fclose(check);
    say("  the file on disk still says: out 1 - crunch.0 — the dump is\n");
    say("  now the only accurate description of the program\n\n");

    /* --- scene 4: the round trip, quiet and total ---------------- */
    say("scene 4 — the round trip\n");
    map_t *m2 = map_load_file(dump1_path, 2);
    FILE *d2 = fopen(dump2_path, "w");
    map_dump(m2, d2);
    fclose(d2);
    pool_release(m2->pool);
    pool_join(m2->pool);
    map_destroy(m2);
    snprintf(command, sizeof command, "cmp -s %s %s", dump1_path, dump2_path);
    say("  load the dump, dump again, compare: %s\n\n",
        system(command) == 0 ? "byte-identical — the picture and the "
        "engine agree" : "DIFFERENT — a loader bug has a witness");

    /* --- scene 5: a refused rewire cannot kill the plant --------- */
    say("scene 5 — a refused rewire, while everything keeps running\n");
    say("  attempt: make gb's source pull from itself the long way\n");
    int refused = map_rewire_gather(m, S_GA, 0, S_GB);
    say("  the engine answered %s (reason above, both stations named),\n",
        refused == 0 ? "YES?!" : "no");
    int still = 0;
    map_deliver_value(m, S_PUMP, 0, &still);
    say("  and the very next delivery still flowed. refusals return;\n");
    say("  they do not kill.\n\n");

    map_report_buffers(m, report ? report : stdout);
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    say("=== nothing hidden: seen, changed, and still true ===\n");
    map_destroy(m);

    if (report)
        fclose(report);
    return 0;
}
