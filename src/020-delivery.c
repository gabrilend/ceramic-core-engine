/*
 * 020-delivery.c — how a value becomes the next thing that runs.
 *
 * What this is: the central path of the engine, and the moving half
 * of the station layer. A box function has just returned a value;
 * this file carries it into whoever was waiting, notices the moment
 * a station has everything it needs, and turns that moment into a
 * task in the pool. Nothing polls, nothing scans: the act of
 * finishing is the act of scheduling.
 *
 * How it does it, in general terms: each delivery takes exactly one
 * station's mutex, writes one value, and asks one question — is this
 * station now complete? The contended section is a handful of memory
 * copies and index arithmetic; task allocation, and later gathering,
 * happen after the lock is dropped. Values are claimed (copied out)
 * before the lock releases, which is the entire reason two
 * invocations of one station can run at once without meeting.
 *
 * Built across issues 202–206; routing grows in phase 5, the pull
 * path in phase 4, exactly at the dispatch rows marked for them.
 */
#include "018-station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ die() */
/* Dataflow errors are engine bugs or unbuilt phases; both stop the
 * program and say which station, because a wrong answer that keeps
 * flowing is worse than no answer. */
static void die(const char *what, int station)
{
    fprintf(stderr, "delivery: %s (station %d)\n", what, station);
    abort();
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Slot motion (issues 202, 203). Callers hold the station's mutex.   */
/* ------------------------------------------------------------------ */

/* {{{ slot_count_locked() */
static int slot_count_locked(const slot_t *sl)
{
    /* Two paths: tail ahead of head reads directly; tail wrapped
     * behind adds one lap. */
    if (sl->tail >= sl->head)
        return sl->tail - sl->head;
    return sl->tail + sl->capacity - sl->head;
}
/* }}} */

/* {{{ slot_grow_locked() */
/*
 * Double the cells (issue 203). Only the storage the slot points at
 * is reallocated — never the slot, never the station — so every wire
 * and every in-flight value is untouched. The wrapped portion is
 * copied so the contents read contiguously from cell zero again.
 */
static void slot_grow_locked(slot_t *sl)
{
    int held = slot_count_locked(sl);
    int new_capacity = sl->capacity * 2;
    unsigned char *fresh = malloc((size_t)new_capacity * (size_t)sl->elem_size);
    if (!fresh) {
        fprintf(stderr, "delivery: ring buffer growth to %d cells failed\n",
                new_capacity);
        abort();
    }

    unsigned char *old = sl->storage;
    if (sl->tail >= sl->head) {
        memcpy(fresh, old + (size_t)sl->head * (size_t)sl->elem_size,
               (size_t)held * (size_t)sl->elem_size);
    } else {
        int first_block = sl->capacity - sl->head;
        memcpy(fresh, old + (size_t)sl->head * (size_t)sl->elem_size,
               (size_t)first_block * (size_t)sl->elem_size);
        memcpy(fresh + (size_t)first_block * (size_t)sl->elem_size, old,
               (size_t)sl->tail * (size_t)sl->elem_size);
    }

    free(old);
    sl->storage = fresh;
    sl->capacity = new_capacity;
    sl->head = 0;
    sl->tail = held;
    sl->growths++;
}
/* }}} */

/* {{{ slot_write_locked() */
static void slot_write_locked(slot_t *sl, const void *value)
{
    /* An input buffer should never be full: grow before the tail can
     * land on the head (issue 203). */
    if ((sl->tail + 1) % sl->capacity == sl->head)
        slot_grow_locked(sl);

    memcpy((unsigned char *)sl->storage + (size_t)sl->tail * (size_t)sl->elem_size,
           value, (size_t)sl->elem_size);
    sl->tail = (sl->tail + 1) % sl->capacity;

    int held = slot_count_locked(sl);
    if (held > sl->high_water)
        sl->high_water = held;
}
/* }}} */

/* {{{ slot_pop_locked() */
static void slot_pop_locked(slot_t *sl, void *into)
{
    memcpy(into,
           (unsigned char *)sl->storage + (size_t)sl->head * (size_t)sl->elem_size,
           (size_t)sl->elem_size);
    sl->head = (sl->head + 1) % sl->capacity;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The readiness dispatch (issue 204). Two tables, indexed by the     */
/* slot's kind: "does it hold a value?" and "claim one". A fourth     */
/* slot kind is a new row in each, never a new branch in two          */
/* functions that must be kept in agreement.                          */
/* ------------------------------------------------------------------ */

/* {{{ filled: ring / gather / static */
static int ring_filled(const slot_t *sl)
{
    return sl->head != sl->tail;
}

static int gather_filled(const slot_t *sl)
{
    /* A gatherer's value is produced on demand, so the slot always
     * holds one, by definition. Real production arrives in phase 4
     * (issue 403); the readiness answer is already correct. */
    (void)sl;
    return 1;
}

static int static_filled(const slot_t *sl)
{
    /* A static's value is simply always there (issue 401). */
    (void)sl;
    return 1;
}

static int (*const slot_filled[SLOT_KIND_COUNT])(const slot_t *) = {
    [SLOT_RING]   = ring_filled,
    [SLOT_GATHER] = gather_filled,
    [SLOT_STATIC] = static_filled,
};
/* }}} */

/* {{{ claim: ring / gather / static */
/*
 * Claiming happens in two moments. Ring values are popped here,
 * under the mutex, which is what makes them spoken-for. Gathered and
 * static values are resolved later, during task construction,
 * outside the mutex — a gatherer runs user code, and user code under
 * a station's lock would hand the hot path to whoever wrote the
 * slowest box. The claim table records that split: a null entry
 * means "resolved at build time".
 */
static void ring_claim(slot_t *sl, void *into)
{
    slot_pop_locked(sl, into);
}

static void (*const slot_claim_locked[SLOT_KIND_COUNT])(slot_t *, void *) = {
    [SLOT_RING]   = ring_claim,
    [SLOT_GATHER] = NULL,
    [SLOT_STATIC] = NULL,
};
/* }}} */

/* {{{ station_input_bytes() */
/*
 * Total bytes of one complete input set. Element sizes never change
 * after construction, so this reads without the mutex.
 */
static int station_input_bytes(const station_t *s)
{
    int total = 0;
    for (int i = 0; i < s->n_slots; i++)
        total += s->slots[i].elem_size;
    return total;
}
/* }}} */

/* {{{ station_ready_and_claim_locked() */
/*
 * The one rule made real: walk every slot; if any is empty, nothing
 * happens and the value just written waits for its siblings. If all
 * are full, claim one value from each ring slot into the caller's
 * buffer — copied out and the head advanced, so no other thread can
 * claim the same ones. Returns whether a task became due.
 */
static int station_ready_and_claim_locked(station_t *s, unsigned char *claimed)
{
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        if (!slot_filled[sl->kind](sl))
            return 0;
    }

    int offset = 0;
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        if (slot_claim_locked[sl->kind])
            slot_claim_locked[sl->kind](sl, claimed + offset);
        offset += sl->elem_size;
    }
    return 1;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The task struct in motion (issue 206).                             */
/* ------------------------------------------------------------------ */

/* {{{ task_build() */
/*
 * One allocation, sized exactly for this box: the struct, the array
 * of input pointers, the input bytes, the output bytes. Runs after
 * the station's mutex is released, so a slow allocator delays one
 * task rather than everyone aiming at that station. This is also
 * where gathered and static slots will be resolved in phase 4 —
 * outside the lock, on the assembling thread's own time.
 */
static task_t *task_build(map_t *m, int station_index,
                          const unsigned char *claimed, int port)
{
    station_t *s = &m->stations[station_index];

    int in_bytes = station_input_bytes(s);
    size_t total = sizeof(task_t)
                 + (size_t)s->n_slots * sizeof(void *)
                 + (size_t)in_bytes
                 + (size_t)s->out_size;

    task_t *t = malloc(total);
    if (!t)
        die("out of memory building a task", station_index);

    t->call = s->call;
    t->station = station_index;
    t->port = port;
    t->n_in = s->n_slots;

    /* The pointer array sits immediately after the struct; the value
     * bytes after it; the output after those. One free() takes the
     * whole thing back. */
    t->in = (void **)(t + 1);
    unsigned char *data = (unsigned char *)(t->in + s->n_slots);

    int offset = 0;
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        t->in[i] = data + offset;
        switch (sl->kind) {
        case SLOT_RING:
            /* Claimed under the mutex; copied into the task here. */
            memcpy(t->in[i], claimed + offset, (size_t)sl->elem_size);
            break;
        case SLOT_GATHER:
        case SLOT_STATIC:
            /* Resolved at build time — phase 4 (issues 401, 403). */
            die("a slot kind from phase 4 was reached before phase 4 was built",
                station_index);
            break;
        default:
            die("a slot of an unknown kind", station_index);
        }
        offset += sl->elem_size;
    }

    t->out = s->out_size > 0 ? data + in_bytes : NULL;
    return t;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Delivery itself (issues 204, 205).                                 */
/* ------------------------------------------------------------------ */

/* {{{ map_deliver_value() */
void map_deliver_value(map_t *m, int station, int slot, const void *value)
{
    if (station < 0 || station >= m->n_stations)
        die("delivering to a station outside the table", station);
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die("delivering to a slot the station does not have", station);
    if (s->slots[slot].kind != SLOT_RING)
        die("delivering into a slot that is not a buffer", station);

    /* The claim buffer lives on this thread's stack, sized for one
     * complete input set. It exists so the readiness check allocates
     * nothing while holding the mutex. */
    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];

    int port = 0;

    pthread_mutex_lock(&s->mutex);
    slot_write_locked(&s->slots[slot], value);
    int due = station_ready_and_claim_locked(s, claimed);
    /* Phase 5: an iterator advances its cursor here, under the
     * mutex, and the chosen port rides out in the task (issue 504). */
    pthread_mutex_unlock(&s->mutex);

    if (due)
        pool_push(m->pool, task_build(m, station, claimed, port));
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Routing (issue 205; grows in phase 5).                             */
/* The kind is consulted at exactly this one moment on the way out    */
/* and nowhere else in the engine. A table, not a chain: a fourth     */
/* kind is a row.                                                     */
/* ------------------------------------------------------------------ */

/* {{{ route: plain / comparator / iterator */
static int route_plain(station_t *s, task_t *t)
{
    (void)s; (void)t;
    /* A plain box has one exit. */
    return 0;
}

static int route_comparator(station_t *s, task_t *t)
{
    (void)s; (void)t;
    /* Filled by issue 502. Failing loudly until then keeps a map
     * that uses the kind early from silently routing one way. */
    die("a comparator routed before phase 5 was built", t->station);
    return -1;
}

static int route_iterator(station_t *s, task_t *t)
{
    (void)s; (void)t;
    /* Filled by issue 504. */
    die("an iterator routed before phase 5 was built", t->station);
    return -1;
}

static int (*const route_choose[STATION_KIND_COUNT])(station_t *, task_t *) = {
    [STATION_PLAIN]      = route_plain,
    [STATION_COMPARATOR] = route_comparator,
    [STATION_ITERATOR]   = route_iterator,
};
/* }}} */

/* {{{ map_deliver() */
/*
 * The delivery walk: the pool's finish hook. Two paths at the top —
 * a sink's task is simply done (a box declared void needs no engine
 * support at all), and everything else chooses one port and delivers
 * its output to every destination on it. A port wired nowhere
 * discards, which is what an unwired comparator outcome wants.
 */
void map_deliver(void *ctx, task_t *t)
{
    map_t *m = ctx;
    station_t *s = &m->stations[t->station];

    if (s->out_size == 0)
        return;

    int port_index = route_choose[s->kind](s, t);
    port_t *port = station_port(s, port_index);
    if (!port)
        return;

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (destination_t *d = port->destinations; d; d = d->next)
        map_deliver_value(m, d->station, d->slot, t->out);
}
/* }}} */
