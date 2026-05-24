/* src/014-event-queue.c — multi-producer event queue with a
 * dedicated writer thread.
 *
 * Design: singly-linked queue under one mutex; one CV the writer
 * thread sleeps on when the queue is empty. Producers lock just
 * long enough to append a node and signal the CV. The writer
 * thread snapshots `head` (atomically swaps it with NULL) under
 * the lock, then drains the snapshot outside the lock — so the
 * serialization + fwrite work happens with zero producer
 * contention.
 *
 * The architecture doc's MPSC ring with Vyukov-style sequence
 * numbers is the next refinement once profiling shows the brief
 * append-mutex contention is the bottleneck. The shape of the
 * API stays the same.
 *
 * Designed in issue 311.
 */

#include "014-event-queue.h"
#include "013-jsonl-events.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct event_node {
    struct event_node *next;
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
} event_node_t;
/* }}} */

/* {{{ Queue struct */
struct event_queue {
    pthread_mutex_t  mtx;
    pthread_cond_t   cv;
    event_node_t    *head;
    event_node_t    *tail;
    _Atomic int      shutdown;

    pthread_t        writer_thread;
    jsonl_writer_t  *writer;

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

/* {{{ enqueue() — append a node, signal the writer */
static void enqueue(event_queue_t *q, event_node_t *n)
{
    n->next = NULL;
    pthread_mutex_lock(&q->mtx);
    if (q->tail) q->tail->next = n;
    else         q->head       = n;
    q->tail = n;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
    atomic_fetch_add_explicit(&q->pending, 1, memory_order_relaxed);
}
/* }}} */

/* {{{ write_one() — serialize one event through jsonl_emit_* */
static void write_one(event_queue_t *q, const event_node_t *n)
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

/* {{{ writer_main() — drain loop */
static void *writer_main(void *arg)
{
    event_queue_t *q = (event_queue_t *)arg;
    while (1) {
        pthread_mutex_lock(&q->mtx);
        while (!q->head && !atomic_load_explicit(&q->shutdown, memory_order_acquire)) {
            pthread_cond_wait(&q->cv, &q->mtx);
        }
        event_node_t *batch = q->head;
        q->head = NULL;
        q->tail = NULL;
        int shutting = atomic_load_explicit(&q->shutdown, memory_order_acquire);
        pthread_mutex_unlock(&q->mtx);

        /* Drain the batch outside the lock — zero contention with
         * producers while we encode + fwrite. */
        while (batch) {
            event_node_t *next = batch->next;
            write_one(q, batch);
            free(batch);
            atomic_fetch_sub_explicit(&q->pending, 1, memory_order_relaxed);
            batch = next;
        }
        if (shutting) {
            /* Catch any stragglers that arrived between the snapshot
             * and reading the shutdown flag. */
            pthread_mutex_lock(&q->mtx);
            batch = q->head; q->head = q->tail = NULL;
            pthread_mutex_unlock(&q->mtx);
            while (batch) {
                event_node_t *next = batch->next;
                write_one(q, batch);
                free(batch);
                atomic_fetch_sub_explicit(&q->pending, 1, memory_order_relaxed);
                batch = next;
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

    if (pthread_mutex_init(&q->mtx, NULL) != 0)        goto fail;
    if (pthread_cond_init (&q->cv,  NULL) != 0)        goto fail_cv;
    atomic_init(&q->shutdown, 0);
    atomic_init(&q->pending,  0);

    if (pthread_create(&q->writer_thread, NULL, writer_main, q) != 0)
        goto fail_thread;
    return q;

fail_thread:
    pthread_cond_destroy(&q->cv);
fail_cv:
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
    /* Signal shutdown, wake the writer if it's parked. */
    atomic_store_explicit(&q->shutdown, 1, memory_order_release);
    pthread_mutex_lock(&q->mtx);
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->mtx);

    pthread_join(q->writer_thread, NULL);

    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy (&q->cv);
    jsonl_writer_close(q->writer);
    free(q);
}
/* }}} */

/* {{{ event_queue_pending() */
int event_queue_pending(const event_queue_t *q)
{
    return q ? atomic_load_explicit(&q->pending, memory_order_relaxed) : 0;
}
/* }}} */

/* {{{ Emit functions */
void event_queue_run_start(event_queue_t *q, double ts,
                           const char *map, int n_workers)
{
    if (!q) return;
    event_node_t *n = calloc(1, sizeof *n);
    if (!n) return;
    n->kind = EV_RUN_START;
    n->ts   = ts;
    copy_str_bounded(n->u.run_start.map, map, MAP_BUF_SZ);
    n->u.run_start.n_workers = n_workers;
    enqueue(q, n);
}

void event_queue_task_submit(event_queue_t *q, double ts,
                             int task_id, const char *box_id, int worker_idx)
{
    if (!q) return;
    event_node_t *n = calloc(1, sizeof *n);
    if (!n) return;
    n->kind = EV_TASK_SUBMIT;
    n->ts   = ts;
    n->u.task_submit.task_id    = task_id;
    copy_str_bounded(n->u.task_submit.box_id, box_id, BOX_ID_BUF_SZ);
    n->u.task_submit.worker_idx = worker_idx;
    enqueue(q, n);
}

void event_queue_task_start(event_queue_t *q, double ts,
                            int task_id, int worker_idx)
{
    if (!q) return;
    event_node_t *n = calloc(1, sizeof *n);
    if (!n) return;
    n->kind = EV_TASK_START;
    n->ts   = ts;
    n->u.task_start.task_id    = task_id;
    n->u.task_start.worker_idx = worker_idx;
    enqueue(q, n);
}

void event_queue_task_end(event_queue_t *q, double ts,
                          int task_id, int worker_idx,
                          long duration_us, int output_size)
{
    if (!q) return;
    event_node_t *n = calloc(1, sizeof *n);
    if (!n) return;
    n->kind = EV_TASK_END;
    n->ts   = ts;
    n->u.task_end.task_id     = task_id;
    n->u.task_end.worker_idx  = worker_idx;
    n->u.task_end.duration_us = duration_us;
    n->u.task_end.output_size = output_size;
    enqueue(q, n);
}

void event_queue_run_end(event_queue_t *q, double ts,
                         long duration_us, int n_tasks)
{
    if (!q) return;
    event_node_t *n = calloc(1, sizeof *n);
    if (!n) return;
    n->kind = EV_RUN_END;
    n->ts   = ts;
    n->u.run_end.duration_us = duration_us;
    n->u.run_end.n_tasks     = n_tasks;
    enqueue(q, n);
}

/* Verbose events go straight through the jsonl_writer's mutex
 * rather than the event queue. The data payload can be up to
 * 4 KB each, so allocating fixed-size queue records for them
 * is wasteful; verbose mode is opt-in and infrequent enough that
 * the mutex contention is acceptable. */
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
