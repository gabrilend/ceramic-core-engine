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
 * Create every station (issue 602): registry lookup, slot array with
 * ring buffers as the default, the comparator's extra slot, statics
 * bound from their entries. Gather inputs wait for the second pass —
 * their sources are names, and a name may belong to a station
 * declared further down.
 */
static void first_pass(map_t *m, map_description_t *d, name_table_t *names)
{
    /* The statics table first, so bindings can parse entries. */
    if (d->max_static_id >= 0) {
        map_statics_alloc(m, d->max_static_id + 1);
        for (desc_static_t *e = d->statics; e; e = e->next)
            map_static_set_text(m, e->id, e->text);
    }

    int index = 0;
    for (desc_station_t *s = d->stations; s; s = s->next, index++) {
        names->by_index[index] = s;

        const box_info_t *b = registry_find(s->box);
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
            station_t *placed = &m->stations[index];
            if (in->slot < 0 || in->slot >= placed->n_slots) {
                char message[256];
                snprintf(message, sizeof message,
                         "'in %d' names a slot that does not exist — '%s' has "
                         "%d slot%s (its parameters%s)",
                         in->slot, s->box, placed->n_slots,
                         placed->n_slots == 1 ? "" : "s",
                         s->kind == STATION_COMPARATOR
                             ? ", plus the threshold" : "");
                die_load(d->path, in->line, s->name, message);
            }
            if (in->is_static)
                map_slot_static(m, index, in->slot, in->static_id);
            /* gather lines resolve in the second pass */
        }
    }
    names->count = index;
}
/* }}} */

/* {{{ type_check_wire() */
/*
 * The wire check (issue 603), at the first moment both ends are
 * known. Type names, not sizes: "4 bytes versus 4 bytes" is not a
 * message. Typedef transparency is inherited from the registry —
 * two names for int would already read "int" there — and
 * distinguishing wrapped types any further is deliberately not done.
 */
static void type_check_wire(map_description_t *d, int line,
                            const char *from_name, const box_info_t *from_box,
                            const char *to_name, int to_slot,
                            const char *to_type)
{
    if (strcmp(from_box->return_type, to_type) != 0) {
        fprintf(stderr,
                "map %s:%d: %s -> %s.%d: box returns %s, slot takes %s\n",
                d->path, line, from_name, to_name, to_slot,
                from_box->return_type, to_type);
        abort();
    }
}
/* }}} */

/* {{{ second_pass() */
/*
 * Resolve every arrow and every gather source (issue 603). By now
 * every station exists and can be found by name.
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
            station_t *dest_station = &m->stations[dest];
            if (out->dest_slot < 0 || out->dest_slot >= dest_station->n_slots) {
                char message[256];
                snprintf(message, sizeof message,
                         "arrow to '%s.%d', but that station has %d slot%s",
                         out->dest_station, out->dest_slot,
                         dest_station->n_slots,
                         dest_station->n_slots == 1 ? "" : "s");
                die_load(d->path, out->line, s->name, message);
            }
            type_check_wire(d, out->line, s->name, from_box,
                            out->dest_station, out->dest_slot,
                            dest_station->slots[out->dest_slot].type_name);
            map_connect(m, index, out->port, dest, out->dest_slot);
        }

        for (desc_input_t *in = s->inputs; in; in = in->next) {
            if (in->is_static)
                continue;
            int source = find_station_index(names, in->gather_source);
            if (source < 0) {
                char message[256];
                snprintf(message, sizeof message,
                         "gathers from '%s', which does not exist",
                         in->gather_source);
                die_load(d->path, in->line, s->name, message);
            }
            const box_info_t *source_box =
                registry_find(names->by_index[source]->box);
            const char *slot_type = m->stations[index].slots[in->slot].type_name;
            if (strcmp(source_box->return_type, slot_type) != 0) {
                fprintf(stderr,
                        "map %s:%d: %s gathers %s.%d: box returns %s, slot "
                        "takes %s\n",
                        d->path, in->line, s->name, in->gather_source,
                        in->slot, source_box->return_type, slot_type);
                abort();
            }
            /* The cycle check runs inside, at the moment the edge is
             * drawn (issue 404). */
            map_slot_gather(m, index, in->slot, source);
        }
    }
}
/* }}} */

/* {{{ whole_map_validation() */
/*
 * The checks only the finished map can answer (issue 604). Failures
 * are collected and printed together; one abort at the end. Gather
 * cycles are absent from this list only because they were refused
 * edge by edge as the wires were drawn — the same rule, applied
 * earlier, which the issue records.
 */
