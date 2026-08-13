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
 * the mechanism. The depth itself is SLOT_DEFAULT_CAPACITY, declared
 * beside the record in the header because the port's contract is
 * where a reader looks for it — and because issue 210b gave a port a
 * way to ask for a different one, which means two places now have to
 * agree on what "unless somebody says otherwise" is.
 */

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

/* {{{ cell_stride() */
/*
 * How many bytes one cell occupies: its value, then its state, then
 * enough padding that the next cell's value is aligned too.
 *
 * The alignment is inferred rather than known, and the inference is
 * the only subtle line in this file. The registry carries every
 * type's *size* and no type's *alignment* — nothing has needed the
 * latter before, because a plain array of values strided by their own
 * size is aligned for free. Adding a state byte per cell breaks that
 * for the first time.
 *
 * What rescues it is a rule the C standard guarantees: a type's
 * alignment always divides its size. So the largest power of two
 * dividing elem_size is *at least* the alignment the type wants, and
 * rounding the stride up to it is safe without knowing what the type
 * actually is. Sixteen is the ceiling because no ordinary C type
 * needs more than max_align_t, and rounding past it would only waste
 * memory.
 *
 * The cost is worth stating plainly. A port of four-byte integers
 * goes from four bytes per cell to eight — the state needs a byte and
 * the alignment rounds it to four. A port of two-hundred-byte structs
 * goes from two hundred to two hundred and eight. So the overhead is
 * large in proportion exactly where it is small in absolute terms,
 * and negligible where the values are big, which is the case this
 * whole line of work is about.
 */
static int cell_stride(int elem_size)
{
    int align = 1;
    while (align < 16 && elem_size % (align * 2) == 0)
        align *= 2;
    int total = elem_size + (int)sizeof(_Atomic unsigned char);
    return (total + align - 1) / align * align;
}
/* }}} */

/* {{{ slot_cell() */
void *slot_cell(const slot_t *sl, int index)
{
    return (unsigned char *)sl->storage + (size_t)index * (size_t)sl->stride;
}
/* }}} */

/* {{{ slot_cell_move() */
int slot_cell_move(const slot_t *sl, int index, int from, int to)
{
    /* The state sits immediately after the value bytes. Reached
     * through a byte pointer and an explicit offset rather than a
     * struct member, because a cell's size is not known until the
     * port exists — the value in the middle of it is as wide as the
     * parameter this port feeds. */
    _Atomic unsigned char *state = (_Atomic unsigned char *)
        ((unsigned char *)slot_cell(sl, index) + sl->elem_size);

    unsigned char expected = (unsigned char)from;
    /* Acquire-release on success: a reader that wins ready-to-claimed
     * must see every byte the writer copied before it published, and
     * a writer that wins claimed-to-empty must not have its next copy
     * hoisted above the release. Acquire on failure, because a caller
     * that lost still read the state and will decide what to do from
     * it. */
    return atomic_compare_exchange_strong_explicit(
        state, &expected, (unsigned char)to,
        memory_order_acq_rel, memory_order_acquire);
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
         * map format also assumes (issue 601). Becoming a static, or
         * having its source taken away, is a conversion applied
         * afterwards; neither one frees what is allocated here
         * (issue 210b). */
        sl->kind = SLOT_RING;
        sl->elem_size = elem_sizes[i];
        sl->capacity = SLOT_DEFAULT_CAPACITY;
        sl->stride = cell_stride(sl->elem_size);
        /* Zeroed rather than merely allocated, because a cell's state
         * is part of it now and empty is zero (issue 210c) — a fresh
         * run of cells has to be a fresh run of *empty* cells, or the
         * first reader to look would find whatever the allocator left
         * behind and believe it. */
        sl->storage = calloc((size_t)sl->capacity, (size_t)sl->stride);
        if (!sl->storage) fail("out of memory for a ring buffer");
        sl->read_hint = 0;
        sl->write_hint = 0;
        sl->held = 0;

        /* The other storage, allocated at the same moment and for the
         * same reason (issues 210b, 401): a port has room for both a
         * buffer and a constant whatever it is currently for, so
         * changing which one is in effect is a field write and never
         * an allocation. Zeroed, so a port whose constant has not been
         * set holds zeroes rather than whatever was there — though
         * nothing reads it until constant_set says somebody wrote it. */
        sl->constant = calloc(1, (size_t)sl->elem_size);
        if (!sl->constant) fail("out of memory for a port's constant");
        sl->constant_string = NULL;
        sl->constant_set = 0;
    }
}
/* }}} */

