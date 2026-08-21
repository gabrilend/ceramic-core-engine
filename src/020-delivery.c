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
 * How it does it, in general terms: a delivery writes one value into
 * a port and then asks one question — is this station now complete?
 *
 * **Almost none of that is under a lock.** Writing takes no lock at
 * all: reserving a slot is one compare-and-swap and the copy that
 * follows goes into bytes the writer owns. The station's mutex covers
 * exactly one thing, the walk that checks every port for a ready slot
 * and then takes one from each — a scan and a state write per port,
 * with no bytes moving. The copies out, the task allocation and the
 * push all happen after it is dropped.
 *
 * What protects the values, then, is not exclusion but **ownership**:
 * a slot in *reserved* or *claimed* belongs to exactly one worker and
 * no other worker may touch it, which is written into the slot state
 * table itself. Bytes nobody else may touch need no lock around them.
 * That is the entire reason two invocations of one station can run at
 * once without meeting, and why several workers can copy out of one
 * station while another holds the lock doing its flips.
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
/* Port motion (issues 202, 203). Callers hold the station's mutex.   */
/* ------------------------------------------------------------------ */

/* {{{ in_port_scan() */
/*
 * The scan (issue 210d). Start where the hint says, sweep forward,
 * wrap, stop where you started; take the first slot that will move
 * from `from` to `to`, and leave the hint pointing just past it.
 * Returns the slot's index, or -1 for a sweep that found nothing.
 *
 * **The hint is read once**, and that is what bounds the work: the
 * sweep visits at most every slot the port has, exactly one time
 * each, and then gives up. Re-reading a hint that other workers keep
 * pushing forward would let a reader chase it, and a scan that can be
 * outrun is a scan with no bound.
 *
 * **Other workers move the hint while a sweep is in progress, and
 * that is fine.** It is a hint, so a sweeper that started from a value
 * now stale is not wrong, only slightly less lucky — it pays a few
 * extra slots of walking. Nothing about correctness rests on the
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
 * ready slot and a writer looking for an empty one are the same
 * search with different names, and the ways they differ — which
 * transition, which hint — are arguments rather than logic.
 */
static int in_port_scan(in_port_t *sl, int *hint, int from, int to,
                        void **found, int exclusive)
{
    /*
     * The capacity is read **once**, and that is what bounds the
     * sweep. Growth can raise it while this runs; a scanner that kept
     * re-reading a number another thread keeps raising could be made
     * to walk further every time it looked. Reading it once means the
     * worst case is a sweep that misses slots which appeared a moment
     * ago, and the next sweep finds them.
     */
    int cap = atomic_load_explicit(&sl->capacity, memory_order_acquire);
    int start = *hint;
    if (start < 0 || start >= cap)
        start = 0;

    /*
     * The pages are walked rather than indexed (issue 210e). Resolving
     * a slot's ordinal to a page costs a walk down the list, so doing
     * it per candidate would make a sweep quadratic in the number of
     * pages. It is done **once**, here, and the sweep then follows
     * `next` — one pointer hop per page boundary and plain pointer
     * arithmetic in between, which is what the old single array did
     * everywhere.
     */
    int page = start / sl->page_slots;
    int off  = start % sl->page_slots;
    in_port_page_t *pg = sl->pages;
    for (int i = 0; i < page && pg; i++)
        pg = pg->next;

    for (int i = 0; i < cap && pg; i++) {
        void *slot = pg->slots + (size_t)off * (size_t)sl->stride;
        /*
         * Two ways to ask, and which one is right is a property of the
         * caller rather than of the slot (issue 210d, step 6).
         *
         * A claimer runs under the station's mutex and is looking for
         * a *ready* slot. Nothing else can move one: other claimers
         * are excluded by the lock, and a writer only ever
         * compare-and-swaps from *empty*, which fails against a ready
         * slot without writing. So a load is enough, and only the slot
         * it actually takes is written to.
         *
         * A writer holds no lock and is looking for an *empty* slot,
         * where it genuinely races other writers. That one has to be a
         * compare-and-swap, because the loser must be told it lost.
         *
         * The difference matters because this question is asked of
         * every candidate walked past, not only of the one taken.
         */
        int got = exclusive
            ? (slot_state_at(slot, sl->elem_size) == from
               && (slot_set_at(slot, sl->elem_size, to), 1))
            : slot_move_at(slot, sl->elem_size, from, to);
        if (got) {
            int c = page * sl->page_slots + off;
            int next = c + 1;
            *hint = next >= cap ? 0 : next;
            /* The address, handed back because the sweep already has
             * it. A caller that took an ordinal and then asked for the
             * slot again would walk the page list a second time to
             * recompute something this loop was holding (issue 210e).
             */
            if (found)
                *found = slot;
            return c;
        }
        /* Forward one slot, crossing to the next page at its end and
         * back to the first page at the last one's — the wrap that
         * makes this a ring. */
        if (++off == sl->page_slots) {
            off = 0;
            pg = atomic_load_explicit(&pg->next, memory_order_acquire);
            page++;
            if (!pg) {
                pg = sl->pages;
                page = 0;
            }
        }
    }
    return -1;
}
/* }}} */

