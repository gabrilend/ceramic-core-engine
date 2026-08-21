/*
 * 042-loader.c — the moment the two halves of a program meet.
 *
 * What this is: the loader (issues 602–605). The binary holds a
 * registry of boxes and no map; the file holds a map and no code.
 * This file walks the parsed description twice — create everything,
 * then connect everything — checks every wire against the registry
 * at the first moment both ends are known, validates what only the
 * whole map can show, and seeds the first tasks.
 *
 * How it does it, in general terms: declaration order must not
 * matter (an arrow may point at a station declared further down),
 * which is the entire reason there are two passes. The name lookup
 * table lives only as long as loading. Validation failures are
 * collected and printed together before stopping — someone fixing a
 * new map wants the whole list — and the error messages carry
 * station names and type names, because they are the surface a
 * person actually touches.
 */
#include "040-mapfile.h"
#include "026-registry.h"
#include "091-stopping.h"

/* A box added while some earlier process ran; see 073-latebox.h. It
 * is declared here rather than included, because the loader needs one
 * function from that file and nothing else it offers. */
const box_info_t *registry_recover_box(const char *name);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Where the most recent load's time went (issue 606's breakdown). */
map_load_timing_t map_load_last_timing;

/* {{{ stamp() */
static double stamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ die_load() */
static void die_load(const char *path, int line, const char *station,
                     const char *what)
{
    /*
     * **Exit 65, meaning the input was malformed** (issue 106), rather
     * than aborting. A core dump says nothing about a mistyped box
     * name, and a shell script that wants to tell "the file was
     * wrong" from "the machine ran out of memory" could not, because
     * both arrived as the same abnormal death.
     */
    char said[768];
    if (station)
        snprintf(said, sizeof said, "map %s:%d: station '%s': %s",
                 path, line, station, what);
    else
        snprintf(said, sizeof said, "map %s:%d: %s", path, line, what);
    sora_stop_now(NULL, SORA_EXIT_BAD_FILE, said);
}
/* }}} */

/* {{{ find_station_index() */
/*
 * Which station a name belongs to, **asked of the description being
 * read and of nothing else** (issue 210g).
 *
 * There used to be a private lookup table here: the description
 * records in file order, searched by name, freed when loading ended.
 * That was a second copy of something, and the copy is gone — this
 * searches the description itself, which the reader is holding
 * anyway, and translates through the table saying where each of its
 * stations landed.
 *
 * **The scope is the point.** A name is an arbitrary label with no
 * mechanical meaning anywhere in the engine; the one place it does
 * any work is inside a description, because text has no indices and
 * an arrow written down has to say something. Two stations in one
 * program may share a name; two stations in one file may not, and the
 * parser refuses that. So a description read into a program that
 * already has stations resolves its arrows among its own, and an
 * arrow to `gate` can never land on somebody else's `gate`.
 */
static int find_station_index(map_description_t *d, const int *at,
                              const char *name)
{
    int i = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, i++)
        if (strcmp(s->name, name) == 0)
            return at[i];
    return -1;
}
/* }}} */

/* {{{ first_pass() */
/*
 * Create every station (issue 602): registry lookup, port array with
 * ring buffers as the default, the comparator's extra port, statics
 * bound from their entries.
 *
 * Every input line is resolvable here now. They used to divide: a
 * static bound immediately, while a gather source was a *name*, and a
 * name may belong to a station declared further down the file, so it
 * had to wait for the second pass. With the pull path gone (issue
 * 210) an input line names an entry number, and a number needs
 * nothing else to exist first.
 */
