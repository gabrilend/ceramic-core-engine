/*
 * 019-station.c — building and dismantling the station table.
 *
 * What this is: the structural half of the engine. It allocates the
 * flat array of stations, hangs slots and ports off them, and tears
 * it all down. Nothing in this file moves a value; motion lives in
 * the delivery file. Data structures here, dataflow there — an error
 * in one is then findable without reading the other.
 *
 * How it does it, in general terms: one allocation for the table,
 * one per station's slot array, one per ring buffer, ports and
 * destinations as small linked nodes created as wires are declared.
 * Every cross-reference is an index, so nothing here ever needs
 * fixing up when storage grows elsewhere.
 */
#include "018-station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ring buffers start small on purpose: growth is cheap, proven, and
 * worth seeing in the demo; a generous initial size would only hide
 * the mechanism. One spare cell distinguishes full from empty, so
 * the usable count is one less than this.
 */
#define SLOT_INITIAL_CAPACITY 4

/* {{{ fail() */
/*
 * Construction errors are author errors: the map being described is
 * wrong, and building on top of a wrong description helps nobody.
 * Say what, where, and stop.
 */
static void fail(const char *what)
{
    fprintf(stderr, "map construction: %s\n", what);
    abort();
}
/* }}} */

/* {{{ map_create() */
map_t *map_create(int n_stations)
{
    if (n_stations <= 0)
        fail("a map needs at least one station");

    map_t *m = calloc(1, sizeof *m);
    if (!m) fail("out of memory for the map");

    /* The one flat array, allocated once, never resized while the
     * program runs (issue 201). Stations are addressed by position
     * in it forever after. */
    m->stations = calloc((size_t)n_stations, sizeof *m->stations);
    if (!m->stations) fail("out of memory for the station table");
    m->n_stations = n_stations;

    pthread_mutex_init(&m->rewire_mutex, NULL);

    return m;
}
/* }}} */

/* {{{ map_place() */
void map_place(map_t *m, int station, task_call_t shim, int kind,
               int n_slots, const int *elem_sizes, int out_size)
{
    if (station < 0 || station >= m->n_stations)
        fail("placing a box at a station index outside the table");
    if (kind < 0 || kind >= STATION_KIND_COUNT)
        fail("placing a box of a kind that does not exist");
    if (n_slots < 0)
        fail("a station cannot have a negative number of slots");

    station_t *s = &m->stations[station];
    if (s->call)
        fail("placing a box at a station already occupied");

    pthread_mutex_init(&s->mutex, NULL);
    s->call = shim;
    s->kind = (unsigned char)kind;
    s->out_size = out_size;
    s->cursor = 0;

    s->n_slots = n_slots;
    s->slots = NULL;
    if (n_slots > 0) {
        s->slots = calloc((size_t)n_slots, sizeof *s->slots);
        if (!s->slots) fail("out of memory for a slot array");
    }

    for (int i = 0; i < n_slots; i++) {
        slot_t *sl = &s->slots[i];
        if (elem_sizes[i] <= 0)
            fail("a slot's element size must be positive");
        /* Every slot starts life as a ring buffer — the default the
         * map format also assumes (issue 601). Statics and gatherers
         * are conversions applied afterwards, in phase 4. */
        sl->kind = SLOT_RING;
        sl->elem_size = elem_sizes[i];
        sl->capacity = SLOT_INITIAL_CAPACITY;
        sl->storage = malloc((size_t)sl->capacity * (size_t)sl->elem_size);
        if (!sl->storage) fail("out of memory for a ring buffer");
        sl->head = 0;
        sl->tail = 0;
        sl->source = -1;
        sl->static_id = -1;
    }
}
/* }}} */

/* {{{ station_port() */
/*
 * The port at a given index, walking the list. Ports are few — one
 * for plain, three for a comparator — so the walk is cheaper than
 * any cleverness. Returns null when the port was never created,
 * which delivery reads as "discard".
 */
port_t *station_port(station_t *s, int index)
{
    port_t *p = s->ports;
    for (int i = 0; p && i < index; i++)
        p = p->next;
    return p;
}
/* }}} */

