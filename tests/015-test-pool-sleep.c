/*
 * 015-test-pool-sleep.c — proves sleeping beats spinning (issue 103).
 *
 * What this is: the test that idle workers cost nearly nothing. A
 * spinning pool burns one full core per idle worker; a sleeping pool
 * shows processor time far below wall-clock time.
 *
 * How it does it, in general terms: several workers sit mostly idle
 * while tasks trickle in slowly from outside. The trickling thread
 * registers itself as an outside submitter so the pool knows not to
 * declare itself finished between two arrivals — that registration
 * is issue 104's one concession to work arriving after startup.
 * Process CPU time is then compared against elapsed wall time.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

enum { WORKERS = 4, TASKS = 20, GAP_MICROSECONDS = 40000 };

static _Atomic int ran;

/* {{{ tick() */
static void tick(task_t *t)
{
    (void)t;
    ran++;
}
/* }}} */

/* {{{ cpu_seconds() */
/* Total processor time this process has consumed, user plus system. */
static double cpu_seconds(void)
{
    struct rusage u;
    if (getrusage(RUSAGE_SELF, &u) != 0) {
        fprintf(stderr, "getrusage failed\n");
        exit(1);
    }
    return (double)u.ru_utime.tv_sec + (double)u.ru_utime.tv_usec / 1e6
         + (double)u.ru_stime.tv_sec + (double)u.ru_stime.tv_usec / 1e6;
}
/* }}} */

/* {{{ wall_seconds() */
static double wall_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

int main(void)
{
    pool_t *p = pool_create(WORKERS, NULL, NULL);

    /* Register before release: from the pool's view this thread is a
     * standing promise that more work may come, so an all-asleep pool
     * waits instead of terminating. */
    pool_submitter_register(p);
    pool_release(p);

    double wall_before = wall_seconds();
    double cpu_before = cpu_seconds();

    for (int i = 0; i < TASKS; i++) {
        task_t *t = malloc(sizeof *t);
        if (!t) abort();
        t->call = tick;
        pool_push(p, t);
        usleep(GAP_MICROSECONDS);
    }

    pool_submitter_unregister(p);
    pool_join(p);

    double wall = wall_seconds() - wall_before;
    double cpu = cpu_seconds() - cpu_before;

    if (ran != TASKS) {
        fprintf(stderr, "only %d of %d trickled tasks ran\n", (int)ran, TASKS);
        exit(1);
    }

    /* Four spinning workers would burn roughly 4x wall in CPU. The
     * bound of half of wall is generous — the actual figure should
     * be near zero — while staying immune to a noisy machine. */
    if (cpu > wall * 0.5) {
        fprintf(stderr,
                "idle pool consumed %.3fs CPU over %.3fs wall — that is a spin\n",
                cpu, wall);
        exit(1);
    }

    pool_destroy(p);
    printf("  %d workers idled through %.2fs of trickle for %.3fs CPU\n",
           WORKERS, wall, cpu);
    return 0;
}