static void whole_map_validation(map_t *m, map_description_t *d,
                                 name_table_t *names)
{
    int failures = 0;

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        const char *name = names->by_index[i]->name;

        int has_ring = 0;
        for (int j = 0; j < s->n_slots; j++)
            if (s->slots[j].kind == SLOT_RING)
                has_ring = 1;

        /* Is anyone pushing into this station, and is anyone pulling
         * from it? Both at once is incoherent (mixed fan-out, seen
         * from the destination side); pulled-while-buffered can
         * never complete a gather. Sized to the station's real slot
         * count — a fixed cap here would be a silent hole in the
         * checking. */
        int pushed_into[s->n_slots > 0 ? s->n_slots : 1];
        memset(pushed_into, 0, sizeof pushed_into);
        int is_pulled = 0;
        for (int k = 0; k < m->n_stations; k++) {
            station_t *other = &m->stations[k];
            for (port_t *p = other->ports; p; p = p->next)
                for (destination_t *dst = p->destinations; dst; dst = dst->next)
                    if (dst->station == i && dst->slot < s->n_slots)
                        pushed_into[dst->slot] = 1;
            for (int j = 0; j < other->n_slots; j++)
                if (other->slots[j].kind == SLOT_GATHER
                    && other->slots[j].source == i)
                    is_pulled = 1;
        }
        int any_push = 0;
        for (int j = 0; j < s->n_slots; j++)
            any_push |= pushed_into[j];

        if (is_pulled && has_ring) {
            fprintf(stderr,
                    "map %s: station '%s' is gathered from, but it has "
                    "ring-buffer inputs — gathering runs inline and cannot "
                    "wait for a value to arrive\n",
                    d->path, name);
            failures++;
        }
        if (is_pulled && any_push) {
            fprintf(stderr,
                    "map %s: station '%s' is both written into by arrows and "
                    "gathered from — neither pushed nor pulled coherently\n",
                    d->path, name);
            failures++;
        }
        /* An arrow landing on a slot that is not a buffer would have
         * nowhere to put its value. */
        for (int j = 0; j < s->n_slots; j++) {
            if (pushed_into[j] && s->slots[j].kind != SLOT_RING) {
                fprintf(stderr,
                        "map %s: an arrow lands on '%s.%d', but that slot is "
                        "%s, not a buffer — the value would have nowhere to "
                        "go\n",
                        d->path, name, j,
                        s->slots[j].kind == SLOT_STATIC ? "static" : "gathered");
                failures++;
            }
        }

        /* Unreachable: ring inputs that nothing ever writes to. Loud
         * but not fatal — a map under construction has these, and
         * silently-never-running is the hardest thing to notice from
         * outside (issue 604). */
        if (has_ring && !any_push && !is_pulled) {
            int externally_fed = 0;
            /* The seed only reaches bufferless stations, so a ring
             * station with no arrows can only be fed by a test or a
             * control surface delivering from outside. Possible, so
             * this stays a warning. */
            (void)externally_fed;
            fprintf(stderr,
                    "map %s: WARNING: station '%s' has buffered inputs that "
                    "no arrow feeds — unless something outside delivers into "
                    "it, it will never run\n",
                    d->path, name);
        }
    }

    if (failures > 0) {
        fprintf(stderr, "map %s: %d validation failure%s — nothing was run\n",
                d->path, failures, failures == 1 ? "" : "s");
        abort();
    }
}
/* }}} */

/* {{{ seed_sweep() */
/*
 * The one time anything iterates the station table looking for work
 * (issue 605). From here on, every station is reached by index,
 * through a wire — the engine never scans, and this is the single
 * exception, which is why the sweep announces itself.
 *
 * Enqueue every station that has no ring-buffer inputs and is not
 * gathered from. The second half excludes gatherers: their values go
 * into tasks being assembled, not into buffers, and at startup
 * nobody is assembling. Sinks with no inputs qualify — they run once
 * for their effect.
 */
static void seed_sweep(map_t *m, map_description_t *d, name_table_t *names)
{
    m->seeded = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];

        int has_ring = 0;
        for (int j = 0; j < s->n_slots; j++)
            if (s->slots[j].kind == SLOT_RING)
                has_ring = 1;
        if (has_ring)
            continue;

        int is_pulled = 0;
        for (int k = 0; k < m->n_stations && !is_pulled; k++) {
            station_t *other = &m->stations[k];
            for (int j = 0; j < other->n_slots; j++)
                if (other->slots[j].kind == SLOT_GATHER
                    && other->slots[j].source == i)
                    is_pulled = 1;
        }
        if (is_pulled)
            continue;

        /* Through the same door delivery uses — one way a task comes
         * into existence, not two. No ring slots, so no claim
         * buffer. The workers are still parked at their gate, which
         * is what keeps the termination rule's "nothing pushes from
         * outside after startup" true. */
        pool_push(m->pool, task_build(m, i, NULL, 0));
        m->seeded++;
        fprintf(stderr, "map %s: seeded '%s'\n", d->path, names->by_index[i]->name);
    }

    if (m->seeded == 0) {
        fprintf(stderr,
                "map %s: nothing to seed — every station either waits for a "
                "buffered value or is gathered on demand, so the map cannot "
                "ever start\n", d->path);
        abort();
    }
}
/* }}} */

/* {{{ map_load_file() */
map_t *map_load_file(const char *path, int n_workers)
{
    double t0 = stamp();
    map_description_t *d = mapfile_parse(path);
    double t1 = stamp();

    map_t *m = map_create(d->n_stations);
    name_table_t names;
    names.by_index = calloc((size_t)d->n_stations, sizeof *names.by_index);
    if (!names.by_index)
        die_load(path, 0, NULL, "out of memory for the name table");
    names.count = 0;

    first_pass(m, d, &names);
    double t2 = stamp();
    second_pass(m, d, &names);
    double t3 = stamp();
    whole_map_validation(m, d, &names);
    double t4 = stamp();

    /* The pool exists before the seed so the seed has somewhere to
     * push, but its workers stay parked until the caller releases —
     * the seeding window issue 102 built. */
    map_start(m, n_workers);
    seed_sweep(m, d, &names);
    double t5 = stamp();

    map_load_last_timing.parse = t1 - t0;
    map_load_last_timing.first_pass = t2 - t1;
    map_load_last_timing.second_pass = t3 - t2;
    map_load_last_timing.validation = t4 - t3;
    map_load_last_timing.seed = t5 - t4;

    /* The loader's lookup table has served — but the names live on,
     * on the map, for the dump and for anyone watching (issue 703). */
    m->station_names = calloc((size_t)m->n_stations, sizeof *m->station_names);
    if (m->station_names)
        for (int i = 0; i < m->n_stations; i++)
            m->station_names[i] = strdup(names.by_index[i]->name);

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
