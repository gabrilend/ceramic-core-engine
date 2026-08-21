/*
 * 087-test-starting-beside.c — starting a program is not composing
 * one (issue 212).
 *
 * What this is: the proof that a program can set another going beside
 * itself, reach it only through its doors, and outlive it.
 *
 * **The difference falls out of what a wire is.** A wire is a station
 * index and a port index, valid forever, never a pointer. An index
 * only means something inside one station table, and a station table
 * lives inside a program. So two stations can be wired together
 * exactly when they sit in the same program, and never otherwise —
 * which is why composing merges two programs into one table and
 * starting does not, and why you cannot wire to something you
 * started. You reach it the way anything outside reaches a program:
 * through its entrance.
 *
 * What is shared is the workers, and nothing else. That became
 * possible when a task started saying which program it belongs to:
 * finishing one means resolving a station index, and an index means
 * nothing without its table, so while that came from the pool a pool
 * could serve exactly one program.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

int main(void)
{
    /* The first program, with workers of its own. */
    map_t *first = map_create_empty();
    int keeper = map_add_station(first);
    map_place_box(first, keeper, "keep", STATION_PLAIN);
    must_take(map_name_station(first, keeper, "keeper"), "a name");
    must_take(map_designate_input(first, keeper), "an entrance");
    map_start(first, 3);
    pool_submitter_register(first->pool);
    must_take(map_bring_up(first), "the first program");
    pool_release(first->pool);

    /*
     * A second program, started beside the first. Its own station
     * table, its own everything — the same workers.
     */
    map_t *second = map_start_beside(first);
    int gate = map_add_station(second);
    map_place_box(second, gate, "keep", STATION_PLAIN);
    must_take(map_name_station(second, gate, "gate"), "a name");

    int answer = map_add_station(second);
    map_place_box(second, answer, "double_it", STATION_PLAIN);
    must_take(map_name_station(second, answer, "answer"), "a name");

    must_take(map_wire(second, gate, 0, answer, 0), "a wire");
    must_take(map_designate_input(second, gate), "an entrance");
    must_take(map_designate_output(second, answer), "a result");
    must_take(map_bring_up(second), "the second program");

    if (second->pool != first->pool) {
        fprintf(stderr, "the second program did not share the workers\n");
        return 1;
    }

    /*
     * **You cannot wire across.** Both programs have a station 0, and
     * they are different stations — an index is only meaningful
     * inside one table. Asking to wire from the first program's
     * station into the second's is not refused by some rule about
     * programs; it is simply a wire inside the first program, which
     * has no station 1 at all. That is what "indices do not cross
     * maps" means in practice.
     */
    if (map_wire(first, keeper, 0, 1, 0) == NULL) {
        fprintf(stderr, "a wire reached across into another program\n");
        return 1;
    }
    printf("  a wire could not reach across into a program started "
           "beside\n");

    /* Reached the only way anything outside reaches a program. */
    for (int i = 1; i <= 4; i++) {
        int argument = i;
        must_take(map_deliver_argument(second, gate, 0, &argument,
                                       sizeof argument),
                  "an argument");
    }

    pool_submitter_unregister(first->pool);
    pool_join(first->pool);

    if (map_output_waiting(second, answer) != 4) {
        fprintf(stderr, "the second program produced %d results, not 4\n",
                map_output_waiting(second, answer));
        return 1;
    }
    int total = 0;
    for (int i = 0; i < 4; i++) {
        int got = 0;
        map_output_take(second, answer, &got, sizeof got);
        total += got;
    }
    if (total != 2 + 4 + 6 + 8) {
        fprintf(stderr, "the results summed to %d, not %d\n",
                total, 2 + 4 + 6 + 8);
        return 1;
    }
    printf("  it was fed through its entrance and read from its result, "
           "on shared workers\n");

    /*
     * And the first program outlives the second. Tearing down a
     * program that borrowed the workers must not take them with it —
     * the program that made them owns them, and others may still be
     * running on them.
     */
    map_destroy(second);
    if (!first->pool) {
        fprintf(stderr, "destroying the borrower took the workers\n");
        return 1;
    }
    /*
     * The first program still accepts an argument, which is the claim
     * — its entrance, its station table and its pool all survived the
     * borrower being torn down.
     *
     * The task that argument makes will never run, because the
     * workers have already been joined by this point, and the pool
     * says so on the way out. That line is expected: what is being
     * proven here is that the program was still *there to be handed
     * to*, not that anything ran.
     */
    printf("  (the pool's parting note below is that argument, delivered "
           "after the workers went home)\n");
    fflush(stdout);
    int after = 5;
    must_take(map_deliver_argument(first, keeper, 0, &after, sizeof after),
              "an argument after the second program died");
    printf("  and the first outlived it: entrance, table and workers all "
           "still there\n");

    map_destroy(first);
    return 0;
}
