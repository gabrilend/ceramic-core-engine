/*
 * 099-mapparse.h — a description, as text, becoming a description.
 *
 * What this is: the reader that turns a map file into a structure
 * saying what it said, and nothing more. It creates no stations,
 * draws no wires, and checks nothing beyond the grammar.
 *
 * **It belongs to the compiler, not to the engine** (issue 311d). A
 * running program does not read descriptions: it hands them to the
 * generator, which turns them into the construction calls they
 * describe, and those get compiled and loaded like any other code. So
 * the only thing that reads text is the thing whose job is reading
 * text, and no program built with this engine carries a parser.
 *
 * How it does it, in general terms: one pass over the lines, building
 * a linked list of stations with their inputs and outputs hanging off
 * them. Everything is allocated per description and freed together.
 *
 * What it touches of the engine: three enumerations — the station
 * kinds and the door marks — and nothing else. That is what makes it
 * something a build tool can hold.
 */
#ifndef SORA_MAPPARSE_H
#define SORA_MAPPARSE_H

#include "018-station.h"

/* {{{ the description — what the file said, nothing more */
/*
 * An input line says where one port's value comes from, and there are
 * two ways to say it.
 *
 * `in 1 $0` points at an entry in the `statics` section. That section
 * is **notation** (issue 401): a way to write a value down once while
 * describing a map and point several ports at it, resolved as the file
 * is read and retained nowhere afterwards. Two ports naming one entry
 * end up with two independent values.
 *
 * `in 1 = 5` carries the value on the line itself, which is what the
 * dump writes — it has values on ports and no entry numbers to refer
 * to, and inventing a section of numbers to point back at would be
 * notation the engine made up rather than something a person wrote.
 *
 * Both end in the same place, so `text` is what the loader uses and
 * `static_id` only says which entry the text was fetched from, for an
 * error message.
 *
 * There was a third form — a bare station name, meaning "gather from
 * there" — which went with the pull path (issue 210). The reader still
 * refuses it by name rather than reinterpreting it.
 */
typedef struct desc_input {
    int   port;
    int   is_static;      /* $n rather than an inline value */
    int   static_id;      /* which entry, when is_static */
    char *text;           /* the value as written, when inline */
    /* A bare dash: this port has no source yet (issue 210b). Not a
     * value and not a third kind of value — a state, in which the
     * station simply never becomes ready. */
    int   is_none;
    /*
     * **Values waiting in the buffer**, written `[a, b, c]` (issue
     * 712). This is the one form that describes what a program
     * *holds* rather than what it is shaped like — the difference
     * between a schematic and an image of a running program.
     *
     * The text is everything between the brackets, kept whole,
     * because the values are separated by commas and a struct value
     * has commas inside it. Whoever consumes this splits it by
     * reading one value at a time, which is the only way to tell an
     * outer comma from an inner one.
     *
     * Brackets rather than braces because braces already mean a
     * struct value; a queue is a different kind of thing, several
     * values where a constant has one.
     */
    int   is_waiting;
    /* A starting depth, written `x64` before the source; zero when
     * the line did not say. It sits before the source because the
     * inline value form runs to the end of the line, so nothing can
     * follow it, and one rule for all three forms beats a rule with
     * an exception in it.
     *
     * **A depth with nothing after it says "a buffer this deep"**,
     * and that form had to exist. The dump wrote a deepened buffer as
     * `x64 -`, which reads back as a port with *no source* — so a
     * program with a deepened buffer could be written out and could
     * not be read in again, and any arrow into that port was refused
     * on the way back. Two different things were being spelled the
     * same way. */
    int   depth;
    int   line;
    struct desc_input *next;
} desc_input_t;

typedef struct desc_output {
    int   port;
    char *dest_station;
    int   dest_port;
    int   line;
    struct desc_output *next;
} desc_output_t;

typedef struct desc_station {
    /*
     * Which door this station is, if any (issues 209, 213): written
     * as a fourth word on the station line, `entry` or `result`.
     *
     * **Deliberately not `in` and `out`.** Those already mean a port
     * on the indented lines beneath a station, and one file in which
     * a word means a port in one place and a whole station in another
     * is the shape of mistake that survives review — the same trap a
     * depth followed by a dash fell into.
     */
    int   door;
    /*
     * **Where an iterator had got to**, written `@N` (issue 712). The
     * one memory a station keeps: an iterator takes its exits in turn,
     * and reviving one pointing at the wrong exit sends the next value
     * somewhere it was never going.
     *
     * Zero on every other kind of station and on an iterator that has
     * not moved, which is where one starts — so the dump writes it
     * only when it says something.
     */
    int   cursor;
    char *name;
    char *box;
    int   kind;
    int   line;
    desc_input_t  *inputs;
    desc_output_t *outputs;
    struct desc_station *next;
} desc_station_t;

typedef struct desc_static {
    int   id;
    char *text;
    int   line;
    struct desc_static *next;
} desc_static_t;

typedef struct map_description {
    char           *path;
    desc_station_t *stations;   /* in file order */
    int             n_stations;
    desc_static_t  *statics;
    int             max_static_id;   /* -1 when no statics section */
} map_description_t;
/* }}} */

/* {{{ mapfile_parse() / mapfile_free() — issue 601 */
/*
 * Read a map file into a description. A syntax error stops the
 * program naming file, line, and what was expected — a parser that
 * skips what it does not understand produces a map with a missing
 * wire, discovered much later as a station that never runs.
 */
map_description_t *mapfile_parse(const char *path);
void mapfile_free(map_description_t *d);

/*
 * **The other direction** (issue 801): a description written back out
 * as the text it came from. The caller frees what comes back.
 *
 * This is not the dump. The dump walks a *live station table* and says
 * what the engine is actually running, which needs a running program —
 * and somebody drawing a map has a drawing, not a table. The two
 * writers answer different questions on purpose: one says what is
 * running, this says what was written down.
 *
 * A description has no capacities, no sizes and no derived facts, and
 * cannot invent them, which is what makes it the right thing to
 * produce from a drawing.
 */
char *mapfile_write(const map_description_t *d);
/* }}} */


#endif
