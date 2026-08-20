/*
 * 040-mapfile.h — the map file: reading it, and becoming it.
 *
 * What this is: the surface of phase 6. A program is a directory of
 * C functions and a text file; this header is where the text file
 * side comes in. The parser reads a map into a description and does
 * no construction; the loader walks the description twice, checks
 * every wire against the registry, validates what only the whole map
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
    int   slot;
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
     * an exception in it. */
    int   depth;
    int   line;
    struct desc_input *next;
} desc_input_t;

typedef struct desc_output {
    int   port;
    char *dest_station;
    int   dest_slot;
    int   line;
    struct desc_output *next;
} desc_output_t;

typedef struct desc_station {
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
 * The whole journey: parse, build every station from the registry
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

/* {{{ map_load_last_timing — the load-time cost, staged */
/*
 * Where the loading time went, in seconds, for the most recent
 * map_load_file. The number someone asks about when a map gets
 * large; measured once here so nobody has to speculate later
 * (issue 606's breakdown scene reads it).
 */
typedef struct map_load_timing {
    double parse;
    double first_pass;
    double second_pass;
    double validation;
    double seed;
} map_load_timing_t;

extern map_load_timing_t map_load_last_timing;
/* }}} */

#endif