/* {{{ in_port_grow_locked() */
/*
 * One more page of slots on the end (issue 210e). Nothing is copied
 * and no existing slot moves, so there is no window to get right.
 *
 * **This replaced a copy-and-unwrap that had an ordering problem with
 * no correct answer**, and the shape of that problem is worth keeping
 * because it is what paging buys. Growing by allocating a larger
 * array meant carrying the live values across. Copy first and then
 * publish, and a value popped from the old array during the copy
 * exists in both places and is delivered twice; publish first and
 * then copy, and readers see an empty buffer while it fills. Neither
 * order is safe on its own. What made it safe was that nothing else
 * could happen at all during the copy, because the station's mutex
 * was held for the whole of it.
 *
 * That protection is exactly what issue 210d spends: once a claimer
 * copies its bytes outside the lock, "nothing else is happening" stops
 * being true, and relocating a slot underneath a worker that owns it
 * is the one thing slot ownership does not cover. So the copy had to
 * go rather than be ordered correctly — **removing the copy removes
 * the requirement rather than satisfying it.**
 *
 * Growth still takes the station's mutex, as one of the rare
 * structural operations, so that two threads meeting a full buffer
 * add one page between them rather than one each.
 */
static void in_port_grow_locked(in_port_t *sl)
{
    in_port_add_page(sl);
    /* A writer looking for space should start where the space now is.
     * Only a hint: being wrong costs a sweep, not a mistake. */
    sl->write_hint = sl->capacity - sl->page_slots;
    sl->growths++;
}
/* }}} */
/* {{{ in_port_write_locked() */
static void in_port_write(station_t *s, in_port_t *sl, const void *value)
{
    /* Look for somewhere to put it, and grow only if there is
     * genuinely nowhere (issue 210d). This used to grow when the tail
     * was one short of the head — a test on indices, which had to
     * leave a slot spare so that the two meeting could mean empty
     * rather than full. Asking the slots directly needs no spare and
     * no arithmetic: a full buffer is one where nothing answers.
     */
    void *slot = NULL;
    int c = in_port_scan(sl, &sl->write_hint, SLOT_EMPTY, SLOT_RESERVED,
                         &slot, 0);
    if (c < 0) {
        /* Nowhere to put it, so the port has to grow — and growth is
         * the one part of a write that takes the station's mutex, so
         * that two threads meeting a full buffer add one page between
         * them rather than one each.
         *
         * The scan is tried **again** after the lock is taken, before
         * growing. Another thread may have grown it while this one was
         * waiting, and adding a second page on top of the first would
         * be a buffer that doubles every time two writers are unlucky
         * together. */
        pthread_mutex_lock(&s->mutex);
        c = in_port_scan(sl, &sl->write_hint, SLOT_EMPTY, SLOT_RESERVED,
                         &slot, 0);
        if (c < 0) {
            in_port_grow_locked(sl);
            c = in_port_scan(sl, &sl->write_hint,
                             SLOT_EMPTY, SLOT_RESERVED, &slot, 0);
        }
        pthread_mutex_unlock(&s->mutex);
        if (c < 0) {
            fprintf(stderr, "delivery: a port with no free slot immediately "
                            "after growing to %d slots\n",
                    atomic_load(&sl->capacity));
            abort();
        }
    }

    /* **The copy happens with no lock held** (issue 210d). The scan
     * moved this slot to *reserved*, which means it belongs to this
     * thread and no other thread may touch its value — so the bytes
     * need no exclusion from anybody. Publishing it cannot fail for
     * any reason but somebody having touched a slot that was this
     * thread's alone, which is an engine bug worth stopping for.
     *
     * The release on that transition is what makes these bytes
     * visible to whoever later takes the slot from *ready*. */
    memcpy(slot, value, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, SLOT_RESERVED, SLOT_READY)) {
        fprintf(stderr, "delivery: publishing a slot this thread had "
                        "reserved, and somebody else had moved it\n");
        abort();
    }

    /* **Counted after it is published, never before.** A claimer asks
     * this number whether the port holds anything and then goes
     * looking; a count raised before the value was visible would send
     * it to find nothing, which the claim treats as an engine bug
     * rather than a lost race. Raising it afterwards can only mean a
     * claimer looked a moment too early and did not fire — and the
     * writer's own readiness check, which happens next, covers that. */
    int held = atomic_fetch_add_explicit(&sl->held, 1,
                                         memory_order_acq_rel) + 1;

    /* A diagnostic, and now a racy one: two writers can read the same
     * high-water mark and both write it back. The number can therefore
     * under-report by a little under contention. That is accepted
     * rather than fixed, because making it exact would put a
     * read-modify-write on the delivery path to sharpen a figure whose
     * only job is to tell a person that one input side is outpacing
     * its siblings — and it will still say so. */
    if (held > sl->high_water)
        sl->high_water = held;
}
/* }}} */

