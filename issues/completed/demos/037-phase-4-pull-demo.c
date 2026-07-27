/*
 * 037-phase-4-pull-demo.c — values that are current, not merely correct.
 *
 * What this is: the phase 4 demonstration, about the direction of
 * flow. A pushed value was true once; a gathered value is true now.
 * Each scene measures one consequence: the same file read frozen and
 * fresh side by side, the tax gathering levies on delivery, the cost
 * of a chain, the cycle that is refused beside the silent death it
 * prevents, and a knob turned while the machine runs.
 *
 * How it does it, in general terms: maps place registry boxes, bind
 * statics and gatherers, and run while the main thread — registered
 * as an outside submitter — rewrites files and table entries in
 * mid-flight. The refused-cycle scene forks children so the reader
 * can see both fates: the check's message, and the messageless
 * signal the unchecked world dies of.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static FILE *report;
static char watched_file[4096];

/* {{{ say() / now_seconds() / write_file() */
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

static void write_file(const char *path, int value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fprintf(f, "%d\n", value);
    fclose(f);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: the same file, frozen and fresh.                        */
/* ------------------------------------------------------------------ */

static _Atomic int frozen_last;
static _Atomic int fresh_last;
static _Atomic int both_arrived;

/* Harness sinks recording what each side reports. */
static void frozen_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    frozen_last = x;
    both_arrived++;
}

static void fresh_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    fresh_last = x;
    both_arrived++;
}

/* {{{ scene_frozen_versus_fresh() */
static void scene_frozen_versus_fresh(void)
{
    write_file(watched_file, 111);

    /* Two identical sub-graphs: add(0, file-value) -> sink. The
     * frozen side's file value was read at load and bound as a
     * static — true once. The fresh side gathers read_int_file at
     * every task — true now. */
    map_t *m = map_create(5);
    map_place_box(m, 0, "add", STATION_PLAIN);           /* frozen adder */
    map_place_box(m, 1, "add", STATION_PLAIN);           /* fresh adder */
    map_place_box(m, 2, "read_int_file", STATION_PLAIN); /* the gatherable */
    int one_int[1] = { sizeof(int) };
    map_place(m, 3, frozen_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 4, fresh_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 3, 0);
    map_connect(m, 1, 0, 4, 0);

    map_statics_alloc(m, 2);
    /* The frozen side: the file as it stands at load, read here and
     * sealed into the table — the push-era way. */
    char loaded[32];
    snprintf(loaded, sizeof loaded, "%d", 111);
    map_static_set_text(m, 0, loaded);
    map_slot_static(m, 0, 1, 0);
    /* The fresh side: the path is static; the value is pulled. */
    map_static_set_text(m, 1, watched_file);
    map_slot_static(m, 2, 0, 1);
    map_slot_gather(m, 1, 1, 2);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    say("scene 1 — the same file, frozen and fresh\n");
    say("  the file says 111; both sides fed a zero:\n");
    int zero = 0;
    both_arrived = 0;
    map_deliver_value(m, 0, 0, &zero);
    map_deliver_value(m, 1, 0, &zero);
    while (both_arrived < 2)
        usleep(1000);
    say("    frozen side reports %d, fresh side reports %d\n",
        (int)frozen_last, (int)fresh_last);

    say("  the file changes to 999 while both run:\n");
    write_file(watched_file, 999);
    both_arrived = 0;
    map_deliver_value(m, 0, 0, &zero);
    map_deliver_value(m, 1, 0, &zero);
    while (both_arrived < 2)
        usleep(1000);
    say("    frozen side reports %d, fresh side reports %d\n",
        (int)frozen_last, (int)fresh_last);
    say("  a pushed value was true once; a gathered value is true now\n\n");

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: the gather tax, measured.                               */
/* ------------------------------------------------------------------ */

static _Atomic int tax_runs;

static void tax_sink__call(task_t *t)
{
    (void)t;
    tax_runs++;
}

/* {{{ run_tax_map() */
/* The same shape twice: add(x, seven) -> sink, with the seven either
 * a static (no pull) or gathered from the deliberately slow source.
 * The wall-clock difference per task is the tax. */
static double run_tax_map(int gathered, int values)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "slow_seven", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 2, tax_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 2, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "7");
    if (gathered)
        map_slot_gather(m, 0, 1, 1);
    else
        map_slot_static(m, 0, 1, 0);

    map_start(m, 4);
    tax_runs = 0;

    /* The timer wraps the seeding too: gathering happens at task
     * assembly, which for seeded values is right here on this
     * thread — a subtlety this demo itself surfaced. The tax is paid
     * wherever the task is built, not where it runs. */
    double before = now_seconds();
    for (int i = 0; i < values; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;

    if (tax_runs != values) {
        fprintf(stderr, "tax map ran %d of %d\n", (int)tax_runs, values);
        exit(1);
    }
    map_destroy(m);
    return elapsed;
}
/* }}} */

