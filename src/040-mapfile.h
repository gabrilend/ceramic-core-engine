/*
 * 040-mapfile.h — a description on disk becoming a running program.
 *
 * What this is: the surface of phase 6, and one operation seen from
 * three distances. Read a whole description into a fresh program;
 * bring one inside a program that already exists; add a part, which
 * is either a box or a description and the caller need not know
 * which.
 *
 * How it does it, in general terms: **by compiling, not by reading**
 * (issue 311d). The text goes to the generator, which turns it into
 * the construction calls it describes, and the compiler that built
 * this binary compiles them. So a description becomes a program
 * exactly one way, and this header is a caller of that way rather
 * than a second implementation of it.
 *
 * The parser those calls are derived from lives with the compiler now
 * (scripts/099-mapparse.h) and is not part of any program built with
 * this engine. Reading text is the compiler's job.
 *
 * Failure is fatal and always names the description — and both ends
 * of a wire — because these messages are the surface a person
 * actually touches.
 */
#ifndef SORA_MAPFILE_H
#define SORA_MAPFILE_H

#include "018-station.h"

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


#endif