/* {{{ in_port_take_locked() */
/*
 * Take a ready slot, and take **only** the slot (issue 210d). The
 * value stays where it is; what changes is who owns it.
 *
 * This is the half of a claim that has to be under the station's
 * mutex, and it is deliberately the cheap half: a scan and one state
 * write per port. No bytes move here. The copying happens afterwards,
 * outside the lock, in `in_port_release`.
 *
 * **What makes that safe is ownership rather than exclusion.** A slot
 * in *claimed* belongs to exactly one worker and no other worker may
 * touch its value — which is written into the state table itself, not
 * asserted here. Bytes nobody else may touch need no lock around them.
 *
 * Returns whether it got one. Under the mutex, with the readiness
 * walk having already answered for this port, it always will: no
 * other claimer can be inside, and a writer only ever *adds*
 * availability. A no here is therefore an engine bug rather than a
 * lost race, which is why the caller stops rather than retrying.
 */
static int in_port_take_locked(in_port_t *sl, void **taken)
{
    int c = in_port_scan(sl, &sl->read_hint, SLOT_READY, SLOT_CLAIMED,
                         taken, 1);
    if (c < 0)
        return 0;
    sl->held--;
    return 1;
}
/* }}} */

/* {{{ in_port_release() */
/*
 * The other half, and the expensive one: copy the value out of a slot
 * this worker owns and hand the slot back empty. **Called with no lock
 * held at all.**
 *
 * That is where this whole line of work pays. A two-hundred-byte
 * struct per port used to be copied inside the station's mutex, so
 * every worker delivering into that station waited behind it; now
 * several workers copy out of one station at the same moment while a
 * different worker holds the lock doing its flips.
 *
 * The worker is finished with the slot the moment the copy lands in
 * its buffer. The box does not run until somebody picks the task up
 * later, reading from the task and holding no slot at all — which is
 * why no user code is ever inside this window either.
 */
static void in_port_release(in_port_t *sl, void *slot, void *into)
{
    memcpy(into, slot, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, SLOT_CLAIMED, SLOT_EMPTY)) {
        fprintf(stderr, "delivery: releasing a slot this thread had claimed, "
                        "and somebody else had moved it\n");
        abort();
    }
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The readiness dispatch (issue 204). Two tables, indexed by the     */
/* port's kind: "does it hold a value?" and "claim one". Another      */
/* port kind is a new row in each, never a new branch in two          */
/* functions that must be kept in agreement.                          */
/* ------------------------------------------------------------------ */