static void first_pass(map_t *m, map_description_t *d, int *at)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        /*
         * One place at a time, the same call a running program makes
         * to add a station (issue 211). The index it hands back is the
         * one this station will answer to forever.
         *
         * **What the description says and where it lands are two
         * different numbers, and they are not related by an offset.**
         * That was the plan and it is not true: adding a station hands
         * back a *freed* place before it grows the table, so reading a
         * description into a program that has had removals gets
         * whatever holes exist, in whatever order. The offset is what
         * the translation degenerates to when nothing has been removed
         * — which is every program that has never had a station taken
         * out, and is why the offset story reads correctly right up
         * until it does not.
         *
         * So the reader keeps a small table of where each of the
         * description's stations landed, and translates through it.
         * Both passes read that table; nothing else ever sees it
         * (issue 217).
         */
        at[index] = map_add_station(m);
        if (at[index] < 0)
            die_load(d->path, s->line, s->name,
                     "the station table would not grow");

        /*
         * **The name goes on immediately** (issue 210g), rather than
         * in a sweep after both passes as it used to. Two things
         * follow, and the second is the reason: this file no longer
         * keeps its own lookup table, and every refusal raised from
         * here onwards — a port that does not exist, an arrow onto a
         * static, a width that disagrees — can say which station it
         * is about in the word the file's author typed.
         */
        const char *named = map_name_station(m, at[index], s->name);
        if (named)
            die_load(d->path, s->line, s->name, named);

        const box_info_t *b = registry_find(s->box);
        if (!b) {
            /* Before giving up: a box added while some earlier
             * process ran left its source behind under its own name,
             * and this may be that program's dump being reloaded
             * (issue 310). Recovery compiles it back and says out
             * loud that it did. */
            b = registry_recover_box(s->box);
        }
        if (!b) {
            /* The most common error a map will ever have; its
             * message should be the best one in the program. */
            char message[256];
            snprintf(message, sizeof message,
                     "no box named '%s' exists — misspelled, or its function "
                     "is not in a file under src/boxes/", s->box);
            die_load(d->path, s->line, s->name, message);
        }
        map_place_box(m, at[index], s->box, s->kind);

        /* A door, if the line said so (issues 209, 213). Through the
         * same call anybody else would make — reading a file has no
         * privileges here either. */
        if (s->door != DOOR_NONE) {
            const char *no = s->door == DOOR_IN
                           ? map_designate_input(m, at[index])
                           : map_designate_output(m, at[index]);
            if (no)
                die_load(d->path, s->line, s->name, no);
        }

        /*
         * **Every port line below is one call on the configuration
         * surface** (issue 210g), and the loader has nothing of its
         * own left on this path.
         *
         * It used to check the port number by hand, then reach for
         * whichever of three differently-shaped calls the line seemed
         * to want: one to set a depth, one to convert a tag, one to
         * bind a constant. Two of those three are one call now, the
         * check they each needed lives inside it, and the loader's
         * part is to hand the refusal upward with the file and the
         * line stuck to the front of it.
         *
         * The wording did not get worse for moving. The good version
         * of "that port does not exist" — the one that names the box
         * and remembers a comparator's threshold — was this file's,
         * and it went down into the surface with the check.
         */
        for (desc_input_t *in = s->inputs; in; in = in->next) {
            /*
             * A starting depth, if the line gave one, before anything
             * else touches the port — it sizes the slots, and sizing
             * them after a value has been put in them would be a
             * reallocation nobody asked for (issue 210b).
             */
            if (in->depth > 0) {
                const char *no = map_in_port_start_depth(m, at[index],
                                                         in->port, in->depth);
                if (no)
                    die_load(d->path, in->line, s->name, no);
            }

            /*
             * A bare dash: this port has no source yet. Nothing is
             * written into it and nothing is invented for it — the
             * station simply never becomes ready, which is an
             * ordinary state rather than a fault. It is what lets a
             * half-built program be a real program (issue 210b).
             */
            if (in->is_none) {
                const char *no = map_configure_port(m, at[index], in->port,
                                                    IN_PORT_NONE, NULL);
                if (no)
                    die_load(d->path, in->line, s->name, no);
                continue;
            }

            /* A depth and nothing else: the line said how deep, which
             * is already done above, and said nothing about the
             * source — so the port stays the buffer it was placed as.
             * There is nothing further to do, and doing nothing is
             * the whole of this case. */
            if (!in->is_static && !in->text)
                continue;

            /* The other two forms end here, with text going into this
             * port at this port's own type (issue 401). The `statics`
             * section is notation and nothing more: its text is copied
             * into every port that names an entry, and the entry has
             * then done its job. Two ports naming one entry end up
             * with two independent values — writing one cannot disturb
             * the other, and neither can be shaped by the other's
             * type, which is a hazard that stops being expressible
             * rather than being better documented. */
            const char *text = in->text;
            if (in->is_static) {
                for (desc_static_t *e = d->statics; e; e = e->next)
                    if (e->id == in->static_id) {
                        text = e->text;
                        break;
                    }
                if (!text) {
                    char message[256];
                    snprintf(message, sizeof message,
                             "'in %d $%d' names a statics entry the file does "
                             "not give a value for", in->port, in->static_id);
                    die_load(d->path, in->line, s->name, message);
                }
            }
            const char *no = map_configure_port(m, at[index], in->port,
                                                IN_PORT_STATIC, text);
            if (no)
                die_load(d->path, in->line, s->name, no);
        }
    }
}
/* }}} */

