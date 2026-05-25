/* src/014-event-queue.c — multi-producer event queue with a
 * dedicated writer thread.
 *
 * Design: Vyukov-style bounded MPSC ring. Producers do one
 * fetch-add / CAS on the enqueue position, write their payload
 * into the claimed slot, and bump the slot's sequence number to
 * publish it. The single consumer thread drains the ring by
 * reading the slot at the dequeue position and observing the
 * sequence number to know when the producer has finished writing.
 * A pthread mutex + condition variable provide the wakeup
 * mechanism — the mutex is NOT on the producer hot path; it's
 * only taken briefly to signal the consumer that data is
 * available (or, on rare full-ring contention, to block a
 * producer waiting for space).
 *
 * Why this shape: the mutex-linked-list predecessor serialised
 * every producer through one append-mutex; under bursty dispatch
 * (many small task_start / task_end events firing in close
 * succession from every worker) that mutex was the contention
 * hotspot. The ring's per-slot atomic seq lets producers proceed
 * independently — they only see each other if they happen to
 * claim adjacent slots in the same instant.
 *
 * Designed in issue 311.
 */

#include "014-event-queue.h"
#include "013-jsonl-events.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Ring sizing */
/* Power-of-two ring capacity. 4096 slots × ~160 B per slot ≈ 640
 * KB per event_queue. The dispatch can fire roughly 3 events per
 * task; a run of 1000 tasks queues ~3000 events, well under
 * capacity, but the ring sizes itself to absorb dispatch bursts
 * without backpressuring producers. */
#define EQ_RING_SIZE 4096u
#define EQ_RING_MASK (EQ_RING_SIZE - 1u)
/* }}} */

/* {{{ Event record */
typedef enum {
    EV_RUN_START,
    EV_TASK_SUBMIT,
    EV_TASK_START,
    EV_TASK_END,
    EV_RUN_END,
} event_kind_t;

#define MAP_BUF_SZ    128
#define BOX_ID_BUF_SZ 64

typedef struct {
    event_kind_t       kind;
    double             ts;
    union {
        struct {
            char map[MAP_BUF_SZ];
            int  n_workers;
        } run_start;

        struct {
            int  task_id;
            char box_id[BOX_ID_BUF_SZ];
            int  worker_idx;
        } task_submit;

        struct {
            int task_id;
            int worker_idx;
        } task_start;

        struct {
            int  task_id;
            int  worker_idx;
            long duration_us;
            int  output_size;
        } task_end;

        struct {
            long duration_us;
            int  n_tasks;
        } run_end;
    } u;
} event_payload_t;
/* }}} */

/* {{{ Ring slot — Vyukov sequence pattern.
 *
 * Each slot carries an atomic sequence number that tracks the
 * slot's lifecycle:
 *   - At init: seq[i] = i. The slot is "free for enqueue at
 *     position i, position i + RING_SIZE, position i + 2*RING_SIZE..."
 *   - Producer claims position p (where p % RING_SIZE == i). It
 *     spins until seq[i] == p, writes the payload, then stores
 *     seq[i] = p + 1 (release). The slot is now "ready for the
 *     consumer at position p."
 *   - Consumer reads position d (where d % RING_SIZE == i). It
 *     checks seq[i] == d + 1 (acquire). If so, reads payload,
 *     then stores seq[i] = d + RING_SIZE — the slot is now free
 *     for the next producer at position d + RING_SIZE.
 *
 * The producer / consumer never share a writable field other
 * than the per-slot seq, so contention is per-slot, not global. */
typedef struct {
    _Atomic uint64_t seq;
    event_payload_t  payload;
} eq_slot_t;
/* }}} */

/* {{{ Queue struct */
struct event_queue {
    /* The ring itself + the producer/consumer positions. */
    eq_slot_t        ring[EQ_RING_SIZE];
    _Atomic uint64_t enqueue_pos;       /* next slot a producer will claim */
    _Atomic uint64_t dequeue_pos;       /* next slot the consumer will read */

    /* Wakeup mechanism. The mutex is NOT held on the lock-free
     * enqueue / dequeue paths — it's only taken briefly to
     * signal a CV after producing (consumer wakes on has_data)
     * or after consuming (any blocked producer wakes on
     * has_space). */
    pthread_mutex_t  mtx;
    pthread_cond_t   has_data;
    pthread_cond_t   has_space;
    _Atomic int      shutdown;

    pthread_t        writer_thread;
    jsonl_writer_t  *writer;

    /* Best-effort count, exposed by event_queue_pending(). Not
     * load-bearing for sync — the per-slot seq is. */
    _Atomic int      pending;
};
/* }}} */

