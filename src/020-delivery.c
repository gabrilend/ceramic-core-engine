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

/* {{{ slot_scan() */
/*
 * The scan (issue 210d). Start where the hint says, sweep forward,
 * wrap, stop where you started; take the first cell that will move
 * from `from` to `to`, and leave the hint pointing just past it.
 * Returns the cell's index, or -1 for a sweep that found nothing.
 *
 * **The hint is read once**, and that is what bounds the work: the
 * sweep visits at most every cell the port has, exactly one time
 * each, and then gives up. Re-reading a hint that other workers keep
 * pushing forward would let a reader chase it, and a scan that can be
 * outrun is a scan with no bound.
 *
 * **Other workers move the hint while a sweep is in progress, and
 * that is fine.** It is a hint, so a sweeper that started from a value
 * now stale is not wrong, only slightly less lucky — it pays a few
 * extra cells of walking. Nothing about correctness rests on the
 * number being current; what rests on it is only how quickly a worker
 * finds work.
 *
 * The write to the hint is a plain one and races with other writes to
 * it. The worst outcome of losing that race is a hint pointing
 * somewhere unhelpful, which costs a longer sweep next time. Making it
 * atomic would buy nothing, because there is no value it could hold
 * that would be wrong.
 *
 * One function serves both directions because a reader looking for a
 * ready cell and a writer looking for an empty one are the same
 * search with different names, and the ways they differ — which
 * transition, which hint — are arguments rather than logic.
 */
static int slot_scan(slot_t *sl, int *hint, int from, int to)
{
    int start = *hint;
    if (start < 0 || start >= sl->capacity)
        start = 0;

    for (int i = 0; i < sl->capacity; i++) {
        int c = start + i;
        if (c >= sl->capacity)
            c -= sl->capacity;
        if (slot_cell_move(sl, c, from, to)) {
            int next = c + 1;
            *hint = next >= sl->capacity ? 0 : next;
            return c;
        }
    }
    return -1;
}
/* }}} */

/* {{{ slot_grow_locked() */
/*
 * Double the cells (issue 203). Only the storage the slot points at
 * is reallocated — never the slot, never the station — so every wire
 * and every in-flight value is untouched.
 */
static void slot_grow_locked(slot_t *sl)
{
    int new_capacity = sl->capacity * 2;
    /* Zeroed, so every cell past the ones carried over is empty
     * (issue 210c). The carried-over ones are published ready below. */
    unsigned char *fresh = calloc((size_t)new_capacity, (size_t)sl->stride);
    if (!fresh) {
        fprintf(stderr, "delivery: ring buffer growth to %d cells failed\n",
                new_capacity);
        abort();
    }

    /* Value by value rather than in one or two block copies, and each
     * one taken out of the old array before it is put into the new.
     *
     * The old cells carry their states interleaved with their bytes,
     * and the states are not what should be carried across — the
     * fresh run is being *built* rather than moved, so each surviving
     * value is written into an empty cell and published as ready,
     * exactly as an ordinary arrival would be. Copying the bytes
     * wholesale would bring the old states with them.
     *
     * Claiming each cell on the way out is what identifies which ones
     * held a value: only a ready cell will move, so the walk finds
     * every value and nothing else. It also means the old array is
     * left correctly emptied rather than merely abandoned, which
     * costs nothing here and would be a real bug if this ever ran
     * with anything else looking.
     *
     * Nothing else can be happening during this: the station's mutex
     * is held for the whole of a growth. Issue 210e removes both the
     * copying and the need for that.
     */
    int placed = 0;
    for (int c = 0; c < sl->capacity; c++) {
        if (!slot_cell_move(sl, c, CELL_READY, CELL_CLAIMED))
            continue;
        unsigned char *dst = fresh + (size_t)placed * (size_t)sl->stride;
        memcpy(dst, slot_cell(sl, c), (size_t)sl->elem_size);
        dst[sl->elem_size] = CELL_READY;
        placed++;
    }
    if (placed != sl->held) {
        fprintf(stderr, "delivery: growth found %d values in a port holding "
                        "%d — the count and the cells disagree\n",
                placed, (int)sl->held);
        abort();
    }

    free(sl->storage);
    sl->storage = fresh;
    sl->capacity = new_capacity;
    /* Values sit at the front of the fresh run and space follows
     * them, so a reader should start at the front and a writer just
     * past the values. Both are only hints; being wrong would cost a
     * sweep, not a mistake. */
    sl->read_hint = 0;
    sl->write_hint = placed;
    sl->growths++;
}
/* }}} */