/* {{{ second_pass() */
/*
 * Resolve every arrow (issue 603). By now every station exists and
 * can be found by name, which is the whole reason for a second pass:
 * an arrow names its destination, and a file may draw an arrow to a
 * station it has not declared yet.
 *
 * **The wire check that used to live here is gone** (issue 210g).
 * There was a `type_check_wire` in this file that compared the box's
 * return width against the destination port's, printed the two type
 * names and the two widths, and stopped the program — and the wiring
 * operation performs exactly that check, in exactly those words, for
 * every caller. Keeping both meant this file could decide what a
 * legal wire is, which is the capability that had to stop existing
 * for there to be one construction path rather than two that agree by
 * inspection.
 *
 * So do the one thing the surface cannot: turn a *name* into an index,
 * which is a fact about the file being read and about nothing else.
 * Then draw the wire, and hand any refusal upward with the file and
 * the line in front of it.
 */
static void second_pass(map_t *m, map_description_t *d, const int *at)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        for (desc_output_t *out = s->outputs; out; out = out->next) {
            int dest = find_station_index(d, at, out->dest_station);
            if (dest < 0) {
                char message[256];
                snprintf(message, sizeof message,
                         "arrow to '%s', which does not exist",
                         out->dest_station);
                die_load(d->path, out->line, s->name, message);
            }
            const char *no = map_wire(m, at[index], out->port, dest,
                                      out->dest_port);
            if (no)
                die_load(d->path, out->line, s->name, no);
        }
    }
}
/* }}} */

/* {{{ map_load_file() */
map_t *map_load_file(const char *path, int n_workers)
{
    double t0 = stamp();
    map_description_t *d = mapfile_parse(path);
    double t1 = stamp();

    /*
     * An empty table, grown one station at a time as the file is read
     * (issue 211). It used to count the station lines and allocate
     * exactly that many, which meant reading a map was a different act
     * from adding a station to a running program — and under one
     * construction surface it should not be. This is the step that
     * proves the mechanism, because every existing test loads a
     * program.
     */
    map_t *m = map_create_empty();

    /*
     * An empty program, so every station lands where the description
     * said. The translation table is filled and read anyway, because
     * *this* caller getting the identity translation is a fact about
     * this caller and not about the mechanism (issue 217).
     */
    int *at = calloc((size_t)(d->n_stations > 0 ? d->n_stations : 1),
                     sizeof *at);
    if (!at)
        die_load(path, 0, NULL, "out of memory reading a map");

    first_pass(m, d, at);
    double t2 = stamp();
    second_pass(m, d, at);
    free(at);
    double t3 = stamp();

    /* The pool exists before the seed so the seed has somewhere to
     * push, but its workers stay parked until the caller releases —
     * the seeding window issue 102 built. */
    map_start(m, n_workers);

    /*
     * **Reading a file no longer validates or seeds; it asks for the
     * program to be brought up, the same as anybody else would**
     * (issue 212). The checks and the seed were the last thing the
     * loader could do that nothing else could, and with them moved
     * there is no state called *still loading* left for it to be in.
     */
    const char *no = map_bring_up(m);
    if (no)
        die_load(path, 0, NULL, no);

    /*
     * **Whether starting nothing is a fault is the caller's to say**,
     * and this caller says yes.
     *
     * Bringing a program up is repeatable, so seeding nothing is
     * perfectly ordinary — a program brought up, grown by one
     * station, and brought up again seeds nothing the second time and
     * should not be scolded for it. But a *file* somebody asked to be
     * run is a different promise: if no station can start without
     * waiting for a value, and no value can arrive because nothing is
     * running to send one, then the program does nothing at all, and
     * saying so is more use than starting it.
     */
    /*
     * **Unless the program has a declared entrance**, in which case
     * waiting is exactly what it is supposed to do (issue 213). This
     * refusal means "nothing can start and nothing can arrive, so
     * this program will do nothing at all" — and a declared entrance
     * is a station something outside delivers to, which makes the
     * second half of that false.
     *
     * The same escape clause the unfed-inputs warning gained, and for
     * the same reason: both were written when there was no way for a
     * program to say it expected to be fed, so both had to assume the
     * worst.
     */
    int has_entrance = 0;
    for (int i = 0; i < m->n_stations; i++)
        if (map_station(m, i)->door == DOOR_IN)
            has_entrance = 1;

    if (map_seed_count(m) == 0 && !has_entrance)
        die_load(path, 0, NULL,
                 "nothing to seed — every station waits for a buffered "
                 "value, so the map cannot ever start");
    double t4 = stamp();

    /*
     * Four stages rather than five (issue 210g). Naming used to be a
     * sweep of its own between wiring and starting, and it was timed
     * as "validation" because validating is what else happened there.
     * Naming happens as each station is created now, so what is left
     * between the wires and the first task is the bring-up, which is
     * what this last number should have been called all along.
     */
    map_load_last_timing.parse = t1 - t0;
    map_load_last_timing.first_pass = t2 - t1;
    map_load_last_timing.second_pass = t3 - t2;
    map_load_last_timing.bring_up = t4 - t3;

    mapfile_free(d);
    return m;
}
/* }}} */

