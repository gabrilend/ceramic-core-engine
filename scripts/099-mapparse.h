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
 * What it touches of the engine: one enumeration — the station kinds —
 * and nothing else. That is what makes it something a build tool can
 * hold.
 */
#ifndef CERA_MAPPARSE_H
#define CERA_MAPPARSE_H

#include "cera.h"

/* {{{ the description — what the file said, nothing more */
/*
 * An input line says where one port's value comes from, and there are
 * four ways to say it: a wire from another station, a constant carried
 * on the line, `$N` saying the port is one of the map's arguments, or
 * a bare dash saying nothing feeds it yet. The list is closed.
 *
 * There was a fifth form — a bare station name, meaning "gather from
 * there" — which went with the pull path (issue 210). The reader still
 * refuses it by name rather than reinterpreting it.
 */
typedef struct desc_input {
    int   port;
    /*
     * **`$N`: this port is the map's argument N** (issue 601b). It
     * used to mean "the value at entry N of the statics section",
     * which was a second spelling of a constant and read like a shell
     * positional while being nothing of the sort.
     */
    int   is_argument;
    int   argument;
    char *text;           /* the value as written, when inline */
    /* A bare dash: this port has no source yet (issue 210b). Not a
     * value and not a third kind of value — a state, in which the
     * station simply never becomes ready. */
    int   is_none;
    /*
     * **A wire, named from the receiving end** — `in 0 - feed.0`
     * (issue 601a). Every wire is written twice, once on each end, and
     * the loader refuses when the two declarations disagree; reading
     * one station then tells the whole truth about that station with
     * no scanning.
     *
     * The dash is the same dash. It has always been an arrow, and the
     * keyword says which way it points: on an `out` line away, on an
     * `in` line toward. `in 3 -` keeps its meaning exactly — an arrow
     * from nothing — because the form is unchanged and only the
     * source is absent.
     *
     * Nothing here reaches the running program. The second
     * declaration is checked while loading and then dropped; a wire
     * still exists once, as a destination record on the producing
     * station's output port, because that is the only direction
     * delivery ever asks about.
     */
    int   is_source;
    char *source_station;
    int   source_port;
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
    /* **`$N`: this port is the map's result N** (issue 601b). A mark
     * rather than an arrow — `dest_station` is null on one of these,
     * because it says where the value goes to *outside* rather than to
     * another station. */
    int   is_result;
    int   result;
    int   line;
    struct desc_output *next;
} desc_output_t;

typedef struct desc_station {
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


/* {{{ desc_shortcut_t — issue 610 */
/*
 * **A short name standing for a path**, declared before any station as
 * `name = path`, so a line does not spell out a long path every time.
 *
 * | field | type | what it holds |
 * |---|---|---|
 * | `name` | `char *` | the short name, as written |
 * | `path` | `char *` | what it stands for, as written — relative to the description, or absolute |
 * | `line` | `int` | where it was declared, for refusing a duplicate |
 *
 * **There is no such thing as a directory shortcut or a file
 * shortcut.** A shortcut is a piece of path, and it stands in for the
 * first segment of an address wherever one is used — so `math` in
 * `math/arithmetic.c` and `curves` in `curves:rotate` are the same
 * substitution, differing only in whether anything followed.
 *
 * A trailing slash on the path is therefore optional and means
 * nothing, the way it means nothing in a shell: `libs/` and `libs`
 * name one directory, and `libs//math.c` and `libs/math.c` name one
 * file. Doubled slashes are collapsed on the way out.
 */
typedef struct desc_shortcut {
    char *name;
    char *path;
    int   line;
    struct desc_shortcut *next;
} desc_shortcut_t;
/* }}} */

typedef struct map_description {
    char           *path;
    desc_station_t *stations;   /* in file order */
    int             n_stations;
    /* Where this description says to look (issue 610). Empty on a
     * description that names every path in full, which is every
     * description that was written before shortcuts existed. */
    desc_shortcut_t *shortcuts;
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

/* {{{ mapfile_source_of() — issue 610 */
/*
 * **Which file holds the box a station line names**, as a path anybody
 * can open.
 *
 * Given a description and a box address — `math/arithmetic.c:add`,
 * `curves:rotate`, `boxes/shapes.c:twice` — this applies the
 * description's shortcuts and resolves what is left against the
 * directory the description lives in. An absolute path is returned
 * unchanged.
 *
 * There is one of these because there are two callers who must agree:
 * the compiler, finding the sources to parse, and the emitter,
 * deciding which parsed box a line means. They disagreed once, and the
 * symptom was a description that built under one and was refused by
 * the other naming a file it could not find.
 *
 * The caller frees what comes back. Null when the address has no colon,
 * which the reader has already refused.
 */
char *mapfile_source_of(const map_description_t *d, const char *box_address);
/* }}} */


#endif