/* {{{ map_connect() */
void map_connect(map_t *m, int from_station, int port,
                 int to_station, int to_slot)
{
    if (from_station < 0 || from_station >= m->n_stations)
        fail("connecting from a station outside the table");
    if (to_station < 0 || to_station >= m->n_stations)
        fail("connecting to a station outside the table");

    station_t *from = &m->stations[from_station];
    station_t *to = &m->stations[to_station];
    if (!from->call || !to->call)
        fail("connecting a station that has no box placed yet — place, then connect");
    if (to_slot < 0 || to_slot >= to->n_slots)
        fail("connecting to a slot the destination box does not have");
    if (from->out_size == 0)
        fail("connecting from a sink — a box that returns nothing has no output to wire");
    if (port < 0)
        fail("a port index cannot be negative");

    /* How many exits a station may have is its kind's business
     * (issue 501): a plain box has exactly one; a comparator exactly
     * three, indexed less/equal/greater; an iterator as many as the
     * map draws. For the kinds where an index carries meaning,
     * wiring only some outcomes is legitimate — sending everything
     * below a threshold somewhere and discarding the rest — so
     * intermediate ports are created empty rather than refused. An
     * empty port discards, which is exactly what an unwired outcome
     * should do. A dispatch-by-kind table, not a chain. */
    static const int port_limit[STATION_KIND_COUNT] = {
        [STATION_PLAIN]      = 1,
        [STATION_COMPARATOR] = 3,
        [STATION_ITERATOR]   = 0,   /* zero meaning: no limit */
    };
    int limit = port_limit[from->kind];
    if (limit > 0 && port >= limit)
        fail("a port index beyond what this station kind can mean — a plain "
             "box has one exit, a comparator three");

    while (from->n_ports <= port) {
        port_t *fresh = calloc(1, sizeof *fresh);
        if (!fresh) fail("out of memory for a port");
        port_t **link = &from->ports;
        while (*link)
            link = &(*link)->next;
        *link = fresh;
        from->n_ports++;
    }
    port_t *p = station_port(from, port);

    destination_t *d = calloc(1, sizeof *d);
    if (!d) fail("out of memory for a destination");
    d->station = to_station;
    d->slot = to_slot;
    d->next = NULL;

    /* Append at the tail: fan-out delivers in the order the wires
     * were drawn, which the loader tests rely on being stable. */
    destination_t **link = &p->destinations;
    while (*link)
        link = &(*link)->next;
    *link = d;
}
/* }}} */

/* {{{ map_start() */
void map_start(map_t *m, int n_workers)
{
    if (m->pool)
        fail("the map was already started");
    /* Delivery rides the pool's finish hook: after a worker runs a
     * task, the map decides where its output goes. This is the whole
     * of the pool's knowledge of the engine — one function pointer. */
    m->pool = pool_create(n_workers, map_deliver, m);
    /* The started map becomes the process's active map, which is how
     * a box — which receives only values — can reach the statics
     * table's write call. One live map per process is the standing
     * assumption; the first-pass report weighs it. */
    sora_active_map = m;
}
/* }}} */

/* {{{ map_slot_depth() */
int map_slot_depth(map_t *m, int station, int slot)
{
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        fail("asking the depth of a slot that does not exist");
    slot_t *sl = &s->slots[slot];

    pthread_mutex_lock(&s->mutex);
    int depth;
    if (sl->tail >= sl->head)
        depth = sl->tail - sl->head;
    else
        depth = sl->tail + sl->capacity - sl->head;
    pthread_mutex_unlock(&s->mutex);
    return depth;
}
/* }}} */

/* {{{ map_destroy() */
/* Phase 7 joints, implemented in the observe module; declared here
 * narrowly so teardown can call them without the whole header. */
void map_observe_stop(map_t *m);
void map_report_shutdown(map_t *m);

void map_destroy(map_t *m)
{
    map_observe_stop(m);
    if (m->pool)
        pool_destroy(m->pool);
    map_report_shutdown(m);
    if (sora_active_map == m)
        sora_active_map = NULL;
    map_statics_free(m);
    if (m->station_names) {
        for (int i = 0; i < m->n_stations; i++)
            free(m->station_names[i]);
        free(m->station_names);
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_slots; j++)
            free(s->slots[j].storage);
        free(s->slots);
        port_t *p = s->ports;
        while (p) {
            destination_t *d = p->destinations;
            while (d) {
                destination_t *next = d->next;
                free(d);
                d = next;
            }
            port_t *next = p->next;
            free(p);
            p = next;
        }
        pthread_mutex_destroy(&s->mutex);
    }
    pthread_mutex_destroy(&m->rewire_mutex);
    free(m->stations);
    free(m);
}
/* }}} */
