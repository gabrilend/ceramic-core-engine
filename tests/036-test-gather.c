/*
 * 036-test-gather.c — proves the pull path (issues 403, 404).
 *
 * What this is: the tests that a gathered value is fresh at the
 * moment it is used, that gathering runs once per task from any
 * number of threads, that chains walk to their recorded depth, that
 * a gather cycle is refused at wiring time, and that a push cycle —
 * the engine's only way to carry state — still counts correctly.
 *
 * How it does it, in general terms: a read box pulls an integer
 * from a file the test rewrites between deliveries; the values that
 * arrive downstream tell exactly when each pull happened. Cycles are
 * proven by forking a child that tries to wire one and demanding it
 * dies. The push-cycle counter pairs its loop with a finite ticket
 * supply, so the loop terminates when the tickets run out — which is
 * itself a finding about counters in this engine.
 */
#include "018-station.h"
#include "026-registry.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *file_path;

/* {{{ check() / expect_death() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "gather test failed: %s\n", what);
        exit(1);
    }
}

static void expect_death(void (*scenario)(void), const char *what)
{
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        scenario();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFSIGNALED(status)) {
        fprintf(stderr, "gather test failed: %s — the child survived\n", what);
        exit(1);
    }
}
/* }}} */

/* {{{ write_file() */
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

/* {{{ test_freshness() */
/*
 * add(x, file) where the file side is gathered through read_int_file
 * with its path from a static. The file changes between deliveries;
 * each arriving sum tells which world its pull saw.
 */
static _Atomic long fresh_sum;
static _Atomic int fresh_runs;

static void fresh_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    fresh_sum += x;
    fresh_runs++;
}

static void test_freshness(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "add", STATION_PLAIN);           /* x + gathered */
    map_place_box(m, 1, "read_int_file", STATION_PLAIN); /* the gatherable */
    int one_int[1] = { sizeof(int) };
    map_place(m, 2, fresh_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 2, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, file_path);
    map_slot_static(m, 1, 0, 0);   /* the reader's path is static */
    map_slot_gather(m, 0, 1, 1);   /* add's second input is pulled */

    map_start(m, 2);
    fresh_sum = 0;
    fresh_runs = 0;

    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* World one: the file says 100. Two deliveries pull it twice. */
    write_file(file_path, 100);
    int zero = 0;
    map_deliver_value(m, 0, 0, &zero);
    map_deliver_value(m, 0, 0, &zero);
    while (fresh_runs < 2)
        usleep(1000);

    /* World two: the file changes; the next pull follows it. */
    write_file(file_path, 5000);
    map_deliver_value(m, 0, 0, &zero);
    while (fresh_runs < 3)
        usleep(1000);

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    check(fresh_sum == 100 + 100 + 5000,
          "each task pulled the file as it was at assembly, not at startup");
    map_destroy(m);
    printf("  a gathered file read followed the file; a pushed one could not have\n");
}
/* }}} */

/* {{{ test_once_per_task_many_threads() */
/*
 * Saturate a station whose second input is gathered; every result
 * must be exactly right, from many workers gathering concurrently.
 */
static _Atomic long crowd_sum;
static _Atomic int crowd_runs;

static void crowd_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    crowd_sum += x;
    crowd_runs++;
}

static void test_once_per_task_many_threads(void)
{
    enum { VALUES = 2000 };
    map_t *m = map_create(3);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "read_int_file", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 2, crowd_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 2, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, file_path);
    map_slot_static(m, 1, 0, 0);
    map_slot_gather(m, 0, 1, 1);

    write_file(file_path, 7);
    map_start(m, 0);
    crowd_sum = 0;
    crowd_runs = 0;
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    long expected = 0;
    for (int i = 0; i < VALUES; i++)
        expected += i + 7;
    check(crowd_runs == VALUES, "one gather per task, exactly");
    check(crowd_sum == expected, "every concurrent gather read correctly");
    map_destroy(m);
    printf("  %d tasks gathered concurrently, every value exact\n", VALUES);
}
/* }}} */

/* {{{ test_chain_depth() */
/*
 * seven -> double_it -> double_it -> add: a chain three gatherers
 * deep, walked inline per task; the recorded depth must say 3.
 */
static _Atomic long chain_sum;
static _Atomic int chain_runs;

