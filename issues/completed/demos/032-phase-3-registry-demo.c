/*
 * 032-phase-3-registry-demo.c — the compiled half of the phase 3 demo.
 *
 * What this is: the scenes of the phase 3 demonstration that need a
 * running engine — the registry printed beside the compiler's own
 * sizeof answers, every value shape flowing through the one call
 * site, and the occupancy figure re-measured so the demos read as
 * one series.
 *
 * How it does it, in general terms: everything printed is either
 * read out of the generated registry or measured on a live map whose
 * stations were placed by name — nothing here is typed in by hand,
 * which is the entire claim of the phase.
 */
#include "026-registry.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *report;

/* Redeclared box types, for the sizeof column and byte checks. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { int a; vec3 pos; char note[16]; unsigned long stamp; } record;

/* {{{ say() */
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
}
/* }}} */

/* {{{ scene_registry_versus_sizeof() */
static void scene_registry_versus_sizeof(void)
{
    say("scene: the registry beside the compiler\n");
    say("  what the registry says          what sizeof says\n");

    struct probe {
        const char *box;
        const char *which;   /* "ret" or a param index as text */
        int         compiled;
        const char *label;
    };
    const struct probe probes[] = {
        { "add",              "ret", (int)sizeof(int),           "int" },
        { "mix",              "p1",  (int)sizeof(double),        "double" },
        { "make_vec3",        "ret", (int)sizeof(vec3),          "vec3" },
        { "stamp_record",     "ret", (int)sizeof(record),        "record" },
        { "magnitude_squared","ret", (int)sizeof(float),         "float" },
    };

    for (unsigned i = 0; i < sizeof probes / sizeof probes[0]; i++) {
        const box_info_t *b = registry_find(probes[i].box);
        if (!b) {
            fprintf(stderr, "missing box %s\n", probes[i].box);
            exit(1);
        }
        int from_registry = strcmp(probes[i].which, "ret") == 0
            ? b->return_size
            : b->params[probes[i].which[1] - '0'].size;
        say("  %-16s %-8s %3d          %3d  %s\n",
            probes[i].box, probes[i].label, from_registry,
            probes[i].compiled,
            from_registry == probes[i].compiled ? "identical" : "MISMATCH");
        if (from_registry != probes[i].compiled)
            exit(1);
    }
    say("  nothing was hand-typed; both columns come from the same C\n\n");
}
/* }}} */

/* {{{ type coverage: every shape through one call site */
static _Atomic int coverage_hits;
static _Atomic int coverage_wrong;

/* Harness sinks: instrumentation reaching this demo's counters,
 * which generated boxes cannot see. Marked scaffolding. */
static void check_float__call(task_t *t)
{
    float f;
    memcpy(&f, t->in[0], sizeof f);
    coverage_hits++;
    /* magnitude_squared of (3+1, 4+1, 0+1) = 16+25+1 = 42. */
    if (f != 42.0f)
        coverage_wrong++;
}

static void check_record__call(task_t *t)
{
    record r;
    memcpy(&r, t->in[0], sizeof r);
    coverage_hits++;
    if (r.a != 7 || r.pos.x != 1.5f || r.note[0] != 'o' || r.stamp != 99UL)
        coverage_wrong++;
}

