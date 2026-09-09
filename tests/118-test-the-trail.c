/*
 * 118-test-the-trail.c — a program leaves a trail, and a reader follows it.
 *
 * What this proves, in four scenes:
 *
 *   1. A program built with watching on writes events, and a reader
 *      that is not the program reads them back in order.
 *   2. A reader too slow for the ring loses events and is told exactly
 *      how many — never quietly shown an incomplete picture.
 *   3. The reader's reconstruction agrees with the counters the program
 *      keeps for itself. Two independent accounts of the same run.
 *   4. A second program cannot write a ring the first still owns.
 *
 * The reader is in the same process here, which is fine: it maps the
 * file read-only and holds nothing the writer needs, so it is exactly
 * as invisible to the program as one in another process would be.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef CERA_RAM_SHARED
#define CERA_RAM_SHARED "/dev/shm"
#endif

/* {{{ static void must_take(const char *refusal, const char *what) */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "  refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ static cera_map_t *a_program(int *entrance, int *result) */
/* Feed it a number, it comes back with thirty added. Two stations, one
 * wire — small enough that every event is accountable. */
static cera_map_t *a_program(const char *path, int *entrance, int *result)
{
    cera_map_t *m = cera_map_create_empty();
    if (path)
        must_take(cera_watch_open(m, path), "the trail");

    int in = cera_map_add_station(m);
    cera_map_place_box(m, in, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, in, "in"), "a name");
    must_take(cera_map_designate_argument(m, in, 0, 0), "an entrance");

    int add = cera_map_add_station(m);
    cera_map_place_box(m, add, "add", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, add, "total"), "a name");
    must_take(cera_map_designate_result(m, add, 0, 0), "a result");

    must_take(cera_map_wire(m, in, 0, add, 0), "a wire");
    cera_map_in_port_static_text(m, add, 1, "30");

    *entrance = in;
    *result = add;
    return m;
}
/* }}} */

