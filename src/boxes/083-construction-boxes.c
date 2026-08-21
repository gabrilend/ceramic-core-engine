/*
 * 083-construction-boxes.c — the boxes a program uses to build a
 * program.
 *
 * What this is: the construction surface, wrapped as ordinary box
 * functions, so that building a program is something a *map* can do
 * rather than only something C can do (issue 212). A station running
 * one of these adds a station; a station running another draws a
 * wire. Nothing was added to the engine to allow it — these are box
 * sources like any other, and the engine has no idea they are
 * special.
 *
 * How a box says which program to act on: **by its address, carried
 * as a plain number.** A box takes its arguments by value and is not
 * permitted to reach anything ambient — the last global pointer to a
 * map was deleted on purpose, so that two programs could run in one
 * process without seeing each other. So a box that acts on a program
 * has to be handed one, and the only way to say "that particular
 * program" as a value is where it lives.
 *
 * **This is the file where the engine's one accepted risk becomes
 * real**, and it is written down rather than left to be discovered. A
 * wire is legal when both ends count the same number of bytes, which
 * is right for data: two boxes may spell one shape differently and
 * mean the same thing. An address is eight bytes on the machines this
 * runs on. So is a double, a long, and a file offset. The engine will
 * therefore accept a wire feeding a computed double into the program
 * argument of any function below, and what happens next is not a
 * wrong answer — it is a write through whatever those bytes were.
 *
 * Two things make that liveable rather than reckless. The functions
 * below **check what they can** before touching anything, so the
 * common mistakes are refused rather than followed. And 058 states
 * the residue plainly, so somebody wiring one of these knows what
 * they are handling before they mis-handle it.
 */
#include <stddef.h>
#include <stdio.h>

/*
 * A program's address, as a number a wire can carry.
 *
 * Its own name, so that the type spelled on a port says what it is
 * even though the engine compares only its width. That buys nothing
 * from the engine and everything from the person reading the map: a
 * port typed `program` is one somebody had to mean.
 */
typedef struct {
    size_t at;
} program;

/*
 * Refuse rather than follow, wherever refusing is possible.
 *
 * A null is the mistake that costs nothing to catch and is the most
 * likely one — an unwired port delivering zero, or a program that
 * failed to be made. Everything past null is indistinguishable from a
 * real address by any means available here, which is exactly the
 * thing 058 records.
 */
static map_t *program_of(program p, const char *what)
{
    if (p.at == 0) {
        fprintf(stderr, "construction: %s was asked of no program at all — "
                        "the port carrying it was never given one\n", what);
        return NULL;
    }
    return (map_t *)p.at;
}

/*
 * Add a station running a named box, and say where it landed.
 *
 * Returns the station's index, or -1 when it could not be added. An
 * index is what every wire is made of, so this is the value the rest
 * of the operations take.
 */
int program_add_station(program p, const char *box_name)
{
    map_t *m = program_of(p, "adding a station");
    if (!m)
        return -1;
    int at = map_add_station(m);
    if (at < 0)
        return -1;
    map_place_box(m, at, box_name, STATION_PLAIN);
    return at;
}

/*
 * Draw a wire from one station's output port to another's input port.
 *
 * Returns 1 when it was drawn and 0 when it was refused, with the
 * reason printed. A box returns one value, so the reason cannot come
 * back beside the answer; a map that wants to react to a refusal
 * reacts to the zero.
 */
int program_wire(program p, int from_station, int from_port,
                 int to_station, int to_port)
{
    map_t *m = program_of(p, "drawing a wire");
    if (!m)
        return 0;
    const char *no = map_wire(m, from_station, from_port,
                              to_station, to_port);
    if (no) {
        fprintf(stderr, "construction: refused a wire: %s\n", no);
        return 0;
    }
    return 1;
}

/*
 * Give a port a constant, written as text.
 *
 * Text rather than bytes, for the reason the whole engine reads
 * constants as text: bytes are exact only against the exact build
 * that wrote them, while text resolves its layout when it is read.
 */
int program_set_constant(program p, int station, int port,
                         const char *text)
{
    map_t *m = program_of(p, "setting a constant");
    if (!m)
        return 0;
    const char *no = map_configure_port(m, station, port,
                                        IN_PORT_STATIC, text);
    if (no) {
        fprintf(stderr, "construction: refused a constant: %s\n", no);
        return 0;
    }
    return 1;
}

/*
 * **Mark a station as a door, and say which way it faces.**
 *
 * One operation rather than two, because the two are one design seen
 * from either side: a program's entrance is the station the outside
 * may deliver to, and its results are where a parent wires from. A
 * station is neither or one of them, never both.
 *
 * This had to exist the moment a program was required to say where
 * its results come from (issue 209). Adding a station, drawing a wire
 * and writing a constant were enough to build a *graph*; they were
 * not enough to build a **program**, because the thing that turns
 * reachable internals into a surface is exactly this mark — and a map
 * that could build only graphs could build nothing that would run.
 *
 * `facing` is 1 for the way in and 2 for the way out, matching the
 * mark the engine keeps. A number rather than text because a wire
 * carries values, and this is one; a box that took a word would need
 * the text to have come from somewhere, and the somewhere would be a
 * constant nobody can see from the map.
 */
int program_set_door(program p, int station, int facing)
{
    map_t *m = program_of(p, "marking a door");
    if (!m)
        return 0;
    const char *no;
    if (facing == DOOR_IN)
        no = map_designate_input(m, station);
    else if (facing == DOOR_OUT)
        no = map_designate_output(m, station);
    else {
        fprintf(stderr, "construction: refused a door: %d is neither the "
                        "way in (%d) nor the way out (%d)\n",
                facing, DOOR_IN, DOOR_OUT);
        return 0;
    }
    if (no) {
        fprintf(stderr, "construction: refused a door: %s\n", no);
        return 0;
    }
    return 1;
}

/*
 * Name a station, so the program it belongs to can be written out as
 * a file that reads back.
 */
int program_name_station(program p, int station, const char *name)
{
    map_t *m = program_of(p, "naming a station");
    if (!m)
        return 0;
    const char *no = map_name_station(m, station, name);
    if (no) {
        fprintf(stderr, "construction: refused a name: %s\n", no);
        return 0;
    }
    return 1;
}