/* {{{ scene_gather_tax() */
static void scene_gather_tax(void)
{
    enum { VALUES = 3000 };
    double flat = run_tax_map(0, VALUES);
    double taxed = run_tax_map(1, VALUES);

    say("scene 2 — the gather tax\n");
    say("  %d tasks, second input static:    %7.2f ms (%5.2f us/task)\n",
        VALUES, flat * 1e3, flat * 1e6 / VALUES);
    say("  %d tasks, second input gathered:  %7.2f ms (%5.2f us/task)\n",
        VALUES, taxed * 1e3, taxed * 1e6 / VALUES);
    say("  the difference is paid on the delivery path, once per task —\n");
    say("  exactly what \"fresh at the moment used\" costs; this is why a\n");
    say("  network call does not belong behind a gatherer\n\n");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: chain depth, recorded and measured.                   */
/* ------------------------------------------------------------------ */

/* {{{ scene_chain_depth() */
static void scene_chain_depth(void)
{
    enum { VALUES = 3000 };
    map_t *m = map_create(5);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "double_it", STATION_PLAIN);
    map_place_box(m, 2, "double_it", STATION_PLAIN);
    map_place_box(m, 3, "seven", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 4, tax_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 4, 0);
    map_slot_gather(m, 2, 0, 3);
    map_slot_gather(m, 1, 0, 2);
    map_slot_gather(m, 0, 1, 1);
    int depth = m->gather_depth;

    map_start(m, 4);
    tax_runs = 0;
    /* As in scene two, the walk happens at assembly — the timer
     * must cover the seeding that assembles. */
    double before = now_seconds();
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;
    map_destroy(m);

    say("scene 3 — a chain, three pulls deep\n");
    say("  depth recorded at wiring time  %d\n", depth);
    say("  measured                       %5.2f us per task for the whole walk\n",
        elapsed * 1e6 / VALUES);
    say("  depth is a static property of the map: the cost is bounded\n");
    say("  and knowable before anything runs\n\n");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the refusal, and the silence it prevents.              */
/* ------------------------------------------------------------------ */

/* {{{ endless_recursion() */
/*
 * The fate of an unchecked gather cycle, reproduced: each call is a
 * frame, nothing ever returns. The recursion hides behind a volatile
 * the compiler cannot fold, because the compiler — quite reasonably —
 * refuses to build a provably endless call.
 */
static void endless_recursion(volatile int *depth)
{
    volatile int frame[64];
    frame[0] = ++*depth;
    if (*depth >= 0)
        endless_recursion(depth);
    /* Touching the frame after the call keeps the recursion out of
     * tail position — otherwise the optimizer turns the doom into a
     * polite loop that never overflows anything. */
    *depth += frame[0];
}
/* }}} */

/* {{{ scene_refused_cycle() */
static void scene_refused_cycle(void)
{
    say("scene 4 — the cycle that is refused, and what it saves us from\n");

    /* First child: tries to wire two gatherers into each other. The
     * engine refuses with names; the child dies saying why. */
    int pipefd[2];
    if (pipe(pipefd) != 0) exit(1);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        map_t *m = map_create(2);
        map_place_box(m, 0, "double_it", STATION_PLAIN);
        map_place_box(m, 1, "double_it", STATION_PLAIN);
        map_slot_gather(m, 0, 0, 1);
        map_slot_gather(m, 1, 0, 0);
        _exit(0);
    }
    close(pipefd[1]);
    char message[512] = {0};
    size_t filled = 0;
    for (;;) {
        ssize_t got = read(pipefd[0], message + filled,
                           sizeof message - 1 - filled);
        if (got <= 0)
            break;
        filled += (size_t)got;
        if (filled >= sizeof message - 1)
            break;
    }
    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);
    say("  the engine's answer, verbatim:\n");
    say("    %s", filled > 0 ? message : "(nothing)\n");

    /* Second child: what the same map would do without the check —
     * unbounded recursion on a modest stack. It dies of a signal,
     * with no message, no line, no names. That silence is what the
     * walk was bought to prevent. */
    pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        struct rlimit small = { 1 << 20, 1 << 20 };
        setrlimit(RLIMIT_STACK, &small);
        volatile int depth = 0;
        endless_recursion(&depth);
        _exit(0);
    }
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status))
        say("  without the check: killed by signal %d, saying nothing at all\n",
            WTERMSIG(status));
    say("  one of these failures can be fixed in a minute; the walk is\n");
    say("  what buys the difference\n\n");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene five: a knob turned while it runs.                           */
