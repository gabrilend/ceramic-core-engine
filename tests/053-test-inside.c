/*
 * 053-test-inside.c — proves phase 7's legibility (issues 701–704).
 *
 * What this is: the tests that a running map can be seen and
 * changed. The buffer report names the right slot; the station
 * report counts truly; a loaded map dumps to a file that loads to a
 * map that dumps identically; a wire changed mid-run changes
 * behaviour from that moment with nothing lost; and of two threads
 * drawing jointly-cyclic gather edges, exactly one is refused.
 *
 * How it does it, in general terms: real maps loaded from text where
 * names matter (the dump and reports speak them), hand maps where
 * they do not, and the round trip compared byte for byte, which is
 * the strongest equality there is.
 */
#include "040-mapfile.h"
#include "049-observe.h"
#include "026-registry.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char work_dir[256];

/* {{{ check() / write_text() / slurp() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "inside test failed: %s\n", what);
        exit(1);
    }
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) exit(1);
    fputs(text, f);
    fclose(f);
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "r");
    check(f != NULL, "slurp target exists");
    static char buffer[2][16384];
    static int which;
    which ^= 1;
    size_t got = fread(buffer[which], 1, sizeof buffer[which] - 1, f);
    buffer[which][got] = 0;
    fclose(f);
    return buffer[which];
}
/* }}} */

/* {{{ test_buffer_report_names_the_right_slot() */
static void test_buffer_report_names_the_right_slot(void)
{
    /* A pairing station starved on one side: the waiting side grows,
     * and the report must say which slot, by station name. */
    char map_path[512], report_path[512], map_text[2048];
    snprintf(map_path, sizeof map_path, "%s/report.map", work_dir);
    snprintf(report_path, sizeof report_path, "%s/report.txt", work_dir);
    snprintf(map_text, sizeof map_text,
        "feeder seven p\n"
        "  out 0 - pairer.0\n"
        "pairer add p\n"
        "  out 0 - drain.0\n"
        "drain swallow p\n");
    write_text(map_path, map_text);

    map_t *m = map_load_file(map_path, 2);
    /* Flood the pairer's first slot from outside; its second side
     * never arrives, so the buffer must grow. */
    pool_submitter_register(m->pool);
    pool_release(m->pool);
    for (int i = 0; i < 200; i++)
        map_deliver_value(m, 1, 0, &i);

    FILE *out = fopen(report_path, "w");
    map_report_buffers(m, out);
    map_report_stations(m, out, REPORT_BY_COUNT);
    fclose(out);

    char *report = slurp(report_path);
    check(strstr(report, "pairer.0") != NULL,
          "the report names the starved slot by station name");
    check(strstr(report, "high water") != NULL, "high water is reported");

    /* Unstick the pairer so the map can finish. */
    for (int i = 0; i < 201; i++)
        map_deliver_value(m, 1, 1, &i);
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  the buffer report pointed at the right slot by name\n");
}
/* }}} */

/* {{{ test_station_counts() */
static void test_station_counts(void)
{
    char map_path[512], report_path[512], out_path[512], map_text[2048];
    snprintf(map_path, sizeof map_path, "%s/counts.map", work_dir);
    snprintf(report_path, sizeof report_path, "%s/counts.txt", work_dir);
    snprintf(out_path, sizeof out_path, "%s/counts-out.txt", work_dir);
    snprintf(map_text, sizeof map_text,
        "statics\n"
        "  0 = \"%s\"\n"
        "head seven p\n"
        "  out 0 - mid.0\n"
        "mid double_it p\n"
        "  out 0 - sink.1\n"
        "sink write_int_file p\n"
        "  in 0 $0\n", out_path);
    write_text(map_path, map_text);

    map_t *m = map_load_file(map_path, 2);
    pool_release(m->pool);
    pool_join(m->pool);

    check(m->stations[0].runs == 1 && m->stations[1].runs == 1
          && m->stations[2].runs == 1, "each station ran exactly once");
    check(m->stations[0].produced == 1 && m->stations[1].produced == 1,
          "each producer made one task due downstream");

    FILE *out = fopen(report_path, "w");
    map_report_stations(m, out, REPORT_BY_COUNT);
    fclose(out);
    char *report = slurp(report_path);
    check(strstr(report, "head") && strstr(report, "runs 1"),
          "the report speaks names and counts");
    map_destroy(m);
    printf("  run and production counts are exact and named\n");
}
/* }}} */

/* {{{ test_round_trip() */
static void test_round_trip(void)
{
    char map_path[512], dump1_path[512], dump2_path[512], out_path[512];
    char map_text[1024];
    snprintf(map_path, sizeof map_path, "%s/trip.map", work_dir);
    snprintf(dump1_path, sizeof dump1_path, "%s/trip-dump1.map", work_dir);
    snprintf(dump2_path, sizeof dump2_path, "%s/trip-dump2.map", work_dir);
    snprintf(out_path, sizeof out_path, "%s/trip-out.txt", work_dir);

    /* Every feature the format has: statics, a comparator with its
     * threshold, a gatherer, an iterator, fan-out. */
    snprintf(map_text, sizeof map_text,
        "statics\n"
        "  0 = 5\n"
        "  1 = \"%s\"\n"
        "first seven p\n"
        "  out 0 - judge.0\n"
        "fresh seven p\n"
        "judge keep c\n"
        "  in 1 $0\n"
        "  out 2 - boost.0\n"
        "boost add p\n"
        "  in 1 fresh\n"
        "  out 0 - deal.0\n"
        "deal keep i\n"
        "  out 0 - sink.1\n"
        "sink write_int_file p\n"
        "  in 0 $1\n", out_path);
    write_text(map_path, map_text);

    /* Load, dump before running (so capacities are virgin), then
     * load the dump and dump again: byte-identical or bust. */
    map_t *m1 = map_load_file(map_path, 2);
    FILE *d1 = fopen(dump1_path, "w");
    map_dump(m1, d1);
    fclose(d1);
    pool_release(m1->pool);
    pool_join(m1->pool);
    map_destroy(m1);

    map_t *m2 = map_load_file(dump1_path, 2);
    FILE *d2 = fopen(dump2_path, "w");
    map_dump(m2, d2);
    fclose(d2);
    pool_release(m2->pool);
    pool_join(m2->pool);
    map_destroy(m2);

    char *first = slurp(dump1_path);
    char *second = slurp(dump2_path);
    check(strcmp(first, second) == 0,
          "dump -> load -> dump produced identical bytes");
    printf("  the round trip closed: dump of the dump is the dump\n");
}
/* }}} */

