/*
 * 063-test-fan-in-cost.c — what one delivery costs when a station has
 * many input ports and the values are large.
 *
 * What this is: the baseline issue 210c asks for before it changes
 * anything. That issue moves the value-copying out of the station's
 * mutex in two steps and wants to know whether each step helped; a
 * number taken afterwards would be a claim rather than a finding, and
 * phase 4 already learned once that a cost measured in the wrong
 * place measures nothing. So this file exists to be run before, run
 * again after each step, and compared against itself.
 *
 * How it does it, in general terms: one station that is a sink — so
 * nothing downstream can pollute the reading — with several ring
 * input ports, and one thread per port hammering values into it. The
 * threads all start together, so they contend for the one mutex the
 * delivery path takes. The wall clock across all of it, divided by
 * the number of deliveries, is the number.
 *
 * Two runs, and the pairing is the point. The **large** run carries a
 * two-hundred-byte struct, which is the case 210c is trying to
 * improve: a station three arrows fan into holds its lock for six
 * hundred bytes of copying while everyone else waits. The **small**
 * run carries a four-byte integer and is the control — it is expected
 * to barely move when 210c lands, because a copy that small was never
 * what anybody was waiting for. If a future run shows both numbers
 * improving equally, the improvement is not the one that was designed
 * and something else changed.
 *
 * Why the deliveries happen with the workers still parked: the pool's
 * termination rule assumes nothing outside pushes tasks after startup
 * (issue 104), so producers racing a live pool could have it decide it
 * was finished mid-measurement. Parked workers change nothing about
 * what is being measured — the contended mutex is the station's, not
 * the pool's, and every deliverer still takes it, writes, walks the
 * readiness check, and claims. What it removes is a source of noise
 * that has nothing to do with the question.
 *
 * It also asserts, because a measurement of a path that loses values
 * is a measurement of the wrong path: every value delivered is
 * accounted for at the sink.
 */
#include "018-station.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Wide enough that deliverers genuinely queue behind each other, and
 * deep enough that the timer is measuring the path rather than its own
 * resolution. Both are cheap: the whole file runs in well under a
 * second, which is what keeps it in the ordinary test run instead of
 * in a corner nobody visits. */
enum { PORTS = 4, VALUES_PER_PORT = 20000, WORKERS = 4 };

/* Two hundred bytes: large enough that the copy dominates the index
 * arithmetic around it, and close to the size the issue text uses when
 * it describes the problem. */
typedef struct bulk {
    long          serial;
    unsigned char padding[192];
} bulk_t;

/* What the sink accumulates, so nothing can go missing unnoticed. The
 * sum survives reordering, which matters because issue 210d is about
 * to stop promising that values leave a port in the order they
 * arrived — a test that depended on order would start failing for a
 * reason that has nothing to do with cost. */
static _Atomic long sink_total;
static _Atomic long sink_runs;

/* {{{ bulk_sink__call() */
/*
 * Hand-written in the shape the generator emits. It reads every input
 * and adds up their serials — deliberately touching all of them, so
 * the reading includes a consumer that actually looks at what it was
 * handed rather than one the compiler could reason away.
 */
static void bulk_sink__call(task_t *t)
{
    long sum = 0;
    for (int i = 0; i < t->n_in; i++) {
        bulk_t b;
        memcpy(&b, t->in[i], sizeof b);
        sum += b.serial;
    }
    sink_total += sum;
    sink_runs++;
}
/* }}} */

/* {{{ small_sink__call() */
static void small_sink__call(task_t *t)
{
    long sum = 0;
    for (int i = 0; i < t->n_in; i++)
        sum += *(int *)t->in[i];
    sink_total += sum;
    sink_runs++;
}
/* }}} */

/* {{{ struct producer / producer_thread() */
/*
 * One thread per port. They are held at a starting gate and let go
 * together, because threads that trickle in do not contend, and the
 * contention is the whole measurement.
 */
typedef struct producer {
    map_t *map;
    int    port;
    int    elem_size;
    /* The gate: every producer spins until this turns one. A spin
     * rather than a condition variable because the wait is over in
     * microseconds and the point is to release them as close to
     * simultaneously as the hardware allows. */
    _Atomic int *go;
} producer_t;

