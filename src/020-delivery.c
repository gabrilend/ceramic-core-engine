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
 * copies and index arithmetic; task allocation happens after the lock
 * is dropped. Values are claimed (copied out) before the lock
 * releases, which is the entire reason two invocations of one station
 * can run at once without meeting.
 *
 * Built across issues 202–206; routing grew in phase 5 at the
 * dispatch row marked for it. A second row was marked for the pull
 * path and filled in phase 4, and issue 210 took it out again — see
 * docs/implementation-notes/056-no-pull-path.md for why. The shape
 * held up: adding that path was a row, and removing it was a row.
 */
#include "018-station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Timing exists only when SORA_STATS is compiled in (issue 702) —
 * a clock read per box on short boxes is real overhead, and a
 * measurement apparatus that cannot be removed is a tax. The macros
 * vanish entirely without the define.
 */
#ifdef SORA_STATS
#include <time.h>
static long stats_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}
#define STATS_MARK(var) long var = stats_now_ns()
#define STATS_CHARGE(counter, since) (counter) += stats_now_ns() - (since)
#else
#define STATS_MARK(var) do { } while (0)
#define STATS_CHARGE(counter, since) do { } while (0)
#endif

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
    /* Zeroed, so every cell past the ones carried over is empty
     * (issue 210c). The carried-over ones are published ready below. */
    unsigned char *fresh = calloc((size_t)new_capacity, (size_t)sl->stride);
    if (!fresh) {
        fprintf(stderr, "delivery: ring buffer growth to %d cells failed\n",
                new_capacity);
        abort();
    }

    /* Value by value rather than in one or two block copies. The old
     * storage's cells carry their states interleaved with their
     * bytes, and the states are not what should be carried across —
     * the fresh run is being *built* rather than moved, so each
     * surviving value is written into an empty cell and published as
     * ready, exactly as an ordinary arrival would be. Copying the
     * bytes wholesale would carry the old states with them, and a
     * cell that was mid-transition in the old array would arrive in
     * the new one claiming to be mid-transition with nobody in it.
     *
     * Nothing else can be happening during this: the station's mutex
     * is held for the whole of a growth, which is what makes it safe
     * to walk cells one at a time here. Issue 210e removes both the
     * copying and the need for that.
     */
    for (int i = 0; i < held; i++) {
        int from = (sl->head + i) % sl->capacity;
        unsigned char *src = (unsigned char *)slot_cell(sl, from);
        unsigned char *dst = fresh + (size_t)i * (size_t)sl->stride;
        memcpy(dst, src, (size_t)sl->elem_size);
        dst[sl->elem_size] = CELL_READY;
    }

    free(sl->storage);
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

    /* The state machine, running for real (issue 210c) — while the
     * station's mutex still covers the copy, which is deliberate.
     * Building the states first and removing the lock afterwards
     * means a bug here shows up as a refused transition rather than
     * as a torn value.
     *
     * Both transitions must win, because the mutex means nobody else
     * is touching this port at all. A refusal is therefore not a lost
     * race — there is no other racer — but a disagreement between the
     * indices and the states, which is an engine bug and is worth
     * stopping for rather than papering over. */
    if (!slot_cell_move(sl, sl->tail, CELL_EMPTY, CELL_RESERVED)) {
        fprintf(stderr, "delivery: writing into a cell that was not empty — "
                        "the tail index and the cell states disagree\n");
        abort();
    }
    memcpy(slot_cell(sl, sl->tail), value, (size_t)sl->elem_size);
    if (!slot_cell_move(sl, sl->tail, CELL_RESERVED, CELL_READY)) {
        fprintf(stderr, "delivery: publishing a cell this thread had "
                        "reserved, and somebody else had moved it\n");
        abort();
    }
    sl->tail = (sl->tail + 1) % sl->capacity;

    int held = slot_count_locked(sl);
    if (held > sl->high_water)
        sl->high_water = held;
}
/* }}} */