/* {{{ filled: ring / static */
static int ring_filled(const in_port_t *sl)
{
    /* A maintained count rather than two indices differing. The
     * indices stopped being able to answer this when values began
     * being claimed wherever they sat rather than from a computed
     * position (issue 210d) — the distance between a head and a tail
     * describes a contiguous run, and there is no longer one. */
    return sl->held > 0;
}

static int static_filled(const in_port_t *sl)
{
    /* A static's value is simply always there (issue 401). */
    (void)sl;
    return 1;
}

static int none_filled(const in_port_t *sl)
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

static int (*const in_port_filled[IN_PORT_KIND_COUNT])(const in_port_t *) = {
    [IN_PORT_RING]   = ring_filled,
    [IN_PORT_STATIC] = static_filled,
    [IN_PORT_NONE]   = none_filled,
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
static void ring_claim(in_port_t *sl, void *into, void **taken)
{
    /* Under the mutex the readiness walk has already established that
     * this port holds something, and nothing can have taken it since:
     * other claimers are excluded by the lock, and a writer only ever
     * *adds* availability — it moves a slot empty → reserved → ready
     * and never touches a ready one. So a scan that comes back
     * empty-handed means the count and the slots disagree, which is an
     * engine bug rather than a lost race.
     *
     * **There is no roll-back path here, and there is not meant to
     * be.** An earlier design claimed slots one at a time without the
     * lock and undid them when a later port came up empty, which
     * needed an ordering rule to keep two workers from each holding
     * half a claim forever. Checking every port *before* flipping any
     * of them makes that whole situation unreachable: either all the
     * ports answer and the claim succeeds outright, or one does not
     * and nothing was ever taken. */
    if (!in_port_take_locked(sl, taken)) {
        fprintf(stderr, "delivery: a port that answered ready had no ready "
                        "slot when asked for one\n");
        abort();
    }
    (void)into;
}

static void static_claim_locked(in_port_t *sl, void *into, void **taken)
{
    /* Nothing is taken, so nothing is released afterwards: a static is
     * peeked, never consumed. **And the copy stays under the lock**,
     * which is the one place issue 210d's argument does not reach. A
     * claimed ring slot belongs to one worker, and that ownership is
     * what lets its bytes be copied without exclusion; a static
     * belongs to nobody in particular, so the only thing standing
     * between a claim and a concurrent write to the same constant is
     * this mutex. Moving this copy outside it would reintroduce
     * exactly the torn read that putting the value on the port was
     * meant to make impossible. */
    *taken = NULL;
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

static void none_claim(in_port_t *sl, void *into, void **taken)
{
    (void)taken;
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
static void (*const in_port_claim_locked[IN_PORT_KIND_COUNT])
                   (in_port_t *, void *, void **) = {
    [IN_PORT_RING]   = ring_claim,
    [IN_PORT_STATIC] = static_claim_locked,
    [IN_PORT_NONE]   = none_claim,
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
    for (int i = 0; i < s->n_in_ports; i++)
        total += s->in_ports[i].elem_size;
    return total;
}
/* }}} */

/* {{{ station_ready_and_claim_locked() */
/*
 * The one rule made real: walk every port; if any is empty, nothing
 * happens and the value just written waits for its siblings. If all
 * are full, claim one value from each ring port into the caller's
 * buffer — copied out and the head advanced, so no other thread can
 * claim the same ones. Returns whether a task became due.
 */
static int station_ready_and_claim_locked(station_t *s, unsigned char *claimed,
                                          void **taken)
{
    /*
     * **Check all, then flip all** (issue 210d), and the order is the
     * whole safety argument. Nothing can take a ready slot away
     * between the two walks: other claimers are excluded by this
     * mutex, and a writer only ever adds availability — it moves a
     * slot empty → reserved → ready and never touches a ready one. So
     * either every port answers and the claim succeeds outright, or
     * one does not and nothing was ever taken.
     *
     * That is what makes a roll-back path unnecessary rather than
     * merely unused, and it is why livelock here is unreachable rather
     * than prevented.
     */
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (!in_port_filled[sl->kind](sl))
            return 0;
    }

    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        /* Every kind, unconditionally. The caller used to test the
         * function pointer here because the static row was null; there
         * is no null now, so the dispatch is a call rather than a call
         * guarded by a question about the table's own shape.
         *
         * A ring port records the slot it took and copies nothing; a
         * static copies here, under the lock, and records nothing.
         * The two are different because only one of them is owned. */
        in_port_claim_locked[sl->kind](sl, claimed + offset, &taken[i]);
        offset += sl->elem_size;
    }
    return 1;
}
/* }}} */

/* {{{ station_release_claimed() */
/*
 * The copies, and the release, with **no lock held** (issue 210d).
 *
 * Runs after the caller has dropped the station's mutex. Each slot
 * named here is in *claimed*, which means it belongs to this worker
 * and no other worker may touch its value — so the bytes need no
 * exclusion, and several workers can be doing this on one station at
 * the same moment while another holds the lock doing its flips.
 *
 * Statics are absent from this walk by construction: they recorded no
 * slot, because they were peeked rather than taken, and their bytes
 * were copied under the lock where the only protection they have
 * lives.
 */
static void station_release_claimed(station_t *s, unsigned char *claimed,
                                    void **taken)
{
    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (taken[i])
            in_port_release(sl, taken[i], claimed + offset);
        offset += sl->elem_size;
    }
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
                 + (size_t)s->n_in_ports * sizeof(void *)
                 + (size_t)in_bytes
                 + (size_t)s->out_size;

    task_t *t = malloc(total);
    if (!t)
        die("out of memory building a task", station_index);

    t->call = s->call;
    t->station = station_index;
    t->port = port;
    t->n_in = s->n_in_ports;
    /* Zero rather than left over: with timing compiled out nothing
     * ever writes it, and the delivery walk adds it to the station
     * unconditionally. */
    t->box_ns = 0;

    /* The pointer array sits immediately after the struct; the value
     * bytes after it; the output after those. One free() takes the
     * whole thing back. */
    t->in = (void **)(t + 1);
    unsigned char *data = (unsigned char *)(t->in + s->n_in_ports);

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
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
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
int map_station_start_after(map_t *m, int station,
                            void (*while_locked)(void *), void *ctx)
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
    void *taken[s->n_in_ports > 0 ? s->n_in_ports : 1];
    int port = 0;

    pthread_mutex_lock(&s->mutex);
    /*
     * Whatever the caller wanted done *inside* this hold, done here.
     *
     * There is exactly one such caller and it is writing a static
     * (issue 210d, step 7). A write to a static and the readiness
     * check it triggers both want this mutex, and doing them as two
     * acquisitions would leave a gap between the value changing and
     * the question being asked. Handing the work in rather than
     * exporting a locked variant is what keeps every acquisition of a
     * station's mutex inside this file, where the discipline can be
     * read in one place.
     */
    if (while_locked)
        while_locked(ctx);
    int due = station_ready_and_claim_locked(s, claimed, taken);
    if (due && s->kind == STATION_ITERATOR && s->n_out_ports > 0) {
        port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task (issue 210d). */
        station_release_claimed(s, claimed, taken);
        pool_push(m->pool, task_build(m, station, in_bytes > 0 ? claimed : NULL,
                                      port));
    }
    return due;
}
/* }}} */