/* {{{ slot_write_locked() */
static void slot_write_locked(slot_t *sl, const void *value)
{
    /* Look for somewhere to put it, and grow only if there is
     * genuinely nowhere (issue 210d). This used to grow when the tail
     * was one short of the head — a test on indices, which had to
     * leave a cell spare so that the two meeting could mean empty
     * rather than full. Asking the cells directly needs no spare and
     * no arithmetic: a full buffer is one where nothing answers.
     */
    int c = slot_scan(sl, &sl->write_hint, CELL_EMPTY, CELL_RESERVED);
    if (c < 0) {
        slot_grow_locked(sl);
        c = slot_scan(sl, &sl->write_hint, CELL_EMPTY, CELL_RESERVED);
        if (c < 0) {
            fprintf(stderr, "delivery: a port with no free cell immediately "
                            "after growing to %d cells\n", sl->capacity);
            abort();
        }
    }

    /* The state machine, running for real (issue 210c) — while the
     * station's mutex still covers the copy, which is deliberate.
     * Building the states first and removing the lock afterwards
     * means a bug here shows up as a refused transition rather than
     * as a torn value.
     *
     * The scan already won this cell, so publishing it cannot fail
     * for any reason but somebody else having touched a cell that was
     * this thread's alone — an engine bug, and worth stopping for
     * rather than papering over. */
    memcpy(slot_cell(sl, c), value, (size_t)sl->elem_size);
    if (!slot_cell_move(sl, c, CELL_RESERVED, CELL_READY)) {
        fprintf(stderr, "delivery: publishing a cell this thread had "
                        "reserved, and somebody else had moved it\n");
        abort();
    }

    int held = ++sl->held;
    if (held > sl->high_water)
        sl->high_water = held;
}
/* }}} */

/* {{{ slot_pop_locked() */
/*
 * The mirror of the write: find a ready cell, copy out, release it.
 * Returns whether it got one — which under the mutex it always will,
 * because the readiness walk asked first, but which becomes a real
 * answer the moment the lock comes off and the claim walk has to be
 * able to roll back.
 *
 * The reader is finished with the cell the moment the copy lands in
 * the caller's buffer — the box does not run until a worker picks the
 * task up later, reading from the task and holding no cell at all.
 * That is why nothing that can die is ever inside this window.
 */
static int slot_pop_locked(slot_t *sl, void *into)
{
    int c = slot_scan(sl, &sl->read_hint, CELL_READY, CELL_CLAIMED);
    if (c < 0)
        return 0;

    memcpy(into, slot_cell(sl, c), (size_t)sl->elem_size);
    if (!slot_cell_move(sl, c, CELL_CLAIMED, CELL_EMPTY)) {
        fprintf(stderr, "delivery: releasing a cell this thread had claimed, "
                        "and somebody else had moved it\n");
        abort();
    }
    sl->held--;
    return 1;
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
    /* A maintained count rather than two indices differing. The
     * indices stopped being able to answer this when values began
     * being claimed wherever they sat rather than from a computed
     * position (issue 210d) — the distance between a head and a tail
     * describes a contiguous run, and there is no longer one. */
    return sl->held > 0;
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
    /* Under the mutex the readiness walk has already established that
     * this port holds something and nobody can have taken it since,
     * so a scan that comes back empty-handed means the count and the
     * cells disagree. That stops being an engine bug and becomes an
     * ordinary lost race when the lock comes off, at which point this
     * row grows the roll-back that issue 210d designs. */
    if (!slot_pop_locked(sl, into)) {
        fprintf(stderr, "delivery: a port that answered ready had no ready "
                        "cell when asked for one\n");
        abort();
    }
}

