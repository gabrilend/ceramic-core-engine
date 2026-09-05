/*
 * 108-hello-graph.c — the smallest program that shows what the engine
 * is for.
 *
 * What this is: the thing a reader is pointed at first. It builds the
 * map beside it, feeds a number in, and reads the answer out — and the
 * interesting part is everything it never has to say.
 *
 * There is no code here that starts a station, none that decides which
 * branch goes first, and none that waits for the two branches to
 * rejoin. The map has a station with two input ports, so that station
 * runs when both of them hold a value. That is the entire scheduler,
 * and it is not written down anywhere because it is not a thing
 * anybody writes.
 *
 * How it does it, in general terms: the build turned the map beside
 * this file into a function that constructs it, so nothing here reads
 * any text. Feed the entrance, let the workers go, take the result.
 *
 * **One value at a time, and the last thing this prints says why.**
 * Sending several at once lets the two branches race, and a station
 * where they meet pairs whatever each of its ports hands over — there
 * is no notion of a batch or a round anywhere in this engine. That is
 * guarantee V2, it is not a defect, and an example that quietly
 * avoided it would be an example that lied about what independence
 * means.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>

int main(void)
{
    /*
     * The map, as the calls it describes. The build compiled it, so
     * finding it is a lookup in what this binary was built with rather
     * than the reading of a file.
     */
    const cera_map_build_t *program = cera_map_build_find("107-example.map");
    if (!program) {
        fprintf(stderr, "this binary was not built with 107-example.map\n");
        return 1;
    }

    cera_map_t *m = cera_map_create_empty();
    program->build(m, NULL, 0);

    /* Four workers for four stations, which is more than it needs —
     * two of them have nothing to do until the first has run. */
    cera_map_start(m, 4);
    const char *refused = cera_map_bring_up(m);
    if (refused) {
        fprintf(stderr, "the program was refused: %s\n", refused);
        return 1;
    }

    puts("");
    puts("   in ─┬─ twice ──┐");
    puts("       │          ├─ total");
    puts("       └─ plus ───┘");
    puts("");
    puts("   twice doubles it. plus adds ten. total adds those two");
    puts("   together, and cannot run until both have arrived.");
    puts("");

    const int value = 7;
    const char *no = cera_map_deliver_argument(m, 0, 0, &value,
                                          (int)sizeof value);
    if (no) {
        fprintf(stderr, "could not feed the program: %s\n", no);
        return 1;
    }

    /* The workers have been parked at the gate until now, so
     * everything above was seeding. Letting them go starts the
     * program; joining waits until nothing is left to run. */
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    int answer = 0;
    if (cera_map_output_waiting(m, 3) <= 0) {
        fprintf(stderr, "the program produced nothing\n");
        return 1;
    }
    cera_map_output_take(m, 3, &answer, (int)sizeof answer);

    printf("   fed %d  ->  twice gave %d, plus gave %d  ->  total gave %d\n",
           value, value * 2, value + 10, answer);

    long ran = 0;
    for (int i = 0; i < m->n_stations; i++)
        ran += atomic_load(&cera_map_station(m, i)->runs);
    printf("   %ld tasks ran across %d stations, and nothing anywhere\n",
           ran, m->n_stations);
    puts("   said when any of them should.");
    puts("");

    /*
     * The part worth staying for. Somebody's next instinct is to send
     * a hundred values through, and what comes back will not be a
     * hundred right answers — so it is said here, before they try it,
     * rather than left as a surprise that reads like a bug.
     */
    puts("   ── one value at a time, and here is why ──");
    puts("");
    puts("   send several at once and the two branches race. the");
    puts("   station where they meet pairs whatever each of its ports");
    puts("   hands over: there is no batch, no round, no matching set,");
    puts("   and nothing could implement one without a tag riding on");
    puts("   every value.");
    puts("");
    puts("   so total can add one value's double to another value's");
    puts("   plus-ten, and the answer belongs to neither. that is not a");
    puts("   defect to be worked around — it is what it means for the");
    puts("   two branches to be independent, which is the same property");
    puts("   that let them run at once without being told they may.");
    puts("");
    puts("   things that must stay together have to *be* one value: a");
    puts("   struct on one wire, not two values that arrive near each");
    puts("   other. see guarantees V1 and V2 in docs/058-guarantees.md.");
    puts("");

    cera_map_destroy(m);
    return 0;
}