/* {{{ map_station_try_start() */
int map_station_try_start(map_t *m, int station)
{
    return map_station_start_after(m, station, NULL, NULL);
}
/* }}} */

/* {{{ map_deliver_value() */
int map_deliver_value(map_t *m, int station, int port, const void *value)
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

    if (port < 0 || port >= s->n_in_ports)
        die("delivering to a port the station does not have", station);
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
    if (s->in_ports[port].kind == IN_PORT_NONE)
        die("delivering into a port that has no source yet", station);
    if (s->in_ports[port].kind != IN_PORT_RING)
        die("delivering into a port that is not a buffer", station);

    /* The claim buffer lives on this thread's stack, sized for one
     * complete input set. It exists so the readiness check allocates
     * nothing while holding the mutex. */
    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];

    int out_port = 0;
    void *taken[s->n_in_ports > 0 ? s->n_in_ports : 1];

    /* **The write takes no lock** (issue 210d). Reserving a slot is a
     * single compare-and-swap and the copy that follows goes into
     * bytes this thread owns, so deliveries into one station never
     * serialize against each other — only task construction does. */
    in_port_write(s, &s->in_ports[port], value);

    STATS_MARK(wait_start);
    pthread_mutex_lock(&s->mutex);
    STATS_CHARGE(s->mutex_wait_ns, wait_start);
    int due = station_ready_and_claim_locked(s, claimed, taken);
    if (due && s->kind == STATION_ITERATOR && s->n_out_ports > 0) {
        /* The one memory a station keeps, touched at the one moment
         * only one thread can be looking (issue 504): this task
         * takes the cursor's exit, the cursor moves on, and the
         * choice rides out inside the task. The box never sees it. */
        out_port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task (issue 210d). */
        station_release_claimed(s, claimed, taken);
        pool_push(m->pool, task_build(m, station, claimed, out_port));
    }
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
     * like any other port, never handed to the box (issue 502). The
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

