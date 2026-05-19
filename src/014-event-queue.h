/* src/014-event-queue.h — multi-producer event queue with a
 * dedicated writer thread.
 *
 * What it is, in a sentence: every worker thread (and the main
 * thread) calls into one of the emit functions; the queue
 * appends a small event record under a brief mutex and signals
 * the writer thread, which drains the queue in batches and writes
 * each event as a JSONL line to a file.
 *
 * Designed in issue 311. Decouples event emission from the JSON
 * encoding + fwrite hot path: every producer's critical section
 * is one node allocation plus a couple of pointer updates. The
 * MPSC-ring-with-sequence-numbers variant from the design doc is
 * the next refinement once profiling justifies it.
 *
 * Lifecycle: `event_queue_create("tmp/last-run.jsonl")` opens the
 * file, spawns the writer thread, and is ready to receive events.
 * `event_queue_destroy` signals shutdown, joins the writer thread,
 * closes the file, and frees the queue. Any events still in the
 * queue at shutdown drain to the file before the thread exits.
 *
 * The emit functions are non-blocking from the producer's
 * perspective (modulo brief mutex contention). They never write to
 * the file directly.
 */

#ifndef SORAMECH_EVENT_QUEUE_H
#define SORAMECH_EVENT_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct event_queue event_queue_t;

/* {{{ Lifecycle */
/* Open `path` for writing (truncates), spawn the writer thread.
 * Returns NULL on failure. */
event_queue_t *event_queue_create(const char *path);

/* Signal shutdown, drain remaining events, join the writer thread,
 * fflush+fclose, free the queue. Safe on NULL. */
void           event_queue_destroy(event_queue_t *q);
/* }}} */

/* {{{ Emit (producer side) */
/* Every emit clones its arguments into the queued event record;
 * the caller's pointers don't need to outlive the call. Strings
 * are bounded by event-record buffers (typically 64–128 bytes);
 * longer values are truncated. */

void event_queue_run_start  (event_queue_t *q, double ts,
                             const char *map, int n_workers);

void event_queue_task_submit(event_queue_t *q, double ts,
                             int task_id, const char *box_id, int worker_idx);

void event_queue_task_start (event_queue_t *q, double ts,
                             int task_id, int worker_idx);

void event_queue_task_end   (event_queue_t *q, double ts,
                             int task_id, int worker_idx,
                             long duration_us, int output_size);

void event_queue_run_end    (event_queue_t *q, double ts,
                             long duration_us, int n_tasks);

/* Verbose events (opt-in via SORAMECH_LOG_VALUES=1). These bypass
 * the queue and write directly through the writer's mutex — the
 * data fields can be up to 4 KB each, so queueing them as
 * fixed-size records is wasteful. Truncated at NUL or 4 KB. */
void event_queue_task_input (event_queue_t *q, double ts,
                             int task_id, int port_index,
                             const char *data, int size);

void event_queue_task_output(event_queue_t *q, double ts,
                             int task_id, const char *data, int size);

/* Slot allocator events (opt-in via SORAMECH_LOG_SLOTS=1). Like
 * the value events, these bypass the queue and write directly so
 * the soramech-pool can emit them in a tight loop right after
 * graph_attach_runtime without inflating the queue with many
 * fixed-size records that all fire at the same instant. */
void event_queue_slot_alloc (event_queue_t *q, double ts,
                             int slot_id, int cell_capacity, int n_cells,
                             const char *owner_box, const char *owner_port);
/* }}} */

/* {{{ Inspection */
/* Best-effort: returns the count of events currently queued
 * (between produce and drain). Mostly useful for diagnostics. */
int event_queue_pending(const event_queue_t *q);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_EVENT_QUEUE_H */