/* ------------------------------------------------------------------ */

static _Atomic long knob_sum;
static _Atomic int knob_seen;

static void knob_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    knob_sum += x;
    knob_seen++;
}

/* {{{ scene_knob_turned() */
static void scene_knob_turned(void)
{
    enum { PER_PHASE = 200 };
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, knob_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "1000");
    map_slot_static(m, 0, 1, 0);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    say("scene 5 — a knob turned while it runs\n");
    int zero = 0;
    knob_sum = 0;
    knob_seen = 0;
    for (int i = 0; i < PER_PHASE; i++)
        map_deliver_value(m, 0, 0, &zero);
    while (knob_seen < PER_PHASE)
        usleep(1000);
    say("  first %d values through, entry at 1000: mean output %ld\n",
        PER_PHASE, (long)knob_sum / knob_seen);

    /* The turn — through the same call a box could make. */
    int new_value = 5000;
    map_static_write(m, 0, &new_value, sizeof new_value);

    long before_sum = knob_sum;
    int before_seen = knob_seen;
    for (int i = 0; i < PER_PHASE; i++)
        map_deliver_value(m, 0, 0, &zero);
    while (knob_seen < before_seen + PER_PHASE)
        usleep(1000);
    say("  entry written to 5000 mid-run: mean output %ld from that moment\n",
        (long)(knob_sum - before_sum) / PER_PHASE);
    say("  the bend in the numbers is the write becoming every next task\n\n");

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);
}
/* }}} */

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : ".";
    char path[4096];
    snprintf(path, sizeof path, "%s/tmp/shared-memory/phase-4-pull-report.txt", root);
    report = fopen(path, "w");
    snprintf(watched_file, sizeof watched_file,
             "%s/tmp/shared-memory/phase-4-watched.txt", root);

    say("=== phase 4 demo: values that are current, not merely correct ===\n\n");
    scene_frozen_versus_fresh();
    scene_gather_tax();
    scene_chain_depth();
    scene_refused_cycle();
    scene_knob_turned();
    say("=== pushed: true once. gathered: true now. both on purpose ===\n");

    unlink(watched_file);
    if (report)
        fclose(report);
    return 0;
}
