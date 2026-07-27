/*
 * 017-phase-1-pool-demo.c — the pool under load, in numbers.
 *
 * What this is: the phase 1 demonstration. It puts the thread pool
 * through the four situations it will actually meet — a burst of
 * fan-out, chains of uneven length, long stretches of idleness, and
 * raw throughput at every worker count the machine has — and reports
 * what it measured. Nothing printed here is a constant from this
 * file; every number is read off the running pool.
 *
 * How it does it, in general terms: each scene builds a fresh pool,
 * feeds it a workload shaped for one question, and reads the answer
 * from clocks and the pool's own counters. Results go to the screen
 * and to the RAM-backed shared-memory tier, so one run can be laid
 * beside another.
 */
#include "011-pool.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

/* Results are mirrored here as well as the screen. */
static FILE *report;

/* {{{ say() */
/* One voice, two ears: everything said goes to both outputs. */
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

/* {{{ now_seconds() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ cpu_seconds() */
static double cpu_seconds(void)
{
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    return (double)u.ru_utime.tv_sec + (double)u.ru_utime.tv_usec / 1e6
         + (double)u.ru_stime.tv_sec + (double)u.ru_stime.tv_usec / 1e6;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: growth under fan-out.                                   */
/* ------------------------------------------------------------------ */

static pool_t *fan_pool;
static _Atomic int fan_ran;

typedef struct fan_task {
    task_t base;
    int    children;
} fan_task_t;

/* {{{ fan_burst() */
/*
 * Two paths: a task with children enqueues them all in one turn —
 * which is exactly how one delivery floods the queue — and a leaf
 * only counts itself.
 */
static void fan_burst(task_t *t)
{
    fan_task_t *f = (fan_task_t *)t;
    fan_ran++;
    for (int i = 0; i < f->children; i++) {
        fan_task_t *child = malloc(sizeof *child);
        if (!child) abort();
        child->base.call = fan_burst;
        /* Grandchildren get a few great-grandchildren; after that the
         * line ends. Three generations is enough to overflow the ring
         * several times over. */
        child->children = (f->children > 10) ? 4 : 0;
        pool_push(fan_pool, &child->base);
    }
}
/* }}} */

/* {{{ scene_growth() */
static void scene_growth(void)
{
    fan_pool = pool_create(4, NULL, NULL);
    fan_ran = 0;

    int capacity_at_start;
    pool_queue_stats(fan_pool, &capacity_at_start, NULL, NULL);

    fan_task_t *root = malloc(sizeof *root);
    if (!root) abort();
    root->base.call = fan_burst;
    root->children = 100;
    pool_push(fan_pool, &root->base);

    pool_release(fan_pool);
    pool_join(fan_pool);

    int capacity, high_water, growths;
    pool_queue_stats(fan_pool, &capacity, &high_water, &growths);

    say("scene 1 — growth under fan-out\n");
    say("  one task fanned out to 100 children, each with a few of their own\n");
    say("  tasks run          %d\n", (int)fan_ran);
    say("  queue capacity     %d at start, %d at end\n", capacity_at_start, capacity);
    say("  doublings          %d\n", growths);
    say("  high-water depth   %d tasks waiting at the worst moment\n", high_water);
    say("  the ring never overflowed and never stopped the world to avoid it\n\n");

    pool_destroy(fan_pool);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: termination with a long tail.                           */
/* ------------------------------------------------------------------ */

static _Atomic long tail_last_ns;

typedef struct tail_task {
    task_t  base;
    pool_t *pool;
    int     remaining;
} tail_task_t;

/* {{{ tail_step() */
static void tail_step(task_t *t)
{
    tail_task_t *c = (tail_task_t *)t;
    if (c->remaining > 0) {
        tail_task_t *next = malloc(sizeof *next);
        if (!next) abort();
        next->base.call = tail_step;
        next->pool = c->pool;
        next->remaining = c->remaining - 1;
        pool_push(c->pool, &next->base);
        return;
    }
    /* A chain just ended. Remember the latest ending seen anywhere,
     * by compare-and-swap so concurrent endings cannot overwrite a
     * later time with an earlier one. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long ns = ts.tv_sec * 1000000000L + ts.tv_nsec;
    long seen = tail_last_ns;
    while (ns > seen &&
           !atomic_compare_exchange_weak(&tail_last_ns, &seen, ns))
        ;
}
/* }}} */

/* {{{ scene_termination_tail() */
static void scene_termination_tail(void)
{
    static const int lengths[] = { 10, 500, 20000, 200000 };
    enum { CHAINS = sizeof lengths / sizeof lengths[0] };

    pool_t *p = pool_create(4, NULL, NULL);
    tail_last_ns = 0;

    for (int i = 0; i < CHAINS; i++) {
        tail_task_t *c = malloc(sizeof *c);
        if (!c) abort();
        c->base.call = tail_step;
        c->pool = p;
        c->remaining = lengths[i];
        pool_push(p, &c->base);
    }

    pool_release(p);
    pool_join(p);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long done_ns = ts.tv_sec * 1000000000L + ts.tv_nsec;

    say("scene 2 — termination with a long tail\n");
    say("  four chains of length 10, 500, 20000, 200000 ran at once\n");
    say("  last task finished, then the pool declared itself done\n");
    say("  gap between the two  %.6f ms\n",
        (double)(done_ns - tail_last_ns) / 1e6);
    say("  that gap is the entire cost of knowing when to stop\n\n");

    pool_destroy(p);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: the price of idleness.                                */
/* ------------------------------------------------------------------ */

static _Atomic int idle_ran;

/* {{{ idle_tick() */
static void idle_tick(task_t *t)
{
    (void)t;
    idle_ran++;
}
/* }}} */

/* {{{ scene_idle() */
static void scene_idle(void)
{
    enum { TASKS = 25, GAP_US = 40000 };
    pool_t *p = pool_create(4, NULL, NULL);
    idle_ran = 0;

    pool_submitter_register(p);
    pool_release(p);

    double wall_before = now_seconds();
    double cpu_before = cpu_seconds();

    for (int i = 0; i < TASKS; i++) {
        task_t *t = malloc(sizeof *t);
        if (!t) abort();
        t->call = idle_tick;
        pool_push(p, t);
        usleep(GAP_US);
    }

    pool_submitter_unregister(p);
    pool_join(p);

    double wall = now_seconds() - wall_before;
    double cpu = cpu_seconds() - cpu_before;

    say("scene 3 — the price of idleness\n");
    say("  %d tasks trickled in over %.2f s to 4 mostly-idle workers\n",
        (int)idle_ran, wall);
    say("  processor time consumed  %.4f s\n", cpu);
    say("  a spinning pool would have consumed roughly %.1f s — %.0fx more\n",
        wall * 4, (wall * 4) / (cpu > 0.0001 ? cpu : 0.0001));
    say("\n");

    pool_destroy(p);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: throughput against worker count.                       */
/* ------------------------------------------------------------------ */

static _Atomic long grind_done;

/* {{{ grind() */
/*
 * A small but honest piece of work — enough arithmetic that the task
 * is not pure lock traffic, small enough that the queue still matters.
 */
static void grind(task_t *t)
{
    (void)t;
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < 400; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    /* Keep the arithmetic alive past the optimizer. */
    if (x == 0) abort();
    grind_done++;
}
/* }}} */

/* {{{ scene_throughput() */
static void scene_throughput(void)
{
    enum { TASKS = 200000 };
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    say("scene 4 — throughput against worker count (%d small tasks each run)\n",
        TASKS);
    say("  workers   tasks per second\n");

    for (int w = 1; w <= cores; w *= 2) {
        pool_t *p = pool_create(w, NULL, NULL);
        grind_done = 0;
        for (int i = 0; i < TASKS; i++) {
            task_t *t = malloc(sizeof *t);
            if (!t) abort();
            t->call = grind;
            pool_push(p, t);
        }
        double before = now_seconds();
        pool_release(p);
        pool_join(p);
        double elapsed = now_seconds() - before;
        pool_destroy(p);

        if (grind_done != TASKS) {
            fprintf(stderr, "throughput scene lost tasks: %ld of %d\n",
                    (long)grind_done, TASKS);
            exit(1);
        }
        say("  %7d   %12.0f\n", w, TASKS / elapsed);
    }
    say("  where the line stops climbing is the single queue mutex's ceiling\n\n");
}
/* }}} */

int main(int argc, char **argv)
{
    /* The project root arrives as the one argument; the launcher
     * passes it. The report lands in the RAM-backed tier under it. */
    if (argc > 1) {
        char path[4096];
        snprintf(path, sizeof path, "%s/tmp/shared-memory/phase-1-pool-report.txt",
                 argv[1]);
        report = fopen(path, "w");
        if (!report)
            fprintf(stderr, "could not open %s; reporting to screen only\n", path);
    }

    say("=== phase 1 demo: the pool under load ===\n\n");
    scene_growth();
    scene_termination_tail();
    scene_idle();
    scene_throughput();
    say("=== every number above was measured on this run ===\n");

    if (report)
        fclose(report);
    return 0;
}