/* {{{ station_hold_result() */
/*
 * Keep one result for somebody outside to take (issue 209).
 *
 * Under the station's own mutex, because a worker finishing a box and
 * a caller draining results genuinely meet here — unlike a slot,
 * which belongs to exactly one worker and needs no lock around its
 * bytes, a held result belongs to the station until it is taken.
 *
 * Doubling, and **the growth is shouted from the first one** rather
 * than summarised at the end like the other two piles. A port backing
 * up means uneven inputs and the task ring backing up means slow
 * consumers; both are performance signals worth a line at teardown.
 * This backing up means nobody is collecting the program's results at
 * all, which is not a performance signal — it is a program computing
 * into somewhere nobody is looking, and waiting until shutdown to
 * mention it wastes the entire run.
 */
static void station_hold_result(map_t *m, int index, station_t *s,
                                const void *value)
{
    pthread_mutex_lock(&s->mutex);
    if (s->n_held == s->held_room) {
        int room = s->held_room ? s->held_room * 2 : 8;
        void *bigger = realloc(s->held, (size_t)room * (size_t)s->out_size);
        if (!bigger) {
            pthread_mutex_unlock(&s->mutex);
            fprintf(stderr, "delivery: out of memory holding a result\n");
            abort();
        }
        s->held = bigger;
        s->held_room = room;
        if (s->held_growths++ > 0 || room > 8)
            fprintf(stderr,
                    "observe: results are piling up at %s — %d waiting and "
                    "nobody taking them; this program is computing into "
                    "somewhere nobody is looking\n",
                    (m->station_names && index < m->n_named
                     && m->station_names[index])
                        ? m->station_names[index] : "an output station",
                    s->n_held);
    }
    memcpy((unsigned char *)s->held + (size_t)s->n_held * (size_t)s->out_size,
           value, (size_t)s->out_size);
    s->n_held++;
    pthread_mutex_unlock(&s->mutex);
}
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

    int out_port_index = route_choose[s->kind](s, t);

    out_port_t *port = station_out_port(s, out_port_index);
    dest_set_t *set = out_port_dests(port);
    if (!set || set->n == 0) {
        /*
         * Nobody is wired here. For almost every station that means
         * **discard**, deliberately: an unwired comparator branch is
         * the ordinary case, and a program that sends everything below
         * a threshold somewhere means to drop the rest.
         *
         * For a designated output it means **hold**, and that is the
         * one rule the designation adds (issue 209). A program's own
         * results are the one thing discarding makes meaningless —
         * a program that computed them and dropped them did nothing.
         */
        if (s->door == DOOR_OUT)
            station_hold_result(m, t->station, s, t->out);
        return;
    }

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (int i = 0; i < set->n; i++)
        s->produced += map_deliver_value(m, set->items[i].station,
                                         set->items[i].port, t->out);
}
/* }}} */