/* {{{ map_slot_start_depth() */
void map_slot_start_depth(map_t *m, int station, int slot, int cells)
{
    if (station < 0 || station >= m->n_stations)
        fail("setting the starting depth of a port on a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        fail("setting the starting depth of a port the box does not have");
    /* One cell is a legitimate depth. It used to take two, because a
     * spare was held back so that head meeting tail could mean empty
     * rather than full; a cell that carries its own state needs no
     * such stand-in, and every cell is usable (issue 210c). */
    if (cells < 1)
        fail("a ring buffer needs at least one cell");

    slot_t *sl = &s->slots[slot];
    if (sl->held != 0)
        fail("setting the starting depth of a port that already holds values "
             "— this is a starting depth, and the start has been and gone");

    /* Allocate before freeing, so a failure here leaves the port with
     * the buffer it already had rather than with none. Growth is what
     * covers a depth that turns out wrong, so there is nothing to
     * copy: the port is empty, which is what the check above proved. */
    void *fresh = calloc((size_t)cells, (size_t)sl->stride);
    if (!fresh) fail("out of memory resizing a ring buffer to its starting depth");
    free(sl->storage);
    sl->storage = fresh;
    sl->capacity = cells;
    sl->read_hint = 0;
    sl->write_hint = 0;
}
/* }}} */

/* {{{ map_slot_convert() */
void map_slot_convert(map_t *m, int station, int slot, int kind)
{
    if (station < 0 || station >= m->n_stations)
        fail("converting a port on a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        fail("converting a port the box does not have");
    if (kind < 0 || kind >= SLOT_KIND_COUNT)
        fail("converting a port to a kind that does not exist");

    /* Becoming a static again is legitimate and is why the constant
     * survives being converted away (issue 210f): a port that goes
     * static, buffer, static reads the value it read before. Becoming
     * one for the first time is not, because the tag would be in
     * effect over storage nobody has written — and that is a
     * different thing from *none*, which is honest about having no
     * source at all. */
    if (kind == SLOT_STATIC && !s->slots[slot].constant_set)
        fail("this port has never held a constant, so there is no value for "
             "it to go back to — give it one as text first");

    /* Under the station's mutex, as one of the four rare structural
     * operations (issue 210), so no readiness walk sees a port
     * mid-change. Nothing is freed and nothing is cleared: the whole
     * of the change is the tag, which is the entire point — see the
     * header for why the storage staying put is what makes this
     * cheap and what makes it lossless. */
    pthread_mutex_lock(&s->mutex);
    s->slots[slot].kind = (unsigned char)kind;
    pthread_mutex_unlock(&s->mutex);
}
/* }}} */

/* {{{ slot_kind_name() */
const char *slot_kind_name(unsigned char kind)
{
    static const char *const names[SLOT_KIND_COUNT] = {
        [SLOT_RING]   = "a buffer",
        [SLOT_STATIC] = "a static value",
        [SLOT_NONE]   = "a port with no source yet",
    };
    return kind < SLOT_KIND_COUNT ? names[kind] : "a port of an unknown kind";
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

    /* A process-wide "active map" pointer used to be set here, so that
     * a box — which receives only values and has no handle to anything
     * — could reach the statics table's write call. **That pointer was
     * the singleton**: it is what made a process able to run only one
     * map. Issue 405 removed the reason for it by giving a static's
     * write a real address, a station and a port, reachable from
     * outside the graph where every other configuration change already
     * comes from. Nothing here holds process-wide state now, so two
     * maps can run side by side and not see each other. */
}
/* }}} */

/* {{{ map_slot_depth() */
int map_slot_depth(map_t *m, int station, int slot)
{
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        fail("asking the depth of a slot that does not exist");
    slot_t *sl = &s->slots[slot];

    /* A maintained count rather than index arithmetic (issue 210d):
     * with values claimed wherever they sit, the distance between two
     * indices stopped describing how many are waiting.
     *
     * A port that is not a buffer still answers, and answers honestly.
     * A static reports whatever its cells were carrying when it
     * stopped being a buffer, which is the truth — those values are
     * waiting, and will be served if it becomes a buffer again. */
    pthread_mutex_lock(&s->mutex);
    int depth = sl->held;
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
    if (m->station_names) {
        for (int i = 0; i < m->n_stations; i++)
            free(m->station_names[i]);
        free(m->station_names);
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_slots; j++) {
            free(s->slots[j].storage);
            /* Both storages, because a port carries both whatever it
             * was being used for (issue 401). */
            slot_constant_free(&s->slots[j]);
        }
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