/* {{{ copy_str_bounded() — strncpy + always-NUL-terminate */
static void copy_str_bounded(char *dst, const char *src, size_t cap)
{
    if (cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}
/* }}} */

/* {{{ enqueue_ring() — claim a slot, write payload, publish seq.
 *
 * Returns 0 on success. The caller never sees failure — on a full
 * ring the producer blocks on `has_space` until the consumer
 * frees a slot. Blocking is the right tradeoff because the
 * alternative — dropping events — destroys the audit trail the
 * JSONL transcript exists for. */
static void enqueue_ring(event_queue_t *q, const event_payload_t *src)
{
    uint64_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
    for (;;) {
        eq_slot_t *slot = &q->ring[pos & EQ_RING_MASK];
        uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
        int64_t  diff = (int64_t)seq - (int64_t)pos;

        if (diff == 0) {
            /* Slot is ready for us at position pos. Try to claim
             * by bumping enqueue_pos. CAS-weak because the
             * producer is happy to retry on spurious failure. */
            if (atomic_compare_exchange_weak_explicit(
                    &q->enqueue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                slot->payload = *src;
                /* Publish: the seq bump's release pairs with the
                 * consumer's acquire load on slot->seq. Anything
                 * we wrote into payload above is now visible to
                 * the consumer once it sees seq == pos + 1. */
                atomic_store_explicit(&slot->seq, pos + 1,
                                      memory_order_release);
                atomic_fetch_add_explicit(&q->pending, 1,
                                          memory_order_relaxed);
                /* Wake the consumer if it's sleeping. The lock
                 * is held just long enough to signal — never on
                 * the hot path between producers. */
                pthread_mutex_lock(&q->mtx);
                pthread_cond_signal(&q->has_data);
                pthread_mutex_unlock(&q->mtx);
                return;
            }
            /* CAS lost; another producer beat us. pos was
             * refreshed in-place by the CAS. Retry. */
        } else if (diff < 0) {
            /* Ring is full at position pos. Block until the
             * consumer signals has_space. Recheck under the
             * lock to avoid lost wakeups. */
            pthread_mutex_lock(&q->mtx);
            seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
            if ((int64_t)seq - (int64_t)pos < 0
                && !atomic_load_explicit(&q->shutdown,
                                         memory_order_acquire)) {
                pthread_cond_wait(&q->has_space, &q->mtx);
            }
            pthread_mutex_unlock(&q->mtx);
            pos = atomic_load_explicit(&q->enqueue_pos,
                                       memory_order_relaxed);
        } else {
            /* diff > 0: another producer raced ahead and
             * published past us. Refresh and retry. */
            pos = atomic_load_explicit(&q->enqueue_pos,
                                       memory_order_relaxed);
        }
    }
}
/* }}} */

/* {{{ dequeue_ring() — consumer reads the next ready slot.
 *
 * Returns 1 + fills *out if a slot was ready, 0 if empty. The
 * consumer is the sole caller, so no CAS is needed on dequeue_pos
 * — a plain store is fine. */
static int dequeue_ring(event_queue_t *q, event_payload_t *out)
{
    uint64_t pos = atomic_load_explicit(&q->dequeue_pos,
                                        memory_order_relaxed);
    eq_slot_t *slot = &q->ring[pos & EQ_RING_MASK];
    uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
    int64_t  diff = (int64_t)seq - (int64_t)(pos + 1);
    if (diff != 0) return 0;       /* not yet published */

    *out = slot->payload;
    /* Mark the slot free for the producer that wraps around to
     * position (pos + RING_SIZE). */
    atomic_store_explicit(&slot->seq, pos + EQ_RING_SIZE,
                          memory_order_release);
    atomic_store_explicit(&q->dequeue_pos, pos + 1,
                          memory_order_relaxed);
    atomic_fetch_sub_explicit(&q->pending, 1, memory_order_relaxed);
    /* Wake any producer that was blocked waiting for space. */
    pthread_mutex_lock(&q->mtx);
    pthread_cond_signal(&q->has_space);
    pthread_mutex_unlock(&q->mtx);
    return 1;
}
/* }}} */

/* {{{ write_one() — serialize one event through jsonl_emit_* */
static void write_one(event_queue_t *q, const event_payload_t *n)
{
    switch (n->kind) {
        case EV_RUN_START:
            jsonl_emit_run_start(q->writer, n->ts,
                                 n->u.run_start.map,
                                 n->u.run_start.n_workers);
            break;
        case EV_TASK_SUBMIT:
            jsonl_emit_task_submit(q->writer, n->ts,
                                   n->u.task_submit.task_id,
                                   n->u.task_submit.box_id,
                                   n->u.task_submit.worker_idx);
            break;
        case EV_TASK_START:
            jsonl_emit_task_start(q->writer, n->ts,
                                  n->u.task_start.task_id,
                                  n->u.task_start.worker_idx);
            break;
        case EV_TASK_END:
            jsonl_emit_task_end(q->writer, n->ts,
                                n->u.task_end.task_id,
                                n->u.task_end.worker_idx,
                                n->u.task_end.duration_us,
                                n->u.task_end.output_size);
            break;
        case EV_RUN_END:
            jsonl_emit_run_end(q->writer, n->ts,
                               n->u.run_end.duration_us,
                               n->u.run_end.n_tasks);
            break;
    }
}
/* }}} */

/* {{{ writer_main() — drain loop.
 *
 * Two-state loop: drain everything you can see, then park on
 * has_data until a producer (or the destroy path) wakes you. The
 * recheck under the mutex avoids lost wakeups — the producer
 * publishes seq with release ordering BEFORE locking-and-signaling,
 * so a consumer that acquires the same lock after the producer
 * unlocks is guaranteed to observe the publish. */
static void *writer_main(void *arg)
{
    event_queue_t *q = (event_queue_t *)arg;
    event_payload_t payload;
    for (;;) {
        while (dequeue_ring(q, &payload)) {
            write_one(q, &payload);
        }
        pthread_mutex_lock(&q->mtx);
        /* Recheck pending under the lock — if a producer
         * published between the dequeue-empty and the lock, the
         * recheck catches it. */
        while (atomic_load_explicit(&q->pending,
                                    memory_order_acquire) == 0
               && !atomic_load_explicit(&q->shutdown,
                                        memory_order_acquire)) {
            pthread_cond_wait(&q->has_data, &q->mtx);
        }
        int shutting = atomic_load_explicit(&q->shutdown,
                                            memory_order_acquire);
        pthread_mutex_unlock(&q->mtx);
        if (shutting) {
            /* Final drain — anything still on the ring lands in
             * the file before the thread exits. */
            while (dequeue_ring(q, &payload)) {
                write_one(q, &payload);
            }
            break;
        }
    }
    return NULL;
}
/* }}} */

/* {{{ event_queue_create() */
event_queue_t *event_queue_create(const char *path)
{
    if (!path) return NULL;
    event_queue_t *q = calloc(1, sizeof *q);
    if (!q) return NULL;

    q->writer = jsonl_writer_open(path);
    if (!q->writer) { free(q); return NULL; }

    /* Seed each slot's seq to its own index — the slot is ready
     * for the producer at position == index (and at position ==
     * index + RING_SIZE, etc.). */
    for (uint64_t i = 0; i < EQ_RING_SIZE; i++) {
        atomic_init(&q->ring[i].seq, i);
    }
    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);

    if (pthread_mutex_init(&q->mtx, NULL) != 0)         goto fail;
    if (pthread_cond_init (&q->has_data,  NULL) != 0)   goto fail_data;
    if (pthread_cond_init (&q->has_space, NULL) != 0)   goto fail_space;
    atomic_init(&q->shutdown, 0);
    atomic_init(&q->pending,  0);

    if (pthread_create(&q->writer_thread, NULL, writer_main, q) != 0)
        goto fail_thread;
    return q;

fail_thread:
    pthread_cond_destroy(&q->has_space);
fail_space:
    pthread_cond_destroy(&q->has_data);
fail_data:
    pthread_mutex_destroy(&q->mtx);
fail:
    jsonl_writer_close(q->writer);
    free(q);
    return NULL;
}
/* }}} */

/* {{{ event_queue_destroy() */
void event_queue_destroy(event_queue_t *q)
{
    if (!q) return;
    /* Signal shutdown and wake the writer if it's parked on
     * has_data. Also wake any producer blocked on has_space —
     * once shutdown is set, producers that find the ring full
     * will fall through their wait loop rather than blocking
     * indefinitely. */
    atomic_store_explicit(&q->shutdown, 1, memory_order_release);
    pthread_mutex_lock(&q->mtx);
    pthread_cond_broadcast(&q->has_data);
    pthread_cond_broadcast(&q->has_space);
    pthread_mutex_unlock(&q->mtx);

    pthread_join(q->writer_thread, NULL);

    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy (&q->has_data);
    pthread_cond_destroy (&q->has_space);
    jsonl_writer_close(q->writer);
    free(q);
}
/* }}} */

/* {{{ event_queue_pending() */
int event_queue_pending(const event_queue_t *q)
{
    return q ? atomic_load_explicit(&q->pending,
                                    memory_order_relaxed) : 0;
}
/* }}} */

/* {{{ Emit functions */
void event_queue_run_start(event_queue_t *q, double ts,
                           const char *map, int n_workers)
{
    if (!q) return;
    event_payload_t p = {0};
    p.kind = EV_RUN_START;
    p.ts   = ts;
    copy_str_bounded(p.u.run_start.map, map, MAP_BUF_SZ);
    p.u.run_start.n_workers = n_workers;
    enqueue_ring(q, &p);
}

void event_queue_task_submit(event_queue_t *q, double ts,
                             int task_id, const char *box_id, int worker_idx)
{
    if (!q) return;
    event_payload_t p = {0};
    p.kind = EV_TASK_SUBMIT;
    p.ts   = ts;
    p.u.task_submit.task_id    = task_id;
    copy_str_bounded(p.u.task_submit.box_id, box_id, BOX_ID_BUF_SZ);
    p.u.task_submit.worker_idx = worker_idx;
    enqueue_ring(q, &p);
}

void event_queue_task_start(event_queue_t *q, double ts,
                            int task_id, int worker_idx)
{
    if (!q) return;
    event_payload_t p = {0};
    p.kind = EV_TASK_START;
    p.ts   = ts;
    p.u.task_start.task_id    = task_id;
    p.u.task_start.worker_idx = worker_idx;
    enqueue_ring(q, &p);
}

void event_queue_task_end(event_queue_t *q, double ts,
                          int task_id, int worker_idx,
                          long duration_us, int output_size)
{
    if (!q) return;
    event_payload_t p = {0};
    p.kind = EV_TASK_END;
    p.ts   = ts;
    p.u.task_end.task_id     = task_id;
    p.u.task_end.worker_idx  = worker_idx;
    p.u.task_end.duration_us = duration_us;
    p.u.task_end.output_size = output_size;
    enqueue_ring(q, &p);
}

void event_queue_run_end(event_queue_t *q, double ts,
                         long duration_us, int n_tasks)
{
    if (!q) return;
    event_payload_t p = {0};
    p.kind = EV_RUN_END;
    p.ts   = ts;
    p.u.run_end.duration_us = duration_us;
    p.u.run_end.n_tasks     = n_tasks;
    enqueue_ring(q, &p);
}

/* Verbose events go straight through the jsonl_writer's mutex
 * rather than the event queue. The data payload can be up to
 * 4 KB each, so embedding them in a ring slot is wasteful;
 * verbose mode is opt-in and infrequent enough that the writer's
 * own mutex is fine. */
void event_queue_task_input(event_queue_t *q, double ts,
                            int task_id, int port_index,
                            const char *data, int size)
{
    if (!q) return;
    jsonl_emit_task_input(q->writer, ts, task_id, port_index, data, size);
}

void event_queue_task_output(event_queue_t *q, double ts,
                             int task_id, const char *data, int size)
{
    if (!q) return;
    jsonl_emit_task_output(q->writer, ts, task_id, data, size);
}

void event_queue_slot_alloc(event_queue_t *q, double ts,
                            int slot_id, int cell_capacity, int n_cells,
                            const char *owner_box, const char *owner_port)
{
    if (!q) return;
    jsonl_emit_slot_alloc(q->writer, ts, slot_id, cell_capacity, n_cells,
                          owner_box, owner_port);
}

/* Runtime mutation events (issue 319 / 248 self-construction). Same
 * direct-write path as the verbose events — graph mutations are
 * infrequent and the payload is variable-length string data that
 * doesn't fit cleanly into a fixed-size queue record. */
void event_queue_box_create(event_queue_t *q, double ts,
                            const char *box_id, const char *kind,
                            const char *lang, const char *ref,
                            const char *fn)
{
    if (!q) return;
    jsonl_emit_box_create(q->writer, ts, box_id, kind, lang, ref, fn);
}

void event_queue_wire_add(event_queue_t *q, double ts,
                          const char *from_box, const char *from_branch,
                          const char *to_box, const char *to_input)
{
    if (!q) return;
    jsonl_emit_wire_add(q->writer, ts, from_box, from_branch, to_box, to_input);
}

void event_queue_push(event_queue_t *q, double ts,
                      const char *from_box, const char *to_box,
                      const char *to_input, int slot_id,
                      int n_bytes, const char *result)
{
    if (!q) return;
    jsonl_emit_push(q->writer, ts, from_box, to_box, to_input,
                    slot_id, n_bytes, result);
}
/* }}} */