/* {{{ int main(void) */
int main(void)
{
    int failures = 0;

    if (!cera_watch_compiled_in()) {
        printf("  built without CERA_WATCH, so there is nothing to watch\n");
        return 0;
    }

    char path[512];
    snprintf(path, sizeof path, "%s/trail-%d.ring", CERA_RAM_SHARED, (int)getpid());

    int in, out;
    cera_map_t *m = a_program(path, &in, &out);

    /* Attached before the program runs, so nothing is missed by being
     * late rather than by being slow. */
    cera_watch_reader_t *r = cera_watch_attach(path);
    if (!r) {
        fprintf(stderr, "  could not attach to the trail just written\n");
        return 1;
    }

    /* A second program may not write a ring this one still owns. */
    cera_map_t *rival = cera_map_create_empty();
    if (cera_watch_open(rival, path) == NULL) {
        fprintf(stderr, "  a second program was allowed to write the same ring\n");
        failures++;
    } else {
        printf("  a ring already being written is refused, naming the owner\n");
    }
    cera_map_destroy(rival);

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");
    /* Somewhere to put the results, said before the workers are let
     * go: a value reaching a result with nowhere registered is
     * discarded like any other unwired value. */
    static int landed[64];
    must_take(cera_map_collect(m, out, 0, landed,
                               (int)(sizeof landed / sizeof landed[0]),
                               (int)sizeof landed[0]),
              "somewhere to put the results");

    cera_pool_release(m->pool);

    /* Few enough that the ring cannot lap: this scene is about the two
     * accounts agreeing, and a lapped ring would prove nothing. */
    const int values = 40;
    for (int i = 1; i <= values; i++)
        must_take(cera_map_deliver_argument(m, in, 0, &i, sizeof i),
                  "an argument");

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    /* Every value that went in came out, which the trail below has
     * to agree with. A reading rather than a drain: a value reaching
     * a result goes straight into the array registered for it. */
    if (cera_map_collected(m, out, 0) != values) {
        fprintf(stderr, "%d values in, %d results out\n",
                values, cera_map_collected(m, out, 0));
        return 1;
    }

    /* What the program says about itself, before it is destroyed. */
    long counted_runs[2];
    for (int i = 0; i < 2; i++)
        counted_runs[i] = atomic_load(&cera_map_station(m, i)->runs);

    /* Now read the whole trail. */
    cera_watch_event_t e;
    uint64_t lost = 0, lost_total = 0;
    long seen[CERA_WATCH_KIND_COUNT];
    long ran[2] = { 0, 0 };
    memset(seen, 0, sizeof seen);
    uint64_t last_seq = 0;
    int out_of_order = 0;

    while (cera_watch_next(r, &e, &lost)) {
        lost_total += lost;
        if (last_seq && e.seq <= last_seq)
            out_of_order++;
        last_seq = e.seq;
        if (e.kind < CERA_WATCH_KIND_COUNT)
            seen[e.kind]++;
        if (e.kind == CERA_WATCH_RAN && e.a < 2)
            ran[e.a]++;
    }

    if (out_of_order) {
        fprintf(stderr, "  %d events arrived out of sequence\n", out_of_order);
        failures++;
    }
    if (seen[CERA_WATCH_UP] != 1) {
        fprintf(stderr, "  the program came up %ld times\n", seen[CERA_WATCH_UP]);
        failures++;
    }
    if (seen[CERA_WATCH_ADDED] != 2 || seen[CERA_WATCH_WIRED] != 1) {
        fprintf(stderr, "  %ld stations placed and %ld wires drawn, not 2 and 1\n",
                seen[CERA_WATCH_ADDED], seen[CERA_WATCH_WIRED]);
        failures++;
    }

    if (lost_total == 0) {
        /* Nothing was lost, so the two accounts must agree exactly. */
        if (ran[0] != counted_runs[0] || ran[1] != counted_runs[1]) {
            fprintf(stderr, "  the trail says %ld and %ld runs; the program's own "
                            "counters say %ld and %ld\n",
                    ran[0], ran[1], counted_runs[0], counted_runs[1]);
            failures++;
        } else {
            printf("  the trail and the program's own counters agree: "
                   "%ld and %ld runs\n", ran[0], ran[1]);
        }
    } else {
        /* Losing is correct; being told is what makes it honest. */
        printf("  the reader fell behind and was told: %llu events lost\n",
               (unsigned long long)lost_total);
        if (ran[0] > counted_runs[0] || ran[1] > counted_runs[1]) {
            fprintf(stderr, "  a lossy reader saw more runs than happened\n");
            failures++;
        }
    }

    if (seen[CERA_WATCH_MOVED] == 0) {
        fprintf(stderr, "  no value was ever seen moving\n");
        failures++;
    } else if (!failures) {
        printf("  %ld deliveries and %ld tasks becoming due were watched\n",
               seen[CERA_WATCH_MOVED], seen[CERA_WATCH_DUE]);
    }

    if (!cera_watch_writer_alive(r)) {
        fprintf(stderr, "  the writer was reported dead while still running\n");
        failures++;
    }

    cera_map_destroy(m);
    if (cera_watch_writer_alive(r)) {
        fprintf(stderr, "  the writer was reported alive after the program ended\n");
        failures++;
    } else {
        printf("  and the trail says when the program it describes has ended\n");
    }

    cera_watch_detach(r);
    unlink(path);

    /*
     * And the other half of the bargain: a reader too slow for the ring
     * loses events, and is told how many rather than being left with a
     * shorter story it cannot tell is shorter.
     *
     * Nothing here reads until the program has finished, so the ring
     * laps many times over. That is the point — it is the worst a
     * reader can be, and the number it comes back with still has to
     * add up.
     */
    char busy[512];
    snprintf(busy, sizeof busy, "%s/trail-%d-busy.ring", CERA_RAM_SHARED,
             (int)getpid());
    int bin, bout;
    cera_map_t *b = a_program(busy, &bin, &bout);
    cera_watch_reader_t *slow = cera_watch_attach(busy);

    cera_map_start(b, 2);
    cera_pool_submitter_register(b->pool);
    must_take(cera_map_bring_up(b), "the busy program");
    cera_pool_release(b->pool);
    /* Enough to lap a ring of sixty-odd thousand slots several times
     * over. The ring is deliberately generous, so proving that a reader
     * which never looks is told what it missed takes real volume. */
    for (int i = 1; i <= 60000; i++)
        must_take(cera_map_deliver_argument(b, bin, 0, &i, sizeof i),
                  "an argument");
    cera_pool_submitter_unregister(b->pool);
    cera_pool_join(b->pool);

    uint64_t missed = 0, read_back = 0, first_seq = 0;
    while (cera_watch_next(slow, &e, &lost)) {
        missed += lost;
        if (!first_seq) first_seq = e.seq;
        read_back++;
    }

    if (missed == 0) {
        fprintf(stderr, "  a reader that never looked until the end lost "
                        "nothing, so the ring is not being overwritten\n");
        failures++;
    } else if (first_seq != missed + 1) {
        fprintf(stderr, "  it says it lost %llu but the first event it kept "
                        "was number %llu\n", (unsigned long long)missed,
                (unsigned long long)first_seq);
        failures++;
    } else {
        printf("  a reader that fell behind lost %llu events and said so, "
               "and the %llu it kept start exactly where the loss ends\n",
               (unsigned long long)missed, (unsigned long long)read_back);
    }

    cera_watch_detach(slow);

    /*
     * And the other half of that distinction. A reader attaching to a
     * program already well under way has **not lost** anything: it was
     * not there. It starts at the oldest event still in the ring and
     * reports where it came in, which is a different fact from falling
     * behind and must not be dressed up as one.
     */
    cera_watch_reader_t *late = cera_watch_attach(busy);
    uint64_t first = 0, before = 0;
    cera_watch_joined_at(late, &first, &before);

    uint64_t late_lost = 0, late_read = 0;
    while (cera_watch_next(late, &e, &lost)) {
        late_lost += lost;
        late_read++;
    }
    if (before == 0) {
        fprintf(stderr, "  a reader joining a finished run thinks it saw the "
                        "start of it\n");
        failures++;
    } else if (late_lost != 0) {
        fprintf(stderr, "  a reader that arrived late reported %llu lost, "
                        "which is arriving late described as a fault\n",
                (unsigned long long)late_lost);
        failures++;
    } else {
        printf("  a reader arriving late joined at event %llu with %llu behind "
               "it, and lost nothing\n",
               (unsigned long long)first, (unsigned long long)before);
    }
    cera_watch_detach(late);

    cera_map_destroy(b);
    unlink(busy);

    return failures ? 1 : 0;
}
/* }}} */
