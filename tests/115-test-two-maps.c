/*
 * 115-test-two-maps.c — two programs, two pools, one process.
 *
 * What this proves: a process can hold more than one running program,
 * each with its own workers, and neither can see the other. This is the
 * test that could not have passed while the engine kept a process-wide
 * pointer to "the active map" — a box reaching its statics went through
 * that pointer, so a second program silently wrote into the first.
 *
 * Why it is worth a file of its own rather than a scene in another
 * test. A singleton is invisible until two of something exist. Nothing
 * else here builds two programs and runs them at the same time, so
 * without this the claim rests on the absence of a global that somebody
 * would have to go looking for.
 *
 * Not the same as starting a program beside another (087), which shares
 * one pool and reaches the second through its doors. These two share
 * nothing at all and do not know about each other.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static void must_take(const char *refusal, const char *what) */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "  refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ static cera_map_t *a_program(int constant, int *entrance, int *result) */
/*
 * Two stations: an entrance the caller feeds, and an adder holding a
 * constant on its other port. Feed it n and the result is n + constant,
 * so two programs built with different constants give different answers
 * from the same input — which is what makes interference visible rather
 * than merely absent.
 */
static cera_map_t *a_program(int constant, int *entrance, int *result)
{
    cera_map_t *m = cera_map_create_empty();

    int in = cera_map_add_station(m);
    cera_map_place_box(m, in, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, in, "in"), "a name");
    must_take(cera_map_designate_input(m, in), "an entrance");

    int add = cera_map_add_station(m);
    cera_map_place_box(m, add, "add", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, add, "total"), "a name");
    must_take(cera_map_designate_output(m, add), "a result");

    must_take(cera_map_wire(m, in, 0, add, 0), "a wire");

    char text[32];
    snprintf(text, sizeof text, "%d", constant);
    cera_map_in_port_static_text(m, add, 1, text);

    *entrance = in;
    *result = add;
    return m;
}
/* }}} */

/* {{{ int main(void) */
int main(void)
{
    int failures = 0;

    int a_in, a_out, b_in, b_out;
    cera_map_t *a = a_program(100, &a_in, &a_out);
    cera_map_t *b = a_program(200, &b_in, &b_out);

    /* Both running at once, each on its own workers. The promise is
     * made before either gate opens, so neither pool can decide it has
     * finished while the other is still being fed. */
    cera_map_start(a, 2);
    cera_pool_submitter_register(a->pool);
    must_take(cera_map_bring_up(a), "the first program");

    cera_map_start(b, 2);
    cera_pool_submitter_register(b->pool);
    must_take(cera_map_bring_up(b), "the second program");

    if (a->pool == b->pool) {
        fprintf(stderr, "  the two programs share a pool; this proves nothing\n");
        return 1;
    }

    cera_pool_release(a->pool);
    cera_pool_release(b->pool);

    /* Interleaved on purpose: if either program could reach the other's
     * state, feeding them alternately is when it would show. */
    for (int i = 1; i <= 20; i++) {
        must_take(cera_map_deliver_argument(a, a_in, 0, &i, sizeof i),
                  "an argument to the first");
        must_take(cera_map_deliver_argument(b, b_in, 0, &i, sizeof i),
                  "an argument to the second");
    }

    cera_pool_submitter_unregister(a->pool);
    cera_pool_submitter_unregister(b->pool);
    cera_pool_join(a->pool);
    cera_pool_join(b->pool);

    /* Every answer each program gave, checked against what that
     * program's own constant says it should be. */
    int wanted_a = 0, wanted_b = 0;
    for (int i = 1; i <= 20; i++) { wanted_a += i + 100; wanted_b += i + 200; }

    int got_a = 0, seen_a = 0, value = 0;
    while (cera_map_output_take(a, a_out, &value, sizeof value)) {
        got_a += value; seen_a++;
    }
    int got_b = 0, seen_b = 0;
    while (cera_map_output_take(b, b_out, &value, sizeof value)) {
        got_b += value; seen_b++;
    }

    if (seen_a != 20 || seen_b != 20) {
        fprintf(stderr, "  %d results from the first and %d from the second, "
                        "not 20 each\n", seen_a, seen_b);
        failures++;
    }
    if (got_a != wanted_a || got_b != wanted_b) {
        fprintf(stderr, "  the first totalled %d (wanted %d) and the second "
                        "%d (wanted %d) — one program's constant reached the "
                        "other\n", got_a, wanted_a, got_b, wanted_b);
        failures++;
    }

    if (!failures)
        printf("  two programs ran on two pools in one process and neither "
               "saw the other\n");

    /* And each is destroyed on its own, leaving the other whole. */
    cera_map_destroy(a);
    if (cera_map_station(b, b_out) == NULL) {
        fprintf(stderr, "  destroying the first took the second with it\n");
        failures++;
    } else {
        printf("  destroying one left the other standing\n");
    }
    cera_map_destroy(b);

    return failures ? 1 : 0;
}
/* }}} */