/* {{{ slot_pop_locked() */
static void slot_pop_locked(slot_t *sl, void *into)
{
    /* The mirror of the write: take the cell, copy out, release it.
     * The reader is finished with the cell the moment the copy lands
     * in the caller's buffer — the box does not run until a worker
     * picks the task up later, reading from the task and holding no
     * cell at all. That is why nothing that can die is ever inside
     * this window. */
    if (!slot_cell_move(sl, sl->head, CELL_READY, CELL_CLAIMED)) {
        fprintf(stderr, "delivery: claiming a cell that was not ready — "
                        "the head index and the cell states disagree\n");
        abort();
    }
    memcpy(into, slot_cell(sl, sl->head), (size_t)sl->elem_size);
    if (!slot_cell_move(sl, sl->head, CELL_CLAIMED, CELL_EMPTY)) {
        fprintf(stderr, "delivery: releasing a cell this thread had claimed, "
                        "and somebody else had moved it\n");
        abort();
    }
    sl->head = (sl->head + 1) % sl->capacity;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The readiness dispatch (issue 204). Two tables, indexed by the     */
/* slot's kind: "does it hold a value?" and "claim one". Another      */
/* slot kind is a new row in each, never a new branch in two          */
/* functions that must be kept in agreement.                          */
/* ------------------------------------------------------------------ */

/* {{{ filled: ring / static */
static int ring_filled(const slot_t *sl)
{
    return sl->head != sl->tail;
}

static int static_filled(const slot_t *sl)
{
    /* A static's value is simply always there (issue 401). */
    (void)sl;
    return 1;
}

static int none_filled(const slot_t *sl)
{
    /* Nobody has said where this port's value comes from, so there is
     * no value and there is no prospect of one (issue 210b). This is
     * the whole of what *none* does at run time: a station holding one
     * answers no to readiness forever, no matter what arrives at its
     * other ports, and therefore never runs. Not an error, and not a
     * value — a station waiting to be finished being built. */
    (void)sl;
    return 0;
}

static int (*const slot_filled[SLOT_KIND_COUNT])(const slot_t *) = {
    [SLOT_RING]   = ring_filled,
    [SLOT_STATIC] = static_filled,
    [SLOT_NONE]   = none_filled,
};
/* }}} */

/* {{{ claim: ring / static */
/*
 * Claiming happens in two moments. Ring values are popped here,
 * under the mutex, which is what makes them spoken-for. A static is
 * resolved later, during task construction, outside the mutex, so
 * the statics table's lock never nests inside a station's. The claim
 * table records that split: a null entry means "resolved at build
 * time".
 *
 * The split used to carry a second reason and a sharper one — a
 * gatherer ran user code, and user code under a station's lock would
 * hand the hot path to whoever wrote the slowest box. Nothing is
 * gathered now (issue 210), so what remains is only lock ordering.
 */
static void ring_claim(slot_t *sl, void *into)
{
    slot_pop_locked(sl, into);
}

static void none_claim(slot_t *sl, void *into)
{
    /* Unreachable, and saying so out loud is the point. The walk above
     * this one asks every port whether it is filled before it claims
     * from any of them, and an unconfigured port answers no — so
     * arriving here means the readiness check and the claim disagreed
     * about the same port, which is an engine bug rather than a
     * situation to handle (issue 210b). */
    (void)sl; (void)into;
    fprintf(stderr, "delivery: claimed from a port that has no source — the "
                    "readiness walk and the claim walk disagreed\n");
    abort();
}

static void (*const slot_claim_locked[SLOT_KIND_COUNT])(slot_t *, void *) = {
    [SLOT_RING]   = ring_claim,
    /* Still an absence, and still meaning "resolved later, outside the
     * mutex" — issue 210b wanted this to become a named function
     * saying so, and it cannot yet. What the function would say
     * changes under issue 401, which moves a static's claim *into*
     * this walk beside the ring pop; writing the honest version now
     * would mean writing it twice. The caller therefore still tests
     * the pointer, and this comment is the hole's label until then. */
    [SLOT_STATIC] = NULL,
    [SLOT_NONE]   = none_claim,
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
 * task rather than everyone aiming at that station. Static slots are
 * resolved here — outside the lock, on the assembling thread's own
 * time. Non-static since phase 6: the seed sweep builds its first
 * tasks through this same door.
 */
task_t *task_build(map_t *m, int station_index,
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
        case SLOT_STATIC:
            /* A locked copy from the statics table (issue 401) —
             * resolved here, outside the station's mutex, so the
             * table's own lock never nests inside a station's. */
            static_claim(m, sl, t->in[i]);
            break;
        case SLOT_NONE:
            /* A task exists, so something decided this station was
             * ready; readiness cannot say yes about an unconfigured
             * port. Both callers are guarded — delivery asks the
             * readiness walk, and the seed sweep skips a station with
             * any unconfigured port — so reaching here means one of
             * those guards was removed (issue 210b). */
            die("building a task for a station with a port that has no source",
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
int map_deliver_value(map_t *m, int station, int slot, const void *value)
{
    if (station < 0 || station >= m->n_stations)
        die("delivering to a station outside the table", station);
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die("delivering to a slot the station does not have", station);
    /* Two ways this is wrong, and they deserve different sentences: a
     * static already holds its value and has nowhere to queue one
     * (until issue 405, where an arrow into a static overwrites it),
     * and an unconfigured port is one nobody has finished wiring. */
    if (s->slots[slot].kind == SLOT_NONE)
        die("delivering into a port that has no source yet", station);
    if (s->slots[slot].kind != SLOT_RING)
        die("delivering into a slot that is not a buffer", station);

    /* The claim buffer lives on this thread's stack, sized for one
     * complete input set. It exists so the readiness check allocates
     * nothing while holding the mutex. */
    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];

    int port = 0;

    STATS_MARK(wait_start);
    pthread_mutex_lock(&s->mutex);
    STATS_CHARGE(s->mutex_wait_ns, wait_start);
    slot_write_locked(&s->slots[slot], value);
    int due = station_ready_and_claim_locked(s, claimed);
    if (due && s->kind == STATION_ITERATOR && s->n_ports > 0) {
        /* The one memory a station keeps, touched at the one moment
         * only one thread can be looking (issue 504): this task
         * takes the cursor's exit, the cursor moves on, and the
         * choice rides out inside the task. The box never sees it. */
        port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due)
        pool_push(m->pool, task_build(m, station, claimed, port));
    return due;
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
    /* The threshold rode along as the task's last input — claimed
     * like any other slot, never handed to the box (issue 502). The
     * comparison happens here, after the box returned, through the
     * type's own three-way compare (issue 503): the sign maps
     * straight onto the ports — less is 0, equal is 1, greater 2. */
    int sign = s->compare(t->out, t->in[t->n_in - 1]);
    return sign + 1;
}

static int route_iterator(station_t *s, task_t *t)
{
    (void)s;
    /* Chosen at enqueue time, under the station's mutex, and
     * recorded in the task (issue 504) — so two tasks assembled a
     * moment apart carry different exits no matter which finishes
     * first. Reading it here is the whole row. */
    return t->port;
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
 *
 * The destination list is snapshotted under the station's own mutex
 * before any delivering happens, because since phase 7 the list can
 * change while the program runs (issue 704) — a walker holding a
 * node another thread just freed is the alternative. The snapshot
 * costs a short copy; delivering happens outside the lock.
 */
void map_deliver(void *ctx, task_t *t)
{
    map_t *m = ctx;
    station_t *s = &m->stations[t->station];

    s->runs++;

    if (s->out_size == 0)
        return;

    int port_index = route_choose[s->kind](s, t);

    pthread_mutex_lock(&s->mutex);
    port_t *port = station_port(s, port_index);
    int count = 0;
    for (destination_t *d = port ? port->destinations : NULL; d; d = d->next)
        count++;
    destination_t snapshot[count > 0 ? count : 1];
    int i = 0;
    for (destination_t *d = port ? port->destinations : NULL; d; d = d->next)
        snapshot[i++] = *d;
    pthread_mutex_unlock(&s->mutex);

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (i = 0; i < count; i++)
        s->produced += map_deliver_value(m, snapshot[i].station,
                                         snapshot[i].slot, t->out);
}
/* }}} */