static void static_claim_locked(slot_t *sl, void *into)
{
    /* A copy, under the station's mutex, beside the ring pops (issue
     * 401). It used to be resolved later, during task construction and
     * outside this lock, and the reason was lock ordering: the value
     * lived in a table with a mutex of its own, and that mutex must
     * never nest inside a station's.
     *
     * With the value on the port there is no second lock to order, so
     * the split has nothing left to buy — and closing it gains
     * something real. The static half of an input set is now as
     * mutually consistent as the buffered half: every value a task
     * carries was taken in one window, under one lock, so a box
     * reading two statics can no longer get values that were correct
     * at two different moments and never together.
     *
     * Nothing is consumed. A static is always full, which is the whole
     * reason the kind exists. */
    memcpy(into, sl->constant, (size_t)sl->elem_size);
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

/*
 * **No row here is an absence any more** (issues 210b, 401). The
 * static row was a null, and the caller tested the function pointer
 * for truth to discover it; the meaning was "resolved later, outside
 * the mutex", which was a real and correct decision written as a hole
 * that a reader had to already know the meaning of. The decision it
 * encoded is gone with the statics table, so the hole is gone with it.
 */
static void (*const slot_claim_locked[SLOT_KIND_COUNT])(slot_t *, void *) = {
    [SLOT_RING]   = ring_claim,
    [SLOT_STATIC] = static_claim_locked,
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
        /* Every kind, unconditionally. The caller used to test the
         * function pointer here because the static row was null; there
         * is no null now, so the dispatch is a call rather than a call
         * guarded by a question about the table's own shape. */
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
    station_t *s = map_station(m, station_index);

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
    /* Zero rather than left over: with timing compiled out nothing
     * ever writes it, and the delivery walk adds it to the station
     * unconditionally. */
    t->box_ns = 0;

    /* The pointer array sits immediately after the struct; the value
     * bytes after it; the output after those. One free() takes the
     * whole thing back. */
    t->in = (void **)(t + 1);
    unsigned char *data = (unsigned char *)(t->in + s->n_slots);

    /*
     * Every value was claimed under the station's mutex before this
     * ran, whatever kind of port it came from (issue 401) — so this is
     * one copy per port out of the caller's buffer, and there is no
     * longer a case that reaches back into the map for a value it
     * failed to bring along.
     *
     * The claimed buffer may be null only for a station with no ports
     * at all, which the loop below then does not enter.
     */
    int offset = 0;
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        t->in[i] = data + offset;
        if (!claimed)
            die("building a task with no claimed values for a station that "
                "has ports", station_index);
        memcpy(t->in[i], claimed + offset, (size_t)sl->elem_size);
        offset += sl->elem_size;
    }
    (void)m;

    t->out = s->out_size > 0 ? data + in_bytes : NULL;
    return t;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Delivery itself (issues 204, 205).                                 */
/* ------------------------------------------------------------------ */

/* {{{ map_station_try_start() */
/*
 * Readiness, claim, build, push — a delivery with the delivering taken
 * out. Three callers wanted exactly this and were each doing their own
 * version of it (issue 401).
 *
 * The seed sweep enqueued a station without asking whether it was
 * ready, which was safe only while the unasked question happened to
 * have the same answer — and stopped being safe the moment a static's
 * value had to be claimed like any other, because the seed had no
 * claim buffer to put one in.
 *
 * Setting or writing a static is supposed to run the ordinary
 * readiness check on its station. That is what replaced the pull path,
 * and it is what makes a chain of stations wired through static ports
 * into a recalculation graph. It was documented as a guarantee and was
 * not actually happening.
 *
 * A write cannot make something run that could not run anyway, because
 * the check it triggers is this one: an empty ring port still answers
 * no, and the engine will not invent a value for it.
 */
int map_station_try_start(map_t *m, int station)
{
    if (station < 0 || station >= m->n_stations)
        die("starting a station outside the table", station);
    station_t *s = map_station(m, station);
    if (!s->call)
        die("starting a station with no box placed", station);
    /* Removed and not yet reclaimed: nothing new starts from it
     * (issue 216). */
    if (atomic_load_explicit(&s->removed, memory_order_acquire))
        return 0;

    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];
    int port = 0;

    pthread_mutex_lock(&s->mutex);
    int due = station_ready_and_claim_locked(s, claimed);
    if (due && s->kind == STATION_ITERATOR && s->n_ports > 0) {
        port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due)
        pool_push(m->pool, task_build(m, station, in_bytes > 0 ? claimed : NULL,
                                      port));
    return due;
}
/* }}} */

