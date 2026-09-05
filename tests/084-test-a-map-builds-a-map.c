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
#include "cera.h"

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
    map_place_box(builder, adder, "program_add", STATION_PLAIN);
    must_take(map_name_station(builder, adder, "adder"), "a name");

    must_take(map_configure_port(builder, adder, 0, IN_PORT_STATIC, address),
              "the program to build into");
    must_take(map_configure_port(builder, adder, 1, IN_PORT_STATIC,
                                 "\"seven\""),
              "the thing to add");

    /*
     * The second operation, and the one that makes what the first
     * built into a **program** rather than a graph: mark the station
     * it placed as the way out.
     *
     * It had to exist as a box the moment a program was required to
     * say where its results come from (issue 209). Placing stations
     * and drawing wires can assemble any shape; nothing among them
     * could produce something that would *run*, because the thing
     * that turns reachable internals into a surface is this mark.
     *
     * **The part travels down a wire**, which is the whole
     * demonstration. The first operation returns what it added; the
     * second takes something to mark. One is the other, and the
     * engine moves it between them the way it moves any value — no
     * state kept between the two, no order arranged, just a wire and
     * the ordinary rule that a station runs when its ports are full.
     *
     * **A box added is a part whose way in and way out are the same
     * station**, so marking it as a door needs no special case: the
     * mark lands on the one station either way. Adding a whole map
     * would produce a part with two different stations in it and this
     * line would read identically.
     */
    int doorman = map_add_station(builder);
    map_place_box(builder, doorman, "program_set_door", STATION_PLAIN);
    must_take(map_name_station(builder, doorman, "doorman"), "a name");
    must_take(map_configure_port(builder, doorman, 0, IN_PORT_STATIC, address),
              "the program to mark a door in");
    /* Port 1 arrives by wire — it is the part the first operation
     * made, travelling whole. Nothing takes it apart. */
    must_take(map_configure_port(builder, doorman, 2, IN_PORT_STATIC, "2"),
              "which way the door faces");
    must_take(map_wire(builder, adder, 0, doorman, 1),
              "the wire carrying a station index between two operations");

    /* The builder's own way out is the answer to "did it work". */
    must_take(map_designate_output(builder, doorman), "the builder's way out");

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
    /* A station carries the box's **address** — the file it lives in
     * and the function within it (issue 311a) — so this checks the
     * function half rather than the whole string. */
    const char *placed_colon = made->box_name ? strrchr(made->box_name, ':')
                                              : NULL;
    if (!placed_colon || strcmp(placed_colon + 1, "seven") != 0) {
        fprintf(stderr, "the station the builder made is running '%s'\n",
                made->box_name ? made->box_name : "(nothing)");
        return 1;
    }
    if (made->door != DOOR_OUT) {
        fprintf(stderr, "the builder placed the station but did not mark "
                        "it as the way out\n");
        return 1;
    }
    int worked = 0;
    if (!map_output_take(builder, doorman, &worked, sizeof worked)
        || worked != 1) {
        fprintf(stderr, "the builder's own result says the door was not "
                        "marked\n");
        return 1;
    }
    printf("  a map placed a station into another map and marked it as "
           "that map's way out\n");

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
