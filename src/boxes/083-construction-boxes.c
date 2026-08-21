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
        char said[256];
        snprintf(said, sizeof said,
                 "construction: %s was asked of no program at all — the "
                 "port carrying it was never given one", what);
        sora_stop_now(NULL, SORA_EXIT_BAD_CALL, said);
    }
    return (map_t *)p.at;
}

/* {{{ static void refused() */
/*
 * **A refused instruction ends the program** (issue 106).
 *
 * These used to print the reason and return zero, on the argument
 * that a running engine should not die because a control surface sent
 * one bad instruction. The cost of that was written down at the time
 * as a debt: **a caller can ignore a return value**, and a box's
 * caller is a *wire*, which ignores everything it is not attached to.
 * A map that never wired the zero anywhere would carry on believing
 * it had just edited a program it had not edited.
 *
 * So the debt is paid off rather than serviced. A program left half
 * built by an ignored refusal is not a state anything can reach,
 * because there is no surviving path that reaches it.
 */
static void refused(map_t *m, const char *what, const char *why)
{
    char said[512];
    snprintf(said, sizeof said, "construction: refused %s: %s", what, why);
    sora_stop_now(m, SORA_EXIT_BAD_CALL, said);
}
/* }}} */

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
    int at = map_add_station(m);
    if (at < 0)
        refused(m, "a station", "the table would not grow");
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
    const char *no = map_wire(m, from_station, from_port,
                              to_station, to_port);
    if (no)
        refused(m, "a wire", no);
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
    const char *no = map_configure_port(m, station, port,
                                        IN_PORT_STATIC, text);
    if (no)
        refused(m, "a constant", no);
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
    const char *no;
    if (facing == DOOR_IN)
        no = map_designate_input(m, station);
    else if (facing == DOOR_OUT)
        no = map_designate_output(m, station);
    else {
        char which[128];
        snprintf(which, sizeof which,
                 "%d is neither the way in (%d) nor the way out (%d)",
                 facing, DOOR_IN, DOOR_OUT);
        refused(m, "a door", which);
        return 0;
    }
    if (no)
        refused(m, "a door", no);
    return 1;
}

/*
 * **Bring a described part inside this program, and wire it in.**
 *
 * One operation rather than three, and the shape was chosen against a
 * more general one that would have cost more than it bought.
 *
 * The general version returns a *handle* — where the instance's
 * entrance and its way out landed — and the caller wires them. A box
 * returns one value, so that handle is a struct of two numbers, and
 * getting the numbers out of it means a box that takes the struct and
 * returns one field. That is **a function written to fit the engine**,
 * which is the one cost this design refuses to impose (issue 209
 * refused it in the same words when a station with several output
 * ports was proposed).
 *
 * So the operation says what a composing map actually wants: *put
 * this part between here and there*. It instantiates the description,
 * wires the named station of this program into the part's first
 * entrance, and wires the part's first way out into the named
 * destination. No handle escapes, nothing needs unpacking, and every
 * argument is a number or a piece of text a wire already carries.
 *
 * What it does not reach: a part with several entrances or several
 * ways out, which needs the handle and therefore needs an answer to
 * the question above. Recorded in 217 rather than guessed at.
 */
int program_place_part(program p, const char *path,
                       int from_station, int from_port,
                       int to_station, int to_port)
{
    map_t *m = program_of(p, "placing a part");

    map_instance_t in = map_instantiate_file(m, path);
    int entrance = map_instance_entrance(m, &in, 0);
    int result = map_instance_result(m, &in, 0);
    map_instance_free(&in);

    if (entrance < 0)
        refused(m, "a part", "the description declares no entrance, so "
                             "nothing can be fed into it");
    if (result < 0)
        refused(m, "a part", "the description declares no way out, so "
                             "nothing can be taken from it");

    const char *no = map_wire(m, from_station, from_port, entrance, 0);
    if (no)
        refused(m, "the wire into a part", no);
    no = map_wire(m, result, 0, to_station, to_port);
    if (no)
        refused(m, "the wire out of a part", no);
    return 1;
}

/*
 * Name a station, so the program it belongs to can be written out as
 * a file that reads back.
 */
int program_name_station(program p, int station, const char *name)
{
    map_t *m = program_of(p, "naming a station");
    const char *no = map_name_station(m, station, name);
    if (no)
        refused(m, "a name", no);
    return 1;
}
