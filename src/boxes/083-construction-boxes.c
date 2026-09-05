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
static cera_map_t *program_of(program p, const char *what)
{
    if (p.at == 0) {
        char said[256];
        snprintf(said, sizeof said,
                 "construction: %s was asked of no program at all — the "
                 "port carrying it was never given one", what);
        cera_stop_now(NULL, CERA_EXIT_BAD_CALL, said);
    }
    return (cera_map_t *)p.at;
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
static void refused(cera_map_t *m, const char *what, const char *why)
{
    char said[512];
    snprintf(said, sizeof said, "construction: refused %s: %s", what, why);
    cera_stop_now(m, CERA_EXIT_BAD_CALL, said);
}
/* }}} */

/*
 * **What a program is made of, from a program's point of view.**
 *
 * Where values go in and where they come out. For a map brought
 * inside this one those are the stations it declared as doors; for a
 * single box they are the same station, because a box's own input
 * ports are its way in and its own output port is its way out.
 *
 * Its own name, so a port typed `part` is one somebody had to mean —
 * which buys nothing from the engine, a wire being checked by width,
 * and everything from a person reading the map.
 *
 * **It is never taken apart.** Everything below that consumes one
 * takes it whole, so there is no box here whose only job is to pull a
 * field out of it — which is the thing this design refuses to make
 * anybody write.
 */
typedef struct {
    int entrance;
    int result;
} part;

/*
 * **Add a box, or add a map. It is one operation.**
 *
 * A map is a list of boxes and the wiring between them; a box is a
 * list of one. Adding a map walks its list, instantiates each of its
 * boxes and connects them the way it says. Adding a box walks a list
 * of length one and connects nothing. What comes back is the same
 * kind of thing either way.
 *
 * A map brought in this way **shares this program's station table and
 * its workers**. There is no seam afterwards: there are stations with
 * indices, the way there always were.
 *
 * Which kind the name refers to is resolved rather than guessed — a
 * box lives in the binary and a description lives on disk, both are
 * looked for, and finding both or neither ends the program saying
 * which places were searched.
 */
part program_add(program p, const char *what)
{
    cera_map_t *m = program_of(p, "adding a part");
    part made = { -1, -1 };
    cera_map_part_t got;
    const char *no = cera_map_add_part(m, what, &got);
    if (no)
        refused(m, "a part", no);
    made.entrance = got.entrance;
    made.result = got.result;
    return made;
}

/*
 * **A wire from one part's way out to another part's way in.**
 *
 * For two single boxes this is the ordinary wire, because a box's
 * doors are itself. For two maps it crosses what used to be a seam
 * and finds nothing there to cross. The port numbers are the ones a
 * wire has always had, so a comparator's three outcomes are reachable
 * exactly as before.
 */
int program_connect(program p, part from, int from_port,
                    part to, int to_port)
{
    cera_map_t *m = program_of(p, "drawing a wire");
    cera_map_part_t a = { from.entrance, from.result };
    cera_map_part_t b = { to.entrance, to.result };
    const char *no = cera_map_connect_parts(m, a, from_port, b, to_port);
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
int program_set_constant(program p, part which, int port,
                         const char *text)
{
    cera_map_t *m = program_of(p, "setting a constant");
    /* A part's way *in* is what takes a value, whether it is a single
     * box's own port or a brought-in map's entrance. Both read the
     * same way: give this thing's port a value. */
    const char *no = cera_map_configure_port(m, which.entrance, port,
                                        CERA_IN_PORT_STATIC, text);
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
int program_set_door(program p, part which, int facing)
{
    cera_map_t *m = program_of(p, "marking a door");
    /* Marking a part as one of this program's own doors: the way in
     * for an entrance, the way out for a result. A single box has one
     * station for both, so either mark lands where it should. */
    const char *no;
    if (facing == CERA_DOOR_IN)
        no = cera_map_designate_input(m, which.entrance);
    else if (facing == CERA_DOOR_OUT)
        no = cera_map_designate_output(m, which.result);
    else {
        char which[128];
        snprintf(which, sizeof which,
                 "%d is neither the way in (%d) nor the way out (%d)",
                 facing, CERA_DOOR_IN, CERA_DOOR_OUT);
        refused(m, "a door", which);
        return 0;
    }
    if (no)
        refused(m, "a door", no);
    return 1;
}

/*
 * Name a station, so the program it belongs to can be written out as
 * a file that reads back.
 */
int program_name_station(program p, part which, const char *name)
{
    cera_map_t *m = program_of(p, "naming a station");
    /* Names a part's way in, which for a single box is the station
     * itself and for a brought-in map is the station a parent knows
     * about. The rest of a map's stations were named by its own
     * description and keep those names. */
    const char *no = cera_map_name_station(m, which.entrance, name);
    if (no)
        refused(m, "a name", no);
    return 1;
}
