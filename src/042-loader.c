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

/* The loader's name table: file order, discarded when loading ends.
 * Names cost this one table and buy legible errors (issue 601). */
typedef struct name_table {
    desc_station_t **by_index;   /* description records, in file order */
    int              count;
} name_table_t;

/* {{{ find_station_index() */
static int find_station_index(const name_table_t *names, const char *name)
{
    for (int i = 0; i < names->count; i++)
        if (strcmp(names->by_index[i]->name, name) == 0)
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
static void first_pass(map_t *m, map_description_t *d, name_table_t *names)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        /* One place at a time, the same call a running program makes
         * to add a station (issue 211). The index it hands back is the
         * one this station will answer to forever. */
        if (map_add_station(m) != index)
            die_load(d->path, s->line, s->name,
                     "the station table handed back an unexpected place");
        names->by_index[index] = s;

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

        for (desc_input_t *in = s->inputs; in; in = in->next) {
            station_t *placed = map_station(m, index);
            if (in->port < 0 || in->port >= placed->n_in_ports) {
                char message[256];
                snprintf(message, sizeof message,
                         "'in %d' names a port that does not exist — '%s' has "
                         "%d port%s (its parameters%s)",
                         in->port, s->box, placed->n_in_ports,
                         placed->n_in_ports == 1 ? "" : "s",
                         s->kind == STATION_COMPARATOR
                             ? ", plus the threshold" : "");
                die_load(d->path, in->line, s->name, message);
            }
            /*
             * A starting depth, if the line gave one, before anything
             * else touches the port — it sizes the slots, and sizing
             * them after a value has been put in them would be a
             * reallocation nobody asked for (issue 210b).
             */
            if (in->depth > 0)
                map_in_port_start_depth(m, index, in->port, in->depth);

            /*
             * A bare dash: this port has no source yet. Nothing is
             * written into it and nothing is invented for it — the
             * station simply never becomes ready, which is an
             * ordinary state rather than a fault. It is what lets a
             * half-built program be a real program (issue 210b).
             */
            if (in->is_none) {
                map_in_port_convert(m, index, in->port, IN_PORT_NONE);
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
            map_in_port_static_text(m, index, in->port, text);
        }
    }
    names->count = index;
}
/* }}} */

/* {{{ type_check_wire() */
/*
 * The wire check (issue 603), at the first moment both ends are
 * known.
 *
 * **Widths, not names** (issue 309). It compared the two type name
 * strings until then, which meant a box producing four integers
 * called `vec4` could not feed a box taking four integers called
 * `stats` even though the bytes are indistinguishable and delivery
 * would copy them correctly — the author's only options were to
 * rename one or to write a box that took one and returned the other
 * and did nothing.
 *
 * The names still ride along, in the message and nowhere else,
 * because "4 bytes against 4 bytes" is not a sentence anybody can act
 * on. Both widths are reported beside them, because "box returns
 * vec4, port takes stats" does not say *why* those disagree and
 * "16 bytes against 12" does.
 *
 * What this widens rather than closes: two types of the same width
 * and different layouts now wire without complaint. That is the
 * accepted cost, and it is stated as a non-guarantee in 058.
 */
static void type_check_wire(map_description_t *d, int line,
                            const char *from_name, const box_info_t *from_box,
                            const char *to_name, int to_port,
                            const char *to_type, int to_size)
{
    if (from_box->return_size != to_size) {
        fprintf(stderr,
                "map %s:%d: %s -> %s.%d: box returns %s (%d bytes), "
                "port takes %s (%d bytes)\n",
                d->path, line, from_name, to_name, to_port,
                from_box->return_type, from_box->return_size,
                to_type ? to_type : "?", to_size);
        abort();
    }
}
/* }}} */
/* }}} */

/* {{{ second_pass() */
/*
 * Resolve every arrow (issue 603). By now every station exists and
 * can be found by name, which is the whole reason for a second pass:
 * an arrow names its destination, and a file may draw an arrow to a
 * station it has not declared yet.
 */
static void second_pass(map_t *m, map_description_t *d, name_table_t *names)
{
    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        const box_info_t *from_box = registry_find(s->box);

        for (desc_output_t *out = s->outputs; out; out = out->next) {
            int dest = find_station_index(names, out->dest_station);
            if (dest < 0) {
                char message[256];
                snprintf(message, sizeof message,
                         "arrow to '%s', which does not exist",
                         out->dest_station);
                die_load(d->path, out->line, s->name, message);
            }
            station_t *dest_station = map_station(m, dest);
            if (out->dest_port < 0 || out->dest_port >= dest_station->n_in_ports) {
                char message[256];
                snprintf(message, sizeof message,
                         "arrow to '%s.%d', but that station has %d port%s",
                         out->dest_station, out->dest_port,
                         dest_station->n_in_ports,
                         dest_station->n_in_ports == 1 ? "" : "s");
                die_load(d->path, out->line, s->name, message);
            }
            type_check_wire(d, out->line, s->name, from_box,
                            out->dest_station, out->dest_port,
                            dest_station->in_ports[out->dest_port].type_name,
                            dest_station->in_ports[out->dest_port].elem_size);
            map_connect(m, index, out->port, dest, out->dest_port);
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
    name_table_t names;
    names.by_index = calloc((size_t)d->n_stations, sizeof *names.by_index);
    if (!names.by_index)
        die_load(path, 0, NULL, "out of memory for the name table");
    names.count = 0;

    first_pass(m, d, &names);
    double t2 = stamp();
    second_pass(m, d, &names);
    double t3 = stamp();

    /*
     * The names move onto the map **before** the program is brought
     * up, because that is what the bring-up complains with. They used
     * to be copied over at the very end, when the loader's own table
     * had served — which was fine while the loader printed its own
     * messages out of that table, and stops being fine the moment
     * somebody else does the complaining.
     */
    for (int i = 0; i < m->n_stations; i++) {
        const char *no = map_name_station(m, i, names.by_index[i]->name);
        if (no)
            die_load(path, 0, NULL, no);
    }
    double t4 = stamp();

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
    if (map_seed_count(m) == 0)
        die_load(path, 0, NULL,
                 "nothing to seed — every station waits for a buffered "
                 "value, so the map cannot ever start");
    double t5 = stamp();

    map_load_last_timing.parse = t1 - t0;
    map_load_last_timing.first_pass = t2 - t1;
    map_load_last_timing.second_pass = t3 - t2;
    map_load_last_timing.validation = t4 - t3;
    map_load_last_timing.seed = t5 - t4;

    /* The loader's lookup table has served; the names it carried are
     * already on the map, where the dump and anyone watching find
     * them (issue 703). */
    free(names.by_index);
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