static void chain_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    chain_sum += x;
    chain_runs++;
}

static void test_chain_depth(void)
{
    map_t *m = map_create(5);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "double_it", STATION_PLAIN);
    map_place_box(m, 2, "double_it", STATION_PLAIN);
    map_place_box(m, 3, "seven", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 4, chain_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 4, 0);

    /* The chain, wired leaf-first: 2 pulls 3, 1 pulls 2, 0 pulls 1. */
    map_slot_gather(m, 2, 0, 3);
    map_slot_gather(m, 1, 0, 2);
    map_slot_gather(m, 0, 1, 1);

    int recorded_depth = m->gather_depth;
    check(recorded_depth == 3, "the recorded depth is the chain's length");

    map_start(m, 2);
    chain_sum = 0;
    chain_runs = 0;
    for (int i = 0; i < 10; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    /* seven doubled twice is 28; each value gains it. */
    long expected = 0;
    for (int i = 0; i < 10; i++)
        expected += i + 28;
    check(chain_sum == expected, "the chain's value arrived through 3 pulls");
    check(chain_runs == 10, "one walk per task");
    map_destroy(m);
    printf("  a chain three deep walked inline, depth recorded as %d\n",
           recorded_depth);
}
/* }}} */

/* {{{ death: cycles refused at wiring time */
static void die_two_cycle(void)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    map_place_box(m, 1, "double_it", STATION_PLAIN);
    map_slot_gather(m, 0, 0, 1);
    map_slot_gather(m, 1, 0, 0); /* closes the loop — must die */
}

static void die_three_cycle(void)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "double_it", STATION_PLAIN);
    map_place_box(m, 1, "double_it", STATION_PLAIN);
    map_place_box(m, 2, "double_it", STATION_PLAIN);
    map_slot_gather(m, 0, 0, 1);
    map_slot_gather(m, 1, 0, 2);
    map_slot_gather(m, 2, 0, 0); /* the long way around — must die */
}
/* }}} */

/* {{{ test_push_cycle_counts() */
/*
 * The counter the docs promise: state carried on a wire. The loop
 * pairs the running total with a ticket; each pass consumes one
 * ticket, so a finite ticket supply is both the driver and the
 * stopping condition. add(total, ticket) feeds itself and a sink.
 */
static _Atomic int loop_last;
static _Atomic int loop_passes;

static void loop_watch__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    /* Passes arrive in order along the single chain, so the largest
     * is the last. */
    int seen = loop_last;
    while (x > seen &&
           !atomic_compare_exchange_weak(&loop_last, &seen, x))
        ;
    loop_passes++;
}

static void test_push_cycle_counts(void)
{
    enum { TICKETS = 25 };
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN); /* (total, ticket) */
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, loop_watch__call, STATION_PLAIN, 1, one_int, 0);
    /* The push cycle the design celebrates — legal, and checked by
     * nothing, because it passes through a buffer and the call ends. */
    map_connect(m, 0, 0, 0, 0);
    map_connect(m, 0, 0, 1, 0);

    map_start(m, 2);
    loop_last = 0;
    loop_passes = 0;

    int zero = 0;
    map_deliver_value(m, 0, 0, &zero);       /* the running total */
    int one = 1;
    for (int i = 0; i < TICKETS; i++)
        map_deliver_value(m, 0, 1, &one);    /* the ticket supply */
    pool_release(m->pool);
    pool_join(m->pool);

    check(loop_passes == TICKETS, "one pass per ticket");
    check(loop_last == TICKETS, "the total came around the loop intact");
    map_destroy(m);
    printf("  a push cycle counted to %d and stopped when tickets ran out\n",
           TICKETS);
}
/* }}} */

int main(void)
{
    static char path_buffer[256];
    snprintf(path_buffer, sizeof path_buffer,
             "/dev/shm/minimal-soramech/gather-test-%d.txt", (int)getpid());
    file_path = path_buffer;

    test_freshness();
    test_once_per_task_many_threads();
    test_chain_depth();

    expect_death(die_two_cycle, "a two-station gather cycle was accepted");
    expect_death(die_three_cycle, "a three-station gather cycle was accepted");
    printf("  gather cycles of both lengths refused at wiring time\n");

    test_push_cycle_counts();

    unlink(file_path);
    return 0;
}