/* {{{ test_rewire_mid_run() */
static _Atomic int rewire_gate;

static void test_rewire_mid_run(void)
{
    char map_path[512], a_path[512], b_path[512], map_text[2048];
    snprintf(map_path, sizeof map_path, "%s/rewire.map", work_dir);
    snprintf(a_path, sizeof a_path, "%s/rewire-a.txt", work_dir);
    snprintf(b_path, sizeof b_path, "%s/rewire-b.txt", work_dir);
    /* A seeded seven flows through hold into writer A; then the wire
     * moves to writer B and an outside value follows it. The map
     * needs its seedable head because a purely outside-driven map
     * cannot pass the nothing-to-seed check — a genuine collision
     * between issues 605 and 604's own warning, recorded in the
     * first-pass report. */
    snprintf(map_text, sizeof map_text,
        "statics\n"
        "  0 = \"%s\"\n"
        "  1 = \"%s\"\n"
        "head seven p\n"
        "  out 0 - hold.0\n"
        "hold keep p\n"
        "  out 0 - writer_a.1\n"
        "writer_a write_int_file p\n"
        "  in 0 $0\n"
        "writer_b write_int_file p\n"
        "  in 0 $1\n", a_path, b_path);
    write_text(map_path, map_text);

    unlink(a_path);
    unlink(b_path);
    map_t *m = map_load_file(map_path, 2);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* Let the seeded seven travel the old wire to completion. */
    usleep(50000);
    (void)rewire_gate;

    /* The move: disconnect hold->writer_a, connect hold->writer_b —
     * while the map runs. Station indices follow file order: head 0,
     * hold 1, writer_a 2, writer_b 3. */
    check(map_rewire_disconnect(m, 1, 0, 2, 1) == 0, "the old wire came out");
    check(map_rewire_connect(m, 1, 0, 3, 1) == 0, "the new wire went in");

    int w = 222;
    map_deliver_value(m, 1, 0, &w);

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    FILE *fa = fopen(a_path, "r");
    FILE *fb = fopen(b_path, "r");
    int a = -1, b = -1;
    if (fa) { if (fscanf(fa, "%d", &a) != 1) a = -1; fclose(fa); }
    if (fb) { if (fscanf(fb, "%d", &b) != 1) b = -1; fclose(fb); }
    check(a == 7, "the seeded value went down the old wire before the move");
    check(b == 222, "the value after the rewire went down the new one");
    map_destroy(m);
    printf("  a wire moved mid-run; nothing lost, behaviour bent at the seam\n");
}
/* }}} */

/* {{{ test_joint_cycle_refused() */
static map_t *race_map;
static _Atomic int race_refusals;

static void *edge_one(void *arg)
{
    (void)arg;
    if (map_rewire_gather(race_map, 2, 0, 3) != 0)
        race_refusals++;
    return NULL;
}

static void *edge_two(void *arg)
{
    (void)arg;
    if (map_rewire_gather(race_map, 3, 0, 2) != 0)
        race_refusals++;
    return NULL;
}

static void test_joint_cycle_refused(void)
{
    enum { ROUNDS = 50 };
    int ever_refused_exactly_one = 1;
    for (int r = 0; r < ROUNDS; r++) {
        /* Stations 2 and 3 each already gather from a harmless leaf,
         * so each is a legal gather source (no ring inputs). One
         * thread repoints 2 to pull from 3; the other repoints 3 to
         * pull from 2. Each edge alone is legal; together they are a
         * call that never returns. Under the one rewiring lock,
         * exactly one must lose. */
        race_map = map_create(4);
        map_place_box(race_map, 0, "seven", STATION_PLAIN);
        map_place_box(race_map, 1, "seven", STATION_PLAIN);
        map_place_box(race_map, 2, "double_it", STATION_PLAIN);
        map_place_box(race_map, 3, "double_it", STATION_PLAIN);
        map_slot_gather(race_map, 2, 0, 0);
        map_slot_gather(race_map, 3, 0, 1);

        race_refusals = 0;
        pthread_t t1, t2;
        pthread_create(&t1, NULL, edge_one, NULL);
        pthread_create(&t2, NULL, edge_two, NULL);
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);

        if (race_refusals != 1)
            ever_refused_exactly_one = 0;
        map_destroy(race_map);
    }
    check(ever_refused_exactly_one,
          "of two individually-legal, jointly-cyclic edges, exactly one "
          "was refused, every round");
    printf("  %d rounds of racing edges: always exactly one refusal\n",
           ROUNDS);
}
/* }}} */

int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/inside-test-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0)
        exit(1);

    test_buffer_report_names_the_right_slot();
    test_station_counts();
    test_round_trip();
    test_rewire_mid_run();
    test_joint_cycle_refused();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        exit(1);
    return 0;
}