static void *producer_thread(void *arg)
{
    producer_t *p = arg;
    while (!*p->go)
        ;

    for (int i = 0; i < VALUES_PER_PORT; i++) {
        if (p->elem_size == (int)sizeof(bulk_t)) {
            bulk_t b;
            memset(&b, 0, sizeof b);
            b.serial = 1;
            map_deliver_value(p->map, 0, p->port, &b);
        } else {
            int v = 1;
            map_deliver_value(p->map, 0, p->port, &v);
        }
    }
    return NULL;
}
/* }}} */

/* {{{ now_ns() */
static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
/* }}} */

/* {{{ measure() */
/*
 * One configuration, start to finish. Returns nanoseconds per
 * delivery — the number worth writing down, because it is independent
 * of how many values the run happened to use and so stays comparable
 * if those constants are ever tuned.
 */
static double measure(const char *label, task_call_t shim, int elem_size)
{
    map_t *m = map_create(1);
    int sizes[PORTS];
    for (int i = 0; i < PORTS; i++)
        sizes[i] = elem_size;
    /* A sink: zero output size, so delivery stops here and the
     * measurement is of the inbound path alone. */
    map_place(m, 0, shim, STATION_PLAIN, PORTS, sizes, 0);

    /* Deep enough that no port can ever grow during the run, and this
     * matters more than it looks. The first reading taken here had the
     * default ten-slot buffers, and the ports grew thirteen times
     * apiece — each growth allocating a new array and copying every
     * value across, under the very mutex being measured, with the copy
     * proportional to the element size. So the large-value number was
     * partly a measurement of *growth* rather than of the per-delivery
     * copy, and growth is issue 210e's problem, not issue 210c's.
     *
     * A baseline that includes it would understate 210c: moving the
     * delivery copy out of the lock leaves the growth copy inside it,
     * so the improvement would look smaller than it was for a reason
     * that has nothing to do with the change. Sizing the ports past
     * what the run can fill takes growth out of the picture entirely
     * and leaves exactly the cost 210c is aiming at.
     *
     * One spare slot means a buffer holds one less than its depth, and
     * a producer can be a full run ahead of its slowest sibling. */
    for (int i = 0; i < PORTS; i++)
        map_in_port_start_depth(m, 0, i, VALUES_PER_PORT + 2);

    map_start(m, WORKERS);

    sink_total = 0;
    sink_runs = 0;

    _Atomic int go = 0;
    pthread_t threads[PORTS];
    producer_t producers[PORTS];
    for (int i = 0; i < PORTS; i++) {
        producers[i].map = m;
        producers[i].port = i;
        producers[i].elem_size = elem_size;
        producers[i].go = &go;
        pthread_create(&threads[i], NULL, producer_thread, &producers[i]);
    }

    double start = now_ns();
    go = 1;
    for (int i = 0; i < PORTS; i++)
        pthread_join(threads[i], NULL);
    double elapsed = now_ns() - start;

    /* The tasks were built during the timed section and are sitting in
     * the queue; running them is not part of the reading. */
    pool_release(m->pool);
    pool_join(m->pool);

    /* Every port received the same count, so every port's values are
     * consumed by the same number of complete input sets. Nothing may
     * go missing: a delivery path that drops values would post a
     * flattering time for the wrong reason. */
    long expected_runs = VALUES_PER_PORT;
    long expected_total = (long)VALUES_PER_PORT * PORTS;
    if (sink_runs != expected_runs) {
        fprintf(stderr, "%s: the station ran %ld times, expected %ld\n",
                label, (long)sink_runs, expected_runs);
        exit(1);
    }
    if (sink_total != expected_total) {
        fprintf(stderr, "%s: %ld values accounted for, expected %ld\n",
                label, (long)sink_total, expected_total);
        exit(1);
    }

    map_destroy(m);

    long deliveries = (long)VALUES_PER_PORT * PORTS;
    return elapsed / (double)deliveries;
}
/* }}} */

int main(void)
{
    double large = measure("large", bulk_sink__call, (int)sizeof(bulk_t));
    double small = measure("small", small_sink__call, (int)sizeof(int));

    printf("  %d ports, %d values each, %d threads contending\n",
           PORTS, VALUES_PER_PORT, PORTS);
    printf("    %3zu-byte values: %6.1f ns per delivery   <- the one 210c moves\n",
           sizeof(bulk_t), large);
    printf("    %3zu-byte values: %6.1f ns per delivery   <- the control\n",
           sizeof(int), small);
    printf("    ratio %.2fx — the copy's share of a delivery today\n",
           large / small);
    return 0;
}