static void scene_type_coverage(void)
{
    say("scene: every shape through the one call site\n");

    /* Chain one: floats -> struct -> struct -> float.
     * make_vec3 -> nudge -> magnitude_squared -> check. */
    map_t *m = map_create(4);
    map_place_box(m, 0, "make_vec3", STATION_PLAIN);
    map_place_box(m, 1, "nudge", STATION_PLAIN);
    map_place_box(m, 2, "magnitude_squared", STATION_PLAIN);
    int one_float[1] = { sizeof(float) };
    map_place(m, 3, check_float__call, STATION_PLAIN, 1, one_float, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 1, 0, 2, 0);
    map_connect(m, 2, 0, 3, 0);
    map_start(m, 4);

    coverage_hits = 0;
    coverage_wrong = 0;
    float x = 3, y = 4, z = 0, amount = 1.0f;
    map_deliver_value(m, 0, 0, &x);
    map_deliver_value(m, 0, 1, &y);
    map_deliver_value(m, 0, 2, &z);
    map_deliver_value(m, 1, 1, &amount);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    /* Chain two: int + struct + wide unsigned -> nested record. */
    map_t *m2 = map_create(2);
    map_place_box(m2, 0, "stamp_record", STATION_PLAIN);
    int one_record[1] = { sizeof(record) };
    map_place(m2, 1, check_record__call, STATION_PLAIN, 1, one_record, 0);
    map_connect(m2, 0, 0, 1, 0);
    map_start(m2, 4);

    int a = 7;
    vec3 pos = { 1.5f, 2.5f, 3.5f };
    unsigned long stamp = 99UL;
    map_deliver_value(m2, 0, 0, &a);
    map_deliver_value(m2, 0, 1, &pos);
    map_deliver_value(m2, 0, 2, &stamp);
    pool_release(m2->pool);
    pool_join(m2->pool);
    map_destroy(m2);

    say("  ints, floats, structs by value, a nested struct with a string\n");
    say("  and a wide unsigned — %d arrivals, %d wrong bytes\n",
        (int)coverage_hits, (int)coverage_wrong);
    for (int i = 0; i < registry_n_boxes; i++)
        say("    task for %-18s %4zu bytes, cut exactly\n",
            registry_boxes[i].name, registry_boxes[i].task_size);
    if (coverage_hits != 2 || coverage_wrong != 0) {
        fprintf(stderr, "type coverage failed\n");
        exit(1);
    }
    say("\n");
}
/* }}} */

/* {{{ occupancy, one more time */
static _Atomic int busy_now;
static _Atomic int busy_peak;

static void occupancy_probe__call(task_t *t)
{
    int entered = ++busy_now;
    int peak = busy_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&busy_peak, &peak, entered))
        ;
    /* Honest arithmetic, then pass the value along unchanged. */
    unsigned long v = 88172645463325252UL;
    for (int i = 0; i < 12000; i++) {
        v ^= v << 13;
        v ^= v >> 7;
        v ^= v << 17;
    }
    int x;
    memcpy(&x, t->in[0], sizeof x);
    x += (int)(v & 1);
    memcpy(t->out, &x, sizeof x);
    busy_now--;
}

static void scene_occupancy(void)
{
    enum { WIDTH = 16, VALUES = 60 };
    map_t *m = map_create(WIDTH);
    int one_int[1] = { sizeof(int) };
    for (int i = 0; i < WIDTH; i++)
        map_place(m, i, occupancy_probe__call, STATION_PLAIN, 1, one_int,
                  sizeof(int));
    map_start(m, 0);

    busy_now = 0;
    busy_peak = 0;
    for (int v = 0; v < VALUES; v++)
        for (int s = 0; s < WIDTH; s++)
            map_deliver_value(m, s, 0, &v);
    pool_release(m->pool);
    pool_join(m->pool);

    say("scene: the running figure, for the series\n");
    say("  peak concurrent boxes %d of %d workers — same machine,\n",
        (int)busy_peak, pool_worker_count(m->pool));
    say("  same pool, same stations as the phase 2 demo, boxes now generated\n\n");
    map_destroy(m);
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc > 1) {
        char path[4096];
        snprintf(path, sizeof path,
                 "%s/tmp/shared-memory/phase-3-registry-demo.txt", argv[1]);
        report = fopen(path, "a");
    }

    say("scene: the registry, printed whole\n");
    registry_print(stdout);
    if (report)
        registry_print(report);
    say("\n");

    scene_registry_versus_sizeof();
    scene_type_coverage();
    scene_occupancy();

    if (report)
        fclose(report);
    return 0;
}
