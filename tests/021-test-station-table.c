/*
 * 021-test-station-table.c — proves the station table (issue 201).
 *
 * What this is: the test that stations are addressed by position in
 * one flat array and that nothing — in particular a growing ring
 * buffer — ever moves a station.
 *
 * How it does it, in general terms: a small map is built by hand,
 * every station's address is written down, one port is then flooded
 * until its buffer doubles several times, and every address is read
 * again. The two readings must be identical, and the neighbours'
 * buffers must be untouched.
 */
/*
 * This test reaches into the engine's own machinery — slots, pages,
 * destination sets, the constants a port holds — rather than only
 * calling what a program built with this engine calls. So it includes
 * the engine's *source* and is compiled as one unit with it (issue
 * 903). Those functions are private, and a private function cannot be
 * called from another translation unit no matter what is declared.
 *
 * A white-box test belongs inside the thing it examines. The
 * alternative was keeping the machinery public so this file could
 * reach it, which makes the test suite the reason a consumer's link
 * fails, and leaves private-by-default depending on nobody ever
 * writing another test like this one.
 *
 * The build rule for these does not also put the engine on the link
 * line: it is already here, and doing both is every symbol twice.
 */
#include "cera.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A box that is never actually run — this test is about structure,
 * not motion. Hand shim in generator shape; deleted by issue 302. */
static void unused__call(task_t *t) { (void)t; }

int main(void)
{
    enum { STATIONS = 3 };
    map_t *m = map_create(STATIONS);

    /* Two int inputs each; nothing wired, nothing started. */
    int sizes[2] = { sizeof(int), sizeof(int) };
    for (int i = 0; i < STATIONS; i++)
        map_place(m, i, unused__call, STATION_PLAIN, 2, sizes, sizeof(int));

    /* Write down where everything lives. */
    station_t *before[STATIONS];
    for (int i = 0; i < STATIONS; i++)
        before[i] = map_station(m, i);

    /* Leave a fingerprint in station 2's first port. */
    int fingerprint = 777;
    map_deliver_value(m, 2, 0, &fingerprint);

    /* Flood station 1's first port. Its second port stays empty, so
     * the station never becomes ready and the buffer must absorb
     * everything by growing. */
    for (int v = 0; v < 500; v++)
        map_deliver_value(m, 1, 0, &v);

    /* The station table must not have moved a single record. */
    for (int i = 0; i < STATIONS; i++) {
        if (map_station(m, i) != before[i]) {
            fprintf(stderr, "station %d moved when a buffer grew\n", i);
            exit(1);
        }
    }

    in_port_t *grown = &map_station(m, 1)->in_ports[0];
    if (grown->growths < 3) {
        fprintf(stderr, "expected several growths, saw %d\n", grown->growths);
        exit(1);
    }
    if (map_in_port_depth(m, 1, 0) != 500) {
        fprintf(stderr, "flooded port holds %d of 500\n", map_in_port_depth(m, 1, 0));
        exit(1);
    }

    /* The neighbour's fingerprint must have survived unmoved. */
    if (map_in_port_depth(m, 2, 0) != 1) {
        fprintf(stderr, "the neighbour's port depth changed\n");
        exit(1);
    }
    int recovered;
    memcpy(&recovered, in_port_slot(&map_station(m, 2)->in_ports[0], 0),
           sizeof recovered);
    if (recovered != 777) {
        fprintf(stderr, "the neighbour's value was disturbed: %d\n", recovered);
        exit(1);
    }

    map_destroy(m);
    printf("  stations stayed put through %d buffer doublings\n", grown->growths);
    return 0;
}
