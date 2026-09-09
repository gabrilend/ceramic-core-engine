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
#include "cera.h"

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
    cera_map_t *first = cera_map_create_empty();
    int keeper = cera_map_add_station(first);
    cera_map_place_box(first, keeper, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(first, keeper, "keeper"), "a name");
    must_take(cera_map_designate_argument(first, keeper, 0, 0), "an entrance");

    /*
     * A way out, with nothing wired into it yet. Every program
     * declares where its results come from (issue 209), and this is
     * the case that requirement is shaped around: the declaration is
     * the interface, and what flows through it is a separate matter.
     *
     * It is also what makes the demonstration below possible, because
     * it gives this program a station 1 — and the second program has
     * one too, and they are not the same station.
     */
    int outcome = cera_map_add_station(first);
    cera_map_place_box(first, outcome, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(first, outcome, "outcome"), "a name");
    must_take(cera_map_designate_result(first, outcome, 0, 0), "a result");

    cera_map_start(first, 3);
    cera_pool_submitter_register(first->pool);
    /* Its way out has nothing wired into it yet — the wire arrives
     * further down — so bringing it up says so. That notice is this
     * program being honest, not this program being wrong. */
    printf("  (the notice below is a way out nobody has wired yet)\n");
    fflush(stdout);
    must_take(cera_map_bring_up(first), "the first program");
    /* Somewhere to put each program's results, said before the
     * workers are let go: a value reaching a result with nowhere
     * registered is discarded like any other unwired value. */
    int homeward_landed[4] = { 0 };
    int answers[8] = { 0 };
    must_take(cera_map_collect(first, outcome, 0, homeward_landed, 4,
                               (int)sizeof homeward_landed[0]),
              "somewhere to put the first program's result");
    cera_pool_release(first->pool);

    /*
     * A second program, started beside the first. Its own station
     * table, its own everything — the same workers.
     */
    cera_map_t *second = cera_map_start_beside(first);
    int gate = cera_map_add_station(second);
    cera_map_place_box(second, gate, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(second, gate, "gate"), "a name");

    int answer = cera_map_add_station(second);
    cera_map_place_box(second, answer, "double_it", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(second, answer, "answer"), "a name");

    must_take(cera_map_wire(second, gate, 0, answer, 0), "a wire");
    must_take(cera_map_designate_argument(second, gate, 0, 0), "an entrance");
    must_take(cera_map_designate_result(second, answer, 0, 0), "a result");
    must_take(cera_map_bring_up(second), "the second program");

    if (second->pool != first->pool) {
        fprintf(stderr, "the second program did not share the workers\n");
        return 1;
    }

    /*
     * **You cannot wire across, and nothing refuses you.** That is the
     * uncomfortable half of the claim and the half worth proving.
     *
     * Both programs have a station 1. They are different stations,
     * because an index is only meaningful inside one table. So asking
     * the first program to wire into "station 1" is not refused by
     * some rule about programs — there is no such rule and no such
     * check. It draws a perfectly ordinary wire, **inside the first
     * program**, to the first program's own station 1.
     *
     * This scene used to assert a refusal, which was true only because
     * the first program happened to have no station 1 to land on. That
     * made it a test of an accident. What is actually guaranteed is
     * narrower and more useful to know: a wire cannot leave a table,
     * so a request that looks like it crosses lands at home instead.
     * Reaching a program started beside you is what its entrance is
     * for, and there is no second way.
     */
    if (cera_map_station(first, 1) == cera_map_station(second, 1)) {
        fprintf(stderr, "the two station 1s are one record, so this proves "
                        "nothing\n");
        return 1;
    }
    must_take(cera_map_wire(first, keeper, 0, 1, 0),
              "a wire that looks like it crosses");

    /* The second program's turn, once it exists. */
    must_take(cera_map_collect(second, answer, 0, answers, 8,
                               (int)sizeof answers[0]),
              "somewhere to put the second program's results");

    int homeward = 21;
    must_take(cera_map_deliver_argument(first, keeper, 0, &homeward,
                                   sizeof homeward),
              "a value into the first program");

    /* Reached the only way anything outside reaches a program. */
    for (int i = 1; i <= 4; i++) {
        int argument = i;
        must_take(cera_map_deliver_argument(second, gate, 0, &argument,
                                       sizeof argument),
                  "an argument");
    }

    cera_pool_submitter_unregister(first->pool);
    cera_pool_join(first->pool);

    /*
     * The value went to the first program's own station 1 — doubled
     * to 42 and landed in the first program's own array — rather than to the second
     * program's station 1, which doubles too and would have produced
     * the same number somewhere else entirely. Two stations, one
     * index, and the wire stayed home.
     */
    if (cera_map_collected(first, outcome, 0) != 1
        || homeward_landed[0] != 42) {
        fprintf(stderr, "the wire that looked like it crossed did not "
                        "land in the first program\n");
        return 1;
    }
    printf("  a wire naming another program's index drew an ordinary wire "
           "at home instead; indices do not cross\n");

    if (cera_map_collected(second, answer, 0) != 4) {
        fprintf(stderr, "the second program produced %d results, not 4\n",
                cera_map_collected(second, answer, 0));
        return 1;
    }
    int total = 0;
    for (int i = 0; i < 4; i++)
        total += answers[i];
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
    cera_map_destroy(second);
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
    must_take(cera_map_deliver_argument(first, keeper, 0, &after, sizeof after),
              "an argument after the second program died");
    printf("  and the first outlived it: entrance, table and workers all "
           "still there\n");

    cera_map_destroy(first);
    return 0;
}
