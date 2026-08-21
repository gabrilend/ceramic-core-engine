/*
 * 084-test-a-map-builds-a-map.c — a program builds a program, using
 * nothing but the engine (issue 212).
 *
 * What this is: the point of the construction surface, demonstrated
 * rather than asserted. The operations that create structure exist as
 * ordinary boxes, so a map can perform them — a station whose box is
 * "add a station", another whose box is "draw a wire" — and nothing
 * was added to the engine to allow it.
 *
 * How it does it, in general terms: a builder program is assembled,
 * and its boxes are pointed at a second, empty program. Running the
 * builder is what puts stations and wires into the second one. Then
 * the second is brought up and asked to do its work, which is the
 * proof that what the builder made is a real program rather than a
 * shape that merely looks right from outside.
 *
 * **A program's address travels down a wire here**, which is the one
 * accepted risk in the engine becoming real. A wire is legal when
 * both ends count the same bytes, and an address is eight bytes on
 * this machine, as is a double. So the constant this test writes into
 * the builder's first port is a value the engine cannot tell from any
 * other eight-byte value — see 058, which says so plainly.
 */
#include "018-station.h"
#include "026-registry.h"
#include "049-observe.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "the surface refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

int main(void)
{
    /*
     * The program that will be built. Empty, with a pool of its own,
     * because building it and running it are separate acts and the
     * second one needs somewhere to push.
     */
    map_t *built = map_create_empty();
    map_start(built, 2);

    /*
     * The builder. Three stations, each running one construction
     * operation, each told which program to work on by a constant
     * carrying that program's address.
     *
     * Written as a decimal number, because a constant is text and the
     * reader turns text into bytes of the port's own shape. That the
     * text happens to be an address is exactly what the engine cannot
     * check.
     */
    char address[64];
    snprintf(address, sizeof address, "{ %zu }", (size_t)built);

    map_t *builder = map_create_empty();
    int adder   = map_add_station(builder);
    map_place_box(builder, adder, "program_add_station", STATION_PLAIN);
    must_take(map_name_station(builder, adder, "adder"), "a name");

    must_take(map_configure_port(builder, adder, 0, IN_PORT_STATIC, address),
              "the program to build into");
    must_take(map_configure_port(builder, adder, 1, IN_PORT_STATIC,
                                 "\"seven\""),
              "the box to place");

    map_start(builder, 2);
    must_take(map_bring_up(builder), "the builder");
    pool_release(builder->pool);
    pool_join(builder->pool);

    /*
     * The builder ran once and put one station into the other
     * program. Everything about that station came from the engine's
     * ordinary placement, so it is a station in every sense.
     */
    if (built->n_stations != 1) {
        fprintf(stderr, "the builder made %d stations, not 1\n",
                built->n_stations);
        return 1;
    }
    station_t *made = map_station(built, 0);
    if (!made->call) {
        fprintf(stderr, "the station the builder made has no box\n");
        return 1;
    }
    if (!made->box_name || strcmp(made->box_name, "seven") != 0) {
        fprintf(stderr, "the station the builder made is running '%s'\n",
                made->box_name ? made->box_name : "(nothing)");
        return 1;
    }
    printf("  a map placed a station into another map\n");

    /*
     * And the built program runs. This is the half that a shape-only
     * check would miss: a station can look right and still be
     * unrunnable, and the way to find out is to run it.
     */
    must_take(map_name_station(built, 0, "source"), "a name");
    must_take(map_bring_up(built), "the built program");
    pool_release(built->pool);
    pool_join(built->pool);

    if (atomic_load(&made->runs) != 1) {
        fprintf(stderr, "the built program's station ran %ld times, not 1\n",
                (long)atomic_load(&made->runs));
        return 1;
    }
    printf("  and the program it built came up and ran\n");

    map_destroy(builder);
    map_destroy(built);
    return 0;
}
