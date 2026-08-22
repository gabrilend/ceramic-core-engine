/*
 * 040-mapfile.h — the map file: reading it, and becoming it.
 *
 * What this is: the surface of phase 6. A program is a directory of
 * C functions and a text file; this header is where the text file
 * side comes in. The parser reads a map into a description and does
 * no construction; the loader walks the description twice, checks
 * every wire against the emitted sizes, validates what only the whole map
 * can show, and seeds the first tasks.
 *
 * How it does it, in general terms: parse and build stay separate so
 * each can be tested alone — the description is public here for
 * exactly that reason. Loading is fatal on any error, always naming
 * the station (and both ends of a wire), because the error messages
 * are the surface a person actually touches.
 */
#ifndef SORA_MAPFILE_H
#define SORA_MAPFILE_H

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
/* }}} */

/* {{{ map_load_file() — issues 602–605 */
/*
 * The whole journey: parse, build every station from its placement function
 * (first pass), resolve and type-check every arrow (second pass),
 * validate what needs the whole map, start the pool with its workers
 * parked, and seed. The caller releases the pool when ready:
 *
 *   map_t *m = map_load_file("program.map", 0);
 *   pool_release(m->pool);
 *   pool_join(m->pool);
 *   map_destroy(m);
 *
 * Every failure between here and the seed is fatal and names its
 * station; validation failures are collected and printed together
 * before stopping, because someone fixing a new map wants the whole
 * list.
 */
map_t *map_load_file(const char *path, int n_workers);
/* }}} */

/* {{{ map_seed_count() — issue 605's reading */
/* How many stations the seed enqueued on load — a map that seeds one
 * when its author expected ten has a wiring mistake. */
int map_seed_count(map_t *m);
/* }}} */

/* {{{ map_instantiate_file() — issue 217 */
/*
 * **A description brought inside a program that already exists.**
 *
 * Reading a file into a fresh program is this with the program fixed
 * at "a new empty one", which is what it always was. What changes is
 * that the program may already have stations in it, so a description
 * is a **template being instantiated** rather than a program being
 * merged: new stations are built for its stations and wired the way
 * it says, and one description can be instantiated as many times into
 * one program as anybody likes with nothing shared between the
 * copies.
 *
 * **Nothing that already exists is renumbered**, so the invariant
 * everything here rests on — an index means what it meant — is never
 * approached. What the description says and where its stations land
 * are two different numbers, related by a table rather than by an
 * offset: adding a station hands back a *freed* place before it grows
 * the table, so a program that has had removals gets whatever holes
 * exist, in whatever order. The offset is what the translation
 * degenerates to when nothing has been removed.
 *
 * Legal at any moment, because every operation it is made of is.
 */
typedef struct map_instance {
    /* Where each of the description's stations landed, in the order
     * the description declared them. The engine needs it; a parent
     * should want the doors instead. */
    int *station;
    int  count;
} map_instance_t;

map_instance_t map_instantiate_file(map_t *m, const char *path);

/*
 * **The nth door of an instance facing that way**, or -1. This is the
 * whole of what a parent is entitled to know about something it
 * brought inside itself: everything that is not a door belongs to the
 * description's author to rename or restructure.
 */
int  map_instance_entrance(map_t *m, const map_instance_t *in, int nth);
int  map_instance_result(map_t *m, const map_instance_t *in, int nth);
void map_instance_free(map_instance_t *in);
/* }}} */

/* {{{ map_add_part() / map_connect_parts() — issue 217 */
/*
 * **Adding a box and adding a map are one operation.**
 *
 * A map is a list of boxes and the wiring between them; a box is a
 * list of one. Adding a map walks its list, instantiates each box and
 * connects them the way it says; adding a box walks a list of length
 * one and connects nothing.
 *
 * **A part is where values go in and where they come out.** For a map
 * those are the stations it declared as doors. For a single box they
 * are the same station, because a box's own input ports are its way
 * in and its own output port is its way out — a box is a map of one
 * station whose doors are itself.
 *
 * That is what lets a handle be passed around without ever being
 * taken apart: everything that consumes one takes it whole, so
 * nothing has to exist whose only job is to pull a field out of it.
 *
 * Which kind a name refers to is **resolved rather than guessed**: a
 * box lives in the binary and a description lives on disk, both are
 * looked for, and finding both or neither is refused with the places
 * that were searched named.
 */
typedef struct map_part {
    int entrance;   /* where values go in, or -1 if nothing may be fed */
    int result;     /* where values come out */
} map_part_t;

const char *map_add_part(map_t *m, const char *what, map_part_t *out);
const char *map_connect_parts(map_t *m, map_part_t from, int from_port,
                              map_part_t to, int to_port);
/* }}} */

/* {{{ map_load_last_timing — the load-time cost, staged */
/*
 * Where the loading time went, in seconds, for the most recent
 * map_load_file. The number someone asks about when a map gets
 * large; measured once here so nobody has to speculate later
 * (issue 606's breakdown scene reads it).
 */
typedef struct map_load_timing {
    double parse;       /* text into a description                    */
    double first_pass;  /* every station created, named, ports set     */
    double second_pass; /* every arrow drawn                           */
    /*
     * The checks and the first tasks, together (issue 210g).
     *
     * There were two more stages here, and both stopped being stages
     * rather than getting faster. *Validation* timed a sweep that
     * copied the loader's private name table onto the map, which is
     * gone because naming happens as each station is created. *Seed*
     * timed a phase only the loader could enter, which is gone
     * because bringing a program up is something any caller does
     * (issue 212). What is left between the last wire and the first
     * task is one call, so it is one number.
     */
    double bring_up;
} map_load_timing_t;

extern map_load_timing_t map_load_last_timing;
/* }}} */

#endif