/* {{{ map_deliver_value() */
int map_deliver_value(map_t *m, int station, int slot, const void *value)
{
    if (station < 0 || station >= m->n_stations)
        die("delivering to a station outside the table", station);
    station_t *s = map_station(m, station);

    /*
     * The station may have been removed since this value set out
     * (issue 216). A worker reads a port's destinations once and then
     * visits them, so a removal can land between the read and the
     * visit — the wire it was following is gone, and so is what it
     * pointed at.
     *
     * **The value is discarded, which is what this engine already
     * does with a value that has nowhere to go.** A port wired
     * nowhere discards; an unwired comparator outcome discards. This
     * is the same shape: a value in flight toward something that is
     * no longer there. Removing a station is a deliberate act by
     * somebody who knew what was wired into it, and the alternative —
     * stopping the program — would make every removal a race against
     * whatever was already moving.
     */
    if (atomic_load_explicit(&s->removed, memory_order_acquire) || !s->call)
        return 0;

    if (slot < 0 || slot >= s->n_slots)
        die("delivering to a slot the station does not have", station);
    /* Two ways this is wrong, and they deserve different sentences: a
     * static already holds its value and has nowhere to queue one, and
     * an unconfigured port is one nobody has finished wiring.
     *
     * The first of those is not permanent. Issue 405 makes an arrow
     * into a static port *overwrite* the static rather than queue —
     * which is how a constant gets computed at startup instead of
     * written by hand, and is a property of the wire rather than of
     * the box, so it shows up in the map file instead of happening
     * invisibly inside C. The write call exists; teaching delivery to
     * use it belongs with the load-time check that currently refuses
     * such a wire. */
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
 * **The walk takes no lock and copies nothing** (issue 214). A port's
 * destinations are one immutable array; a rewire builds a whole new
 * one and swaps the pointer, so reading that pointer once yields
 * something nobody will ever modify. The old set is filed rather than
 * freed, so a walker already inside one is not walking freed memory.
 *
 * It used to snapshot the list onto this walker's stack under the
 * station's mutex, because a rewire could unlink and free a node
 * under a walker's feet. That cost a lock acquisition and a copy
 * proportional to fan-out **on every value the engine moved**, and it
 * was the last thing holding the station's mutex on the hot path.
 */
void map_deliver(void *ctx, task_t *t)
{
    map_t *m = ctx;
    station_t *s = map_station(m, t->station);

    s->runs++;
    /* The box's own time, charged onto the task by the shim and moved
     * onto the station here — the one place that holds both (issue
     * 405). Zero when timing is compiled out, so this costs an add of
     * nothing rather than a branch. */
    s->box_ns += t->box_ns;

    if (s->out_size == 0)
        return;

    int port_index = route_choose[s->kind](s, t);

    port_t *port = station_port(s, port_index);
    dest_set_t *set = port_dests(port);
    if (!set)
        return;

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (int i = 0; i < set->n; i++)
        s->produced += map_deliver_value(m, set->items[i].station,
                                         set->items[i].slot, t->out);
}
/* }}} */
