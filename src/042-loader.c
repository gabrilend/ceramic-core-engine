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
    if (station)
        fprintf(stderr, "map %s:%d: station '%s': %s\n", path, line, station, what);
    else
        fprintf(stderr, "map %s:%d: %s\n", path, line, what);
    abort();
}
/* }}} */

/* {{{ find_station_index() */
/*
 * Which station a name belongs to, **asked of the program rather than
 * of a table this file keeps** (issue 210g).
 *
 * There used to be a private lookup table here: the description
 * records in file order, searched by name, freed when loading ended.
 * It existed because names were copied onto the map in one sweep at
 * the very end, so during the two passes the map did not know them.
 *
 * Naming is an operation now, and the reader performs it as each
 * station is created — which is what it had to become for a program
 * built by calling the surface to be describable at all (issue 212).
 * The moment that was true, the private table was a second copy of
 * something the map already held, and the refusals raised while
 * wiring could name the stations they were about instead of numbering
 * them.
 */
static int find_station_index(map_t *m, const char *name)
{
    for (int i = 0; i < m->n_named; i++)
        if (m->station_names[i] && strcmp(m->station_names[i], name) == 0)
            return i;
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
static void first_pass(map_t *m, map_description_t *d)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        /* One place at a time, the same call a running program makes
         * to add a station (issue 211). The index it hands back is the
         * one this station will answer to forever. */
        if (map_add_station(m) != index)
            die_load(d->path, s->line, s->name,
                     "the station table handed back an unexpected place");

        /*
         * **The name goes on immediately** (issue 210g), rather than
         * in a sweep after both passes as it used to. Two things
         * follow, and the second is the reason: this file no longer
         * keeps its own lookup table, and every refusal raised from
         * here onwards — a port that does not exist, an arrow onto a
         * static, a width that disagrees — can say which station it
         * is about in the word the file's author typed.
         */
        const char *named = map_name_station(m, index, s->name);
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
        map_place_box(m, index, s->box, s->kind);

        /* A door, if the line said so (issues 209, 213). Through the
         * same call anybody else would make — reading a file has no
         * privileges here either. */
        if (s->door != DOOR_NONE) {
            const char *no = s->door == DOOR_IN
                           ? map_designate_input(m, index)
                           : map_designate_output(m, index);
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
                const char *no = map_in_port_start_depth(m, index, in->port,
                                                         in->depth);
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
                const char *no = map_configure_port(m, index, in->port,
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
            const char *no = map_configure_port(m, index, in->port,
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
static void second_pass(map_t *m, map_description_t *d)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        for (desc_output_t *out = s->outputs; out; out = out->next) {
            int dest = find_station_index(m, out->dest_station);
            if (dest < 0) {
                char message[256];
                snprintf(message, sizeof message,
                         "arrow to '%s', which does not exist",
                         out->dest_station);
                die_load(d->path, out->line, s->name, message);
            }
            const char *no = map_wire(m, index, out->port, dest,
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

    first_pass(m, d);
    double t2 = stamp();
    second_pass(m, d);
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

/* {{{ map_seed_count() */
int map_seed_count(map_t *m)
{
    return m->seeded;
}
/* }}} */
