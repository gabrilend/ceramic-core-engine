/* src/012-dispatch.h — task dispatch layer, public API.
 *
 * What it is, in a sentence: the C function the thread pool runs
 * per task; reads inputs from the slot store, invokes the box's
 * language spec, pushes outputs to downstream input slots, and
 * spawns consumer tasks on input readiness.
 *
 * Designed in issue 304. As of this iteration: real input
 * reading, real spec invocation via per-worker handles, real
 * output pushing with FIFO + 1-cell peek slots, real
 * spawn-on-input-ready chaining under a single-spawn-per-box
 * rule. Iterator multi-spawn, the routing kinds beyond plain,
 * and the large-value heap path are still ahead.
 *
 * The `dispatch_ctx_t` bundle is the durable runtime state for
 * the whole run: graph, slot store, spec registry, pool, plus
 * a per-box "ever spawned" flag array and (optionally) a
 * per-box output-capture table for testing. Tasks carry a
 * pointer to the context so the action has everything it needs
 * without globals.
 */

#ifndef SORAMECH_DISPATCH_H
#define SORAMECH_DISPATCH_H

#include <stdatomic.h>
#include <stdint.h>
#include <pthread.h>

#include "010-graph-loader.h"
#include "011-spec-registry.h"
#include "009-slot-store.h"
#include "014-event-queue.h"
#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Types */
typedef struct dispatch_ctx {
    const graph_t        *graph;
    slot_store_t         *slots;
    spec_registry_t      *specs;
    pool_t               *pool;

    /* Diagnostic: incremented each time dispatch_action runs. */
    _Atomic int           tasks_dispatched;

    /* Per-box runtime state (spawn guard + optional output capture)
     * lives in chunked-append chunks (see ctx_box_chunk_t in
     * dispatch.c). The old flat-arrays-with-headroom model imposed
     * a 4096-box runtime cap that issue 319's create_box could
     * exceed; chunks remove the cap by growing on demand.
     *
     * Access goes through the private ctx_box_slot helper in
     * dispatch.c; callers don't touch these directly. */
    void                 *box_chunks_opaque;   /* ctx_box_chunk_t**, hidden type */
    _Atomic unsigned int  n_box_chunks;
    pthread_mutex_t       box_chunks_mu;
    int                   capture_outputs;

    /* Optional default output buffer size for invoke calls. If 0,
     * defaults to 4096. */
    int                   default_out_capacity;

    /* Optional run-log event queue (issue 311). When non-NULL, the
     * dispatch action emits task_start + task_end events through
     * it. NULL turns the per-task logging off; the runner emits
     * run_start / run_end on its own. */
    event_queue_t        *events;

    /* Verbose-event gate (issue 311 SORAMECH_LOG_VALUES=1). When
     * 1 and `events` is non-NULL, dispatch_action also emits
     * task_input / task_output events with the byte payloads
     * (truncated to 4 KB per the architecture doc). */
    int                   log_values;

    /* Monotonic task id counter for run-log correlation. */
    _Atomic int           next_task_id;
} dispatch_ctx_t;

/* Task structs are allocated from the unified allocator's slab
 * pool so spawn/free pairs amortise across the run instead of
 * paying per-call malloc bookkeeping. The chunk back-reference
 * lets dispatch_action release the cell via ua_unref without
 * having to look it up by data pointer. */
struct ua_chunk;

typedef struct dispatch_task {
    int                   box_id;
    int                   task_id;   /* assigned at dispatch_spawn time */
    const dispatch_ctx_t *ctx;

    /* 304 placeholder — live count of outgoing wires that still
     * carry an unconsumed value pushed by this task. The box-
     * retirement consumer that reads this lands with the
     * lifetime-tracking slice; for now the field is set to 0 on
     * spawn and otherwise unread. Keeping it in the layout
     * documents the intent and lets a future slice flip the
     * read-it-and-act-on-it switch without changing the struct
     * shape callers depend on. */
    uint32_t              live_wire_count;

    struct ua_chunk      *chunk;     /* set by dispatch_spawn; freed by dispatch_action */
} dispatch_task_t;
/* }}} */

/* {{{ Lifecycle */
/* Initialize the per-box runtime state owned by the context. The
 * graph + slots + specs + pool fields are caller-owned and just
 * referenced. Call after `graph_attach_runtime` so the per-box
 * runtime data is in place.
 *
 * `enable_output_capture` allocates `last_outputs` and
 * `last_output_sizes` so tests can inspect what each box wrote
 * after `pool_wait_quiescent`. */
int  dispatch_ctx_init   (dispatch_ctx_t *ctx,
                          const graph_t *g, slot_store_t *s,
                          spec_registry_t *r, pool_t *p,
                          int enable_output_capture, char **err);

/* Free per-box runtime arrays; does not touch the externally-owned
 * graph / slots / specs / pool fields. */
void dispatch_ctx_destroy(dispatch_ctx_t *ctx);

/* Read a previously-captured output (issue 319 follow-on: callers
 * no longer touch the per-box arrays directly because those moved
 * into private chunked storage). Returns the NUL-terminated bytes
 * if capture was enabled at init AND a previous task on `box_id`
 * wrote something; NULL otherwise. `*out_size` is filled with the
 * stored byte count on a hit. The returned pointer is valid until
 * the next capture for the same box or dispatch_ctx_destroy. */
const char *dispatch_captured_output(const dispatch_ctx_t *ctx,
                                     int box_id, int *out_size);
/* }}} */

/* {{{ Startup helpers */
/* Walk every box and push any input-port literal values into the
 * corresponding input slot. After this call, boxes whose inputs
 * are entirely literals are "ready" — spawn them with
 * `dispatch_spawn_if_ready` to start the run. */
int  dispatch_push_literals(dispatch_ctx_t *ctx, char **err);

/* If `box_id` has all required inputs available and hasn't been
 * spawned yet, allocate a task and submit it to the pool. */
void dispatch_spawn_if_ready(dispatch_ctx_t *ctx, int box_id, int priority);
/* }}} */

/* {{{ Task submission / action */
/* Submit a task explicitly, bypassing the spawn-if-ready check.
 * The dispatch_action will still consume inputs from slots and
 * push outputs to downstream slots. */
void dispatch_spawn(const dispatch_ctx_t *ctx, int box_id, int priority);

/* The pool action. Cast `arg` back to `dispatch_task_t *` inside.
 * Frees the task at the end of the action. */
void dispatch_action(void *arg);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_DISPATCH_H */
