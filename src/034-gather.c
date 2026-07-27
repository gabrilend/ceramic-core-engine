/*
 * 034-gather.c — the one place the engine runs backwards.
 *
 * What this is: the pull path. A gatherer slot holds no buffer; at
 * the moment a task is being assembled, it reaches upstream and runs
 * the named station's box inline, so the value is fresh at the moment
 * it is used rather than at the moment it was produced. A file's
 * contents, a clock, an environment variable — pushed, they were
 * correct once; pulled, they are correct now.
 *
 * How it does it, in general terms: gathering is a plain function
 * call on the assembling worker's own stack. No task is pushed, no
 * pool is involved, no mutex is taken on the upstream station —
 * there is nothing on it to guard, because a gatherable station has
 * no buffers. Chains recurse; cycles would recurse forever and die
 * as a bare segfault, so every new gather wire is checked at the
 * moment it is drawn: if the graph was acyclic before the edge, any
 * cycle must run through it, so one forward walk from the source
 * settles the question.
 */
#include "018-station.h"

#include <stdio.h>
#include <stdlib.h>

/* {{{ die_gather() */
static void die_gather(int station, const char *what)
{
    fprintf(stderr, "gather: station %d: %s\n", station, what);
    abort();
}
/* }}} */

/* {{{ walk_reaches() */
/*
 * Does the gather graph reach `target` starting from `from`? The
 * walk follows gather links only — push wires are invisible here,
 * because a push cycle passes through a buffer and ends, while a
 * gather cycle is a call that never returns. Depth-first with an
 * explicit stack; maps are small and chains are short.
 */
static int walk_reaches(map_t *m, int from, int target)
{
    if (from == target)
        return 1;
    station_t *s = &m->stations[from];
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        if (sl->kind == SLOT_GATHER && walk_reaches(m, sl->source, target))
            return 1;
    }
    return 0;
}
/* }}} */

/* {{{ chain_depth() */
/* The longest gather chain hanging below a station — the worst-case
 * inline work a worker does while assembling one of its tasks. */
static int chain_depth(map_t *m, int station)
{
    station_t *s = &m->stations[station];
    int deepest = 0;
    for (int i = 0; i < s->n_slots; i++) {
        slot_t *sl = &s->slots[i];
        if (sl->kind == SLOT_GATHER) {
            int below = 1 + chain_depth(m, sl->source);
            if (below > deepest)
                deepest = below;
        }
    }
    return deepest;
}
/* }}} */

/* {{{ map_slot_gather() */
void map_slot_gather(map_t *m, int station, int slot, int source_station)
{
    if (station < 0 || station >= m->n_stations)
        die_gather(station, "converting a slot on a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die_gather(station, "converting a slot the box does not have");
    slot_t *sl = &s->slots[slot];
    if (sl->kind != SLOT_RING)
        die_gather(station, "the slot was already converted from a ring buffer");
    if (source_station < 0 || source_station >= m->n_stations)
        die_gather(station, "gathering from a station outside the table");
    station_t *src = &m->stations[source_station];
    if (!src->call)
        die_gather(station, "gathering from a station with no box placed");
    if (src->out_size != sl->elem_size)
        die_gather(station,
                   "the gathered box returns a different size than the slot takes");

    /* The cycle check, at the moment the wire is drawn (issue 404).
     * Two paths: the walk from the proposed source reaches back to
     * this station — the edge would close a loop, refuse it naming
     * both ends; or it does not — the graph stays acyclic by
     * induction, and the edge goes in.
     *
     * Note for issue 704: at runtime this check and the conversion
     * below must happen under one lock — two threads each adding an
     * individually legal edge can produce an illegal pair. During
     * construction and loading the caller is single-threaded. */
    if (walk_reaches(m, source_station, station)) {
        fprintf(stderr,
                "gather: wiring station %d to pull from station %d would close "
                "a cycle — a gather cycle is a call that never returns, "
                "surfacing as a bare stack overflow; refused\n",
                station, source_station);
        abort();
    }

    free(sl->storage);
    sl->storage = NULL;
    sl->capacity = 0;
    sl->head = 0;
    sl->tail = 0;
    sl->kind = SLOT_GATHER;
    sl->source = source_station;

    /* The same walk just paid for the depth reading — record the
     * deepest chain for phase 7 to report (issue 404). */
    int depth = chain_depth(m, station);
    if (depth > m->gather_depth)
        m->gather_depth = depth;
}
/* }}} */

/* {{{ gather_run() */
/*
 * Run one gatherable station inline and land its value at `out`.
 * Its arguments are statics (locked copies from the table) or
 * themselves gathered (recursion — bounded, because every edge was
 * cycle-checked when drawn). A ring slot here means the map was
 * mis-built and validation was skipped; the backstop is loud.
 *
 * THE exception, named as the docs demand: this is the one place a
 * box runs without a worker having picked it up from the pool. It
 * runs on a thread that is in the middle of assembling someone
 * else's task. Two workers can be inside the same gathered box at
 * the same instant, so a gathered box must touch nothing but its
 * arguments and the outside world it reads.
 */
static void gather_run(map_t *m, int station_idx, void *out)
{
    station_t *s = &m->stations[station_idx];
    int n = s->n_slots;

    int in_bytes = 0;
    for (int i = 0; i < n; i++)
        in_bytes += s->slots[i].elem_size;

    void *inptrs[n > 0 ? n : 1];
    unsigned char values[in_bytes > 0 ? in_bytes : 1];

    int offset = 0;
    for (int i = 0; i < n; i++) {
        slot_t *sl = &s->slots[i];
        inptrs[i] = values + offset;
        switch (sl->kind) {
        case SLOT_STATIC:
            static_claim(m, sl, values + offset);
            break;
        case SLOT_GATHER:
            gather_run(m, sl->source, values + offset);
            break;
        default:
            die_gather(station_idx,
                       "a gathered station has a ring-buffer slot — nothing "
                       "could ever fill it mid-gather; the map is mis-built");
        }
        offset += sl->elem_size;
    }

    /* A stack-borrowed task, never pooled, never freed: the shim
     * only reads in[] and writes out, and both point at this frame. */
    task_t t;
    t.call = s->call;
    t.station = station_idx;
    t.port = 0;
    t.n_in = n;
    t.in = inptrs;
    t.out = out;
    s->call(&t);
}
/* }}} */

/* {{{ gather_claim() */
void gather_claim(map_t *m, const slot_t *sl, void *into)
{
    gather_run(m, sl->source, into);
}
/* }}} */