/* {{{ map_instantiate_file() */
/*
 * **Bring a description inside a program that already exists** (issue
 * 217) — the operation this whole file turns out to have been, with
 * the program fixed at "a fresh empty one".
 *
 * **It is instantiating a template, not merging two programs.** There
 * is no second running program being picked up and carried, no handle
 * that becomes invalid, no table stitched onto another table. There
 * is a *description* and there is a table with some number of
 * stations in it; this builds new stations for the description's
 * stations and wires them the way the description says. One
 * description can be instantiated as many times into one program as
 * anybody likes, with nothing shared between the copies — separate
 * stations, separate buffers, separate constants.
 *
 * **So no wire is rewritten.** The description says its third station
 * feeds its fifth; that becomes wherever the third landed feeding
 * wherever the fifth landed. Nothing that already exists is
 * renumbered, so the invariant this engine rests on — an index means
 * what it meant — is not approached, let alone bent.
 *
 * **Legal at any moment**, because every operation it is made of is:
 * adding a station, naming one, placing a box, configuring a port,
 * drawing a wire. A program with workers in flight gains a subgraph
 * the same way it gains a station.
 *
 * The caller gets a handle it can find the instance's doors through,
 * and **the doors are all it should want**. A parent wiring into an
 * interior station of an instance is reaching inside, which is the
 * thing the marks exist to stop happening by accident.
 */
map_instance_t map_instantiate_file(map_t *m, const char *path)
{
    map_description_t *d = mapfile_parse(path);

    map_instance_t in;
    in.count = d->n_stations;
    in.station = calloc((size_t)(in.count > 0 ? in.count : 1),
                        sizeof *in.station);
    if (!in.station)
        die_load(path, 0, NULL, "out of memory instantiating a map");

    first_pass(m, d, in.station);
    second_pass(m, d, in.station);

    mapfile_free(d);
    return in;
}
/* }}} */

/* {{{ map_instance_door() / map_instance_free() */
/*
 * **The nth station of this instance facing that way**, or -1.
 *
 * This is the whole of what a parent is entitled to know about
 * something it brought inside itself. It could reach any of the
 * instance's stations through the handle — the translation table is
 * right there — and doing so would be reaching inside a thing whose
 * author may rename or restructure anything that is not a door.
 *
 * A program may have several of each, so the nth rather than the
 * only. They come back in the order the description declared them,
 * which is the one order a description can be said to have.
 */
static int map_instance_door(map_t *m, const map_instance_t *in,
                             int facing, int nth)
{
    int seen = 0;
    for (int i = 0; i < in->count; i++) {
        station_t *s = map_station(m, in->station[i]);
        if (s->call && s->door == facing && seen++ == nth)
            return in->station[i];
    }
    return -1;
}

int map_instance_entrance(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_IN, nth);
}

int map_instance_result(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_OUT, nth);
}

/*
 * The handle goes; the stations stay. Nothing in the running program
 * refers to this — it was the reader's note to itself about where
 * things landed, and a parent keeps it only for as long as it is
 * still deciding what to wire.
 */
void map_instance_free(map_instance_t *in)
{
    free(in->station);
    in->station = NULL;
    in->count = 0;
}
/* }}} */

/* {{{ map_seed_count() */
int map_seed_count(map_t *m)
{
    return m->seeded;
}
/* }}} */
