/* src/012-dispatch.c — task dispatch layer, implementation.
 *
 * The pool action does five things, in order:
 *
 *   1. Look up the box from the graph.
 *   2. Read each input port's value from its slot (peek mode for
 *      this iteration; pop mode + iterator multi-spawn land later).
 *   3. Do the box's work:
 *        - BOX_CALL — invoke the resolved language spec via the
 *          worker's per-language handle.
 *        - BOX_WRITE — write the `value` input to the path from the
 *          `path` input (or `box->path` if a static literal). After
 *          a successful write, emit the boolean `"true"` downstream
 *          for chain-after-write graphs; unwired output discards.
 *        - BOX_READ — never reaches dispatch. Read boxes are
 *          pull-on-demand value sources (issue 244); their cached
 *          bytes are copied directly into a consumer's input buffer
 *          by `read_inputs` when the consumer fires and finds the
 *          slot empty. They are not tasks and never enter the pool.
 *   4. Push the resulting bytes to every outgoing connection's
 *      consumer input slot.
 *   5. For each consumer, fire the spawn-on-input-ready check,
 *      which spawns the consumer iff it has all required inputs
 *      and hasn't been spawned yet (single-spawn guard via an
 *      atomic CAS).
 *
 * The optional `capture_outputs` mode also memcpy's the output
 * bytes into `ctx->last_outputs[box_id]` so tests can verify
 * what each box produced after the pool quiesces.
 *
 * Designed in issue 304.
 */

#include "012-dispatch.h"
#include "016-unified-allocator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Per-task value buffers (per-input scratch and the output buffer)
 * come from the slot store's unified allocator now, not malloc.
 * This is the dispatch layer's participation in the reference-
 * counted value flow described in issue 302's design retraction:
 * every byte carrying a value through the run passes through the
 * same recycling pipeline, instead of one allocator for slot cells
 * and a separate malloc/free churn for the dispatch's scratch. */

/* {{{ now_secs() / mono_us() — timestamps */
static double now_secs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static long mono_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000000L + ts.tv_nsec / 1000;
}
/* }}} */

/* {{{ err_fmt() — malloc'd diagnostic string */
__attribute__((format(printf, 1, 2)))
static char *err_fmt(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len);
    out[len] = '\0';
    return out;
}
/* }}} */

/* {{{ dispatch_ctx_init() */
int dispatch_ctx_init(dispatch_ctx_t *ctx,
                      const graph_t *g, slot_store_t *s,
                      spec_registry_t *r, pool_t *p,
                      int enable_output_capture, char **err)
{
    if (!ctx || !g) {
        if (err) *err = err_fmt("dispatch_ctx_init: NULL ctx or graph");
        return -1;
    }
    memset(ctx, 0, sizeof *ctx);
    ctx->graph = g;
    ctx->slots = s;
    ctx->specs = r;
    ctx->pool  = p;
    atomic_init(&ctx->tasks_dispatched, 0);
    atomic_init(&ctx->next_task_id,     0);
    ctx->n_boxes              = graph_n_boxes(g);
    ctx->default_out_capacity = 4096;
    ctx->events               = NULL;     /* opt-in via caller assignment */
    ctx->log_values           = 0;        /* opt-in via SORAMECH_LOG_VALUES=1 */

    if (ctx->n_boxes > 0) {
        ctx->box_ever_spawned = calloc((size_t)ctx->n_boxes, sizeof(_Atomic int));
        if (!ctx->box_ever_spawned) {
            if (err) *err = err_fmt("out of memory");
            return -1;
        }
        for (int i = 0; i < ctx->n_boxes; i++) {
            atomic_init(&ctx->box_ever_spawned[i], 0);
        }

        if (enable_output_capture) {
            ctx->capture_outputs    = 1;
            ctx->last_outputs       = calloc((size_t)ctx->n_boxes, sizeof(char *));
            ctx->last_output_sizes  = calloc((size_t)ctx->n_boxes, sizeof(int));
            if (!ctx->last_outputs || !ctx->last_output_sizes) {
                if (err) *err = err_fmt("out of memory");
                return -1;
            }
        }
    }
    return 0;
}
/* }}} */

/* {{{ dispatch_ctx_destroy() */
void dispatch_ctx_destroy(dispatch_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->last_outputs) {
        for (int i = 0; i < ctx->n_boxes; i++) free(ctx->last_outputs[i]);
        free(ctx->last_outputs);
    }
    free(ctx->last_output_sizes);
    free(ctx->box_ever_spawned);
    memset(ctx, 0, sizeof *ctx);
}
/* }}} */

/* {{{ box_is_ready() — every required input has a value or fallback
 *
 * A port is satisfied when:
 *   - it is optional, OR
 *   - its slot currently holds a value, OR
 *   - it has at least one read-box predecessor (issue 244 — the
 *     consumer can pull the cached value at attempt time even
 *     though the slot is empty). */
static int box_is_ready(const dispatch_ctx_t *ctx, int box_id)
{
    const box_t *b = graph_box(ctx->graph, box_id);
    if (!b) return 0;
    if (b->n_inputs == 0) return 1;
    if (!b->input_slot_ids) return 0;        /* runtime not attached */
    for (int i = 0; i < b->n_inputs; i++) {
        if (b->inputs[i].optional) continue;
        if (slot_has_value(ctx->slots, b->input_slot_ids[i])) continue;
        if (b->n_read_predecessors && b->n_read_predecessors[i] > 0) continue;
        return 0;
    }
    return 1;
}
/* }}} */

/* {{{ box_pop_ready() — every POP input has at least one cell */
/* For multi-spawn boxes, we want to re-spawn so long as every POP
 * input still has a queued value. PEEK inputs are always available
 * after their initial push, so they don't gate continuation. */
static int box_pop_ready(const dispatch_ctx_t *ctx, int box_id)
{
    const box_t *b = graph_box(ctx->graph, box_id);
    if (!b || !b->input_slot_modes) return 0;
    int saw_pop = 0;
    for (int i = 0; i < b->n_inputs; i++) {
        if (b->input_slot_modes[i] != SLOT_MODE_POP) continue;
        if (b->inputs[i].optional) continue;
        saw_pop = 1;
        if (!slot_has_value(ctx->slots, b->input_slot_ids[i])) return 0;
    }
    return saw_pop;
}
/* }}} */

/* {{{ dispatch_spawn() */
void dispatch_spawn(const dispatch_ctx_t *ctx, int box_id, int priority)
{
    if (!ctx || !ctx->pool) return;
    dispatch_task_t *t = malloc(sizeof *t);
    if (!t) return;

    /* Assign task_id at submission so the same id correlates the
     * task_submit / task_start / task_end events. The next_task_id
     * counter is atomic so concurrent spawners don't collide. */
    dispatch_ctx_t *mctx = (dispatch_ctx_t *)ctx;
    t->task_id = atomic_fetch_add_explicit(&mctx->next_task_id, 1,
                                           memory_order_relaxed);
    t->box_id  = box_id;
    t->ctx     = ctx;

    if (ctx->events) {
        const box_t *b = graph_box(ctx->graph, box_id);
        event_queue_task_submit(ctx->events, now_secs(),
            t->task_id, b ? b->id : "(unknown)", -1);
    }
    pool_spawn(ctx->pool, dispatch_action, t, priority);
}
/* }}} */

/* {{{ dispatch_spawn_if_ready() */
void dispatch_spawn_if_ready(dispatch_ctx_t *ctx, int box_id, int priority)
{
    if (!ctx) return;
    if (box_id < 0 || box_id >= ctx->n_boxes) return;
    if (!box_is_ready(ctx, box_id)) return;

    const box_t *b = graph_box(ctx->graph, box_id);
    /* 244: read boxes are pull-on-demand value sources, not tasks.
     * They never enter the pool — their consumers read the cached
     * value directly when they fire. */
    if (b && b->kind == BOX_READ) return;
    if (b && b->multi_spawn) {
        /* Multi-spawn box (iterator or anything downstream of one).
         * The single-spawn guard doesn't apply: every push that
         * leaves the box ready spawns a new task, which will drain
         * one cell from each POP input. The iterator's own
         * dispatch_action also re-spawns itself while inputs remain
         * so a single push can wake the chain. */
        dispatch_spawn(ctx, box_id, priority);
        return;
    }

    /* Single-spawn case: CAS the per-box guard. The first push that
     * finds the consumer ready wins; everyone else short-circuits.
     * The 1-cell peek model spawns each box at most once per run. */
    int expected = 0;
    if (atomic_compare_exchange_strong(&ctx->box_ever_spawned[box_id],
                                       &expected, 1)) {
        dispatch_spawn(ctx, box_id, priority);
    }
}
/* }}} */

/* {{{ dispatch_push_literals() */
int dispatch_push_literals(dispatch_ctx_t *ctx, char **err)
{
    if (!ctx) {
        if (err) *err = err_fmt("dispatch_push_literals: NULL ctx");
        return -1;
    }
    for (int i = 0; i < ctx->n_boxes; i++) {
        const box_t *b = graph_box(ctx->graph, i);
        if (!b || !b->input_slot_ids) continue;
        for (int j = 0; j < b->n_inputs; j++) {
            const input_decl_t *p = &b->inputs[j];
            if (!p->literal) continue;
            int sz = (int)strlen(p->literal);
            /* slot_push_native aliases to slot_push for single-ring
             * slots; for dual-ring slots (issue 312 slice 3), it
             * writes to the native ring + ordering ring atomically.
             * Literals are treated as native — they're configured
             * inline in the consumer's own box JSON, so semantically
             * they belong to the consumer's language and the spec's
             * native decoder reads them directly. */
            if (slot_push_native(ctx->slots, b->input_slot_ids[j],
                                 p->literal, sz, 0) != 0) {
                if (err) *err = err_fmt("box '%s' input '%s': "
                                        "literal push failed",
                                        b->id, p->name);
                return -1;
            }
        }
    }
    return 0;
}
/* }}} */

/* {{{ resolve_path() — join map_dir + relative path */
/* Caller frees the returned string. */
static char *resolve_path(const dispatch_ctx_t *ctx, const char *p)
{
    if (!p) return NULL;
    if (p[0] == '/') return strdup(p);             /* already absolute */
    const char *base = graph_map_dir(ctx->graph);
    if (!base) return strdup(p);
    size_t blen = strlen(base);
    size_t plen = strlen(p);
    char *out = malloc(blen + 1 + plen + 1);
    if (!out) return NULL;
    memcpy(out, base, blen);
    out[blen] = '/';
    memcpy(out + blen + 1, p, plen);
    out[blen + 1 + plen] = '\0';
    return out;
}
/* }}} */

/* {{{ emit_input_events() — log every input port under LOG_VALUES */
static void emit_input_events(dispatch_ctx_t *ctx, int task_id,
                              char **bufs, const int *sizes, int n_inputs)
{
    if (!ctx->log_values || !ctx->events) return;
    double ts = now_secs();
    for (int i = 0; i < n_inputs; i++) {
        if (!bufs[i]) continue;
        event_queue_task_input(ctx->events, ts, task_id, i,
                               bufs[i], sizes[i]);
    }
}
/* }}} */

/* {{{ release_inputs() — release every per-input chunk back to the allocator */
/* Companion to read_inputs. Callers that own the returned buffers
 * (every do_*_box) call this on every exit path. NULL entries are
 * tolerated — an optional input that wasn't present has both
 * bufs[i] and buf_chunks[i] set to NULL. */
static void release_inputs(const dispatch_ctx_t *ctx, int n,
                           char **bufs, ua_chunk_t **buf_chunks)
{
    ua_t *heap = slot_store_allocator(ctx->slots);
    for (int i = 0; i < n; i++) {
        if (buf_chunks[i]) ua_unref(heap, buf_chunks[i]);
        buf_chunks[i] = NULL;
        bufs[i] = NULL;
    }
}
/* }}} */

/* {{{ read_inputs() — peek or pop every input slot into buffers */
/* Returns the number of inputs read (== b->n_inputs). The read
 * mode per port comes from b->input_slot_modes — PEEK leaves the
 * cell in place (subsequent tasks re-read the same value), POP
 * drains the head cell (the queue advances). Sets *failed nonzero
 * if any required input was missing or oversized.
 *
 * Buffer memory comes from the slot store's unified allocator;
 * each successful read places the chunk handle into buf_chunks[i]
 * and the data pointer into bufs[i]. The caller releases them
 * via release_inputs on every exit path, whether success or
 * failure, so the chunks return to the allocator's free-lists. */
static int read_inputs(const dispatch_ctx_t *ctx, const box_t *b,
                       char **bufs, ua_chunk_t **buf_chunks,
                       const void **datas, int *sizes,
                       int *input_native,
                       int per_buf_cap, int *failed)
{
    *failed = 0;
    if (b->n_inputs == 0 || !b->input_slot_ids) return 0;
    ua_t *heap = slot_store_allocator(ctx->slots);
    int n_present = 0;
    for (int i = 0; i < b->n_inputs; i++) {
        buf_chunks[i] = ua_alloc(heap, (size_t)per_buf_cap);
        if (!buf_chunks[i]) { *failed = 1; return n_present; }
        bufs[i] = ua_data(buf_chunks[i]);
        int got;
        int mode = b->input_slot_modes ? b->input_slot_modes[i] : SLOT_MODE_PEEK;
        /* Slice 3 of issue 312: dual-ring slots are read via
         * slot_pop_ordered so the ordering ring drives the read
         * order across the native and JSON sub-rings. The returned
         * which_ring tag becomes the per-input native flag the spec
         * sees. Single-ring slots fall back to the existing
         * peek / pop and take their format from the box's per-port
         * input_edge_native[] classification (uniform across the
         * port — every cell in a single-ring slot is the same
         * format because the producers were all classified the
         * same way at graph load). */
        int32_t flags  = slot_flags(ctx->slots, b->input_slot_ids[i]);
        int     native = 1;
        if (flags >= 0 && (flags & SLOT_FLAG_DUAL_RING)) {
            int32_t which = SLOT_RING_NATIVE;
            got = slot_pop_ordered(ctx->slots, b->input_slot_ids[i],
                                   bufs[i], per_buf_cap, &which);
            native = (which == SLOT_RING_NATIVE) ? 1 : 0;
        } else {
            if (mode == SLOT_MODE_POP) {
                got = slot_pop(ctx->slots, b->input_slot_ids[i],
                               bufs[i], per_buf_cap);
            } else {
                got = slot_peek(ctx->slots, b->input_slot_ids[i],
                                bufs[i], per_buf_cap);
            }
            native = (b->input_edge_native && b->input_edge_native[i]) ? 1 : 0;
        }
        /* 244 pull-on-demand: if the slot was empty and the port
         * has read-box predecessors, copy the next read box's
         * cached bytes into the buffer. With multiple predecessors
         * the per-port atomic counter rotates the choice across
         * concurrent attempt-tasks. The native flag stays the
         * port's existing input_edge_native value — same wire
         * classification the loader assigned for any feeder. */
        if (got < 0 && b->n_read_predecessors &&
            b->n_read_predecessors[i] > 0) {
            int n_preds = b->n_read_predecessors[i];
            int pick = 0;
            if (n_preds > 1 && b->read_pred_counter_slot &&
                b->read_pred_counter_slot[i] >= 0) {
                uint32_t v = slot_read_inc(ctx->slots,
                                           b->read_pred_counter_slot[i],
                                           (uint32_t)n_preds);
                pick = (int)v;
            }
            int src_idx = b->read_predecessor_ids[i][pick];
            const box_t *src = graph_box(ctx->graph, src_idx);
            if (!src || !src->cached_value) {
                ua_unref(heap, buf_chunks[i]);
                buf_chunks[i] = NULL;
                bufs[i] = NULL;
                if (!b->inputs[i].optional) { *failed = 1; return n_present; }
                continue;
            }
            if (src->cached_size > per_buf_cap) {
                fprintf(stderr, "dispatch: read box '%s' (%d bytes) exceeds "
                                "consumer '%s' input '%s' capacity %d\n",
                        src->id, src->cached_size, b->id,
                        b->inputs[i].name, per_buf_cap);
                ua_unref(heap, buf_chunks[i]);
                buf_chunks[i] = NULL;
                bufs[i] = NULL;
                *failed = 1;
                return n_present;
            }
            memcpy(bufs[i], src->cached_value, (size_t)src->cached_size);
            got    = src->cached_size;
            native = (b->input_edge_native && b->input_edge_native[i]) ? 1 : 0;
        }

        if (got < 0) {
            ua_unref(heap, buf_chunks[i]);
            buf_chunks[i] = NULL;
            bufs[i] = NULL;
            if (!b->inputs[i].optional) { *failed = 1; return n_present; }
            continue;
        }
        datas[n_present]        = bufs[i];
        sizes[n_present]        = got;
        if (input_native) input_native[n_present] = native;
        n_present++;
    }
    return n_present;
}
/* }}} */

/* {{{ wire_is_native() — does this specific wire stay within one language?
 *
 * Returns 1 iff the given outgoing connection terminates at a
 * consumer in the producer's own language. Used by push_one_connection
 * to pick the dual-ring slot's native or JSON ring per wire — each
 * wire's writing style is its own. */
static int wire_is_native(const box_t *producer, const connection_t *c)
{
    if (!producer->output_edge_native || !producer->connections) return 0;
    int idx = (int)(c - producer->connections);
    if (idx < 0 || idx >= producer->n_connections) return 0;
    return producer->output_edge_native[idx];
}
/* }}} */

/* {{{ all_wires_native() — does every outgoing wire stay in this language?
 *
 * Used to pass `output_native` to the spec as a hint about whether
 * any cross-language consumer exists. Specs that can produce
 * different formats may use this to pre-emptively pick the cheaper
 * one when all consumers are same-lang. Slice 4 of issue 312
 * threads this through; per-wire conversion (the symmetric
 * per-edge picture, where each wire gets its own format) is the
 * dispatch's job, not the spec's. */
static int all_wires_native(const box_t *b)
{
    if (!b->output_edge_native || b->n_connections <= 0) return 1;
    for (int j = 0; j < b->n_connections; j++) {
        if (!b->output_edge_native[j]) return 0;
    }
    return 1;
}
/* }}} */

/* {{{ push_one_connection() — push to a single connection's slot */
/* The `tag` is the ordering hint for cell-tagged consumer slots
 * (issue 304's iterator-ordering rule). For pushes from
 * non-iterator producers it's 0; for iterator pushes the routing
 * code passes the counter value so parallel iterator tasks
 * preserve invocation order at the consumer.
 *
 * `conn_idx` is the producer-side connection index (which entry in
 * `b->connections` this is) — used to look up
 * `b->output_edge_native[conn_idx]` when the consumer's slot is
 * dual-ring (issue 312 slice 3). For producers that don't carry
 * per-edge classification (read / write / data boxes — no
 * `output_edge_native` array), the bit defaults to 0 (treated as
 * cross-language; the JSON ring receives the push). */
static int push_one_connection(dispatch_ctx_t *ctx, const box_t *b,
                               const connection_t *c,
                               const void *out_bytes, int out_size,
                               uint32_t tag)
{
    if (c->to_box_idx < 0) return 0;
    const box_t *dst = graph_box(ctx->graph, c->to_box_idx);
    if (!dst || !dst->input_slot_ids) return 0;
    if (c->to_input_idx < 0 || c->to_input_idx >= dst->n_inputs) return 0;

    int     dst_slot = dst->input_slot_ids[c->to_input_idx];
    int32_t dst_flags = slot_flags(ctx->slots, dst_slot);
    int     rc;

    /* Each wire carries its own writing style — same-language wires
     * push native, cross-language wires push JSON. The format is
     * decided per outgoing connection (per-edge bit), not per call.
     * A box with mixed fan-out (some same-lang, some cross-lang)
     * lets each consumer get the right format for its own wire,
     * which is what the dual-ring slot's per-cell tag preserves. */
    if (dst_flags >= 0 && (dst_flags & SLOT_FLAG_DUAL_RING)) {
        int native_edge = wire_is_native(b, c);
        if (native_edge) {
            rc = slot_push_native(ctx->slots, dst_slot,
                                  out_bytes, out_size, tag);
        } else {
            rc = slot_push_json(ctx->slots, dst_slot,
                                out_bytes, out_size, tag);
        }
    } else {
        rc = slot_push(ctx->slots, dst_slot, out_bytes, out_size, tag);
    }

    if (rc != 0) {
        fprintf(stderr, "dispatch: '%s' → '%s'.%s: push failed\n",
                b->id, dst->id, c->to_input);
        return -1;
    }
    dispatch_spawn_if_ready(ctx, c->to_box_idx, 0);
    return 0;
}
/* }}} */

/* {{{ push_branch() — push only to outgoing connections matching `branch` */
/* `branch` is one of "lt" / "eq" / "gt" / "out_<n>". Connections
 * whose from_branch is NULL (plain) are skipped; connections whose
 * from_branch equals `branch` fire. */
static int push_branch(dispatch_ctx_t *ctx, const box_t *b,
                       const char *branch,
                       const void *out_bytes, int out_size,
                       uint32_t tag)
{
    int fired = 0;
    for (int i = 0; i < b->n_connections; i++) {
        const connection_t *c = &b->connections[i];
        if (!c->from_branch) continue;
        if (strcmp(c->from_branch, branch) != 0) continue;
        if (push_one_connection(ctx, b, c, out_bytes, out_size, tag) != 0)
            return -1;
        fired++;
    }
    return fired;
}
/* }}} */

/* {{{ push_to_downstream() — fan output to every outgoing connection */
/* The push triggers spawn_if_ready on the consumer. Tag = 0 for
 * the plain fan-out case; iterator routing uses push_branch with a
 * non-zero tag. */
static int push_to_downstream(dispatch_ctx_t *ctx, const box_t *b,
                              const void *out_bytes, int out_size)
{
    for (int i = 0; i < b->n_connections; i++) {
        if (push_one_connection(ctx, b, &b->connections[i],
                                out_bytes, out_size, 0) != 0) return -1;
    }
    return 0;
}
/* }}} */

/* {{{ parse_double() — parse a NUL-bounded byte run as a double */
static double parse_double(const void *bytes, int size)
{
    char tmp[64];
    int n = size < 63 ? size : 63;
    if (n > 0) memcpy(tmp, bytes, (size_t)n);
    tmp[n] = '\0';
    return strtod(tmp, NULL);
}
/* }}} */

/* {{{ push_routed() — branch picker over routing.kind, then push */
static int push_routed(dispatch_ctx_t *ctx, const box_t *b,
                       const void *out_bytes, int out_size)
{
    /* Read and write boxes don't carry a routing kind; their
     * output fans plain. Same for call boxes with no routing set
     * (treated as plain). */
    if (b->kind != BOX_CALL) return push_to_downstream(ctx, b, out_bytes, out_size);

    switch (b->routing.kind) {
        case ROUTING_PLAIN:
            return push_to_downstream(ctx, b, out_bytes, out_size);

        case ROUTING_COMPARATOR: {
            double v = parse_double(out_bytes, out_size);
            const char *branch =
                (v < b->routing.comparand) ? "lt" :
                (v > b->routing.comparand) ? "gt" : "eq";
            return push_branch(ctx, b, branch, out_bytes, out_size, 0) < 0 ? -1 : 0;
        }

        case ROUTING_ITERATOR: {
            if (b->counter_slot_id < 0 || b->routing.n_outputs < 1) {
                /* No counter slot or zero outputs — fall back to plain. */
                return push_to_downstream(ctx, b, out_bytes, out_size);
            }
            /* The counter is both the branch selector AND the order
             * tag for the consumer slot (issue 304 cell-tagged
             * ordering). Tagged consumers serve pops by ascending
             * tag, so parallel iterator tasks land in invocation
             * order regardless of which worker finishes first. */
            uint32_t idx = slot_read_inc(ctx->slots, b->counter_slot_id,
                                         (uint32_t)b->routing.n_outputs);
            char branch[24];
            snprintf(branch, sizeof branch, "out_%u", idx);
            return push_branch(ctx, b, branch, out_bytes, out_size, idx) < 0 ? -1 : 0;
        }

        case ROUTING_RANDOMIZER: {
            if (b->counter_slot_id < 0 || b->routing.n_outputs < 1) {
                return push_to_downstream(ctx, b, out_bytes, out_size);
            }
            /* Mix the monotonic counter so consecutive invocations
             * don't go to consecutive branches. xorshift32-style,
             * no crypto pretensions. */
            uint32_t i = slot_read_inc(ctx->slots, b->counter_slot_id, UINT32_MAX);
            uint32_t h = i;
            h ^= h >> 16; h *= 0x85ebca6bu;
            h ^= h >> 13; h *= 0xc2b2ae35u;
            h ^= h >> 16;
            uint32_t branch_idx = h % (uint32_t)b->routing.n_outputs;
            char branch[24];
            snprintf(branch, sizeof branch, "out_%u", branch_idx);
            /* Randomizer is also multi-spawn-aware; pass i as tag so
             * any downstream tagged slot still serves in order. */
            return push_branch(ctx, b, branch, out_bytes, out_size, i) < 0 ? -1 : 0;
        }

        case ROUTING_WEIGHTED: {
            if (b->counter_slot_id < 0 || b->routing.n_outputs < 1 ||
                !b->routing.weights) {
                return push_to_downstream(ctx, b, out_bytes, out_size);
            }
            /* Cumulative-band lookup against a counter scaled to a
             * fixed precision. The last band absorbs floating-point
             * rounding so the bands sum to the whole range. */
            enum { WEIGHTED_PRECISION = 1000 };
            uint32_t i = slot_read_inc(ctx->slots, b->counter_slot_id,
                                       (uint32_t)WEIGHTED_PRECISION);
            double total = 0.0;
            for (int k = 0; k < b->routing.n_outputs; k++) {
                total += b->routing.weights[k];
            }
            if (total <= 0.0) {
                return push_to_downstream(ctx, b, out_bytes, out_size);
            }
            int branch_idx = b->routing.n_outputs - 1;
            double acc = 0.0;
            for (int k = 0; k < b->routing.n_outputs; k++) {
                acc += (b->routing.weights[k] / total) * WEIGHTED_PRECISION;
                if ((double)i < acc) { branch_idx = k; break; }
            }
            char branch[24];
            snprintf(branch, sizeof branch, "out_%d", branch_idx);
            return push_branch(ctx, b, branch, out_bytes, out_size, i) < 0 ? -1 : 0;
        }

        case ROUTING_DISTRIBUTOR:
        default:
            /* Distributor inspects downstream slot fill levels; not
             * yet implemented. Fall back to plain so the runtime
             * doesn't deadlock. */
            return push_to_downstream(ctx, b, out_bytes, out_size);
    }
}
/* }}} */

/* {{{ capture() — optional test hook, memcpy output into ctx */
static void capture(dispatch_ctx_t *ctx, int box_id,
                    const void *bytes, int size)
{
    if (!ctx->capture_outputs || !ctx->last_outputs) return;
    if (box_id < 0 || box_id >= ctx->n_boxes) return;
    free(ctx->last_outputs[box_id]);
    char *copy = malloc((size_t)size + 1);
    if (!copy) return;
    if (size > 0) memcpy(copy, bytes, (size_t)size);
    copy[size] = '\0';
    ctx->last_outputs[box_id]      = copy;
    ctx->last_output_sizes[box_id] = size;
}
/* }}} */

/* {{{ do_call_box() — invoke a call box's language spec */
static int do_call_box(dispatch_ctx_t *ctx, const box_t *b, int task_id,
                       char *out_buf, int out_capacity, int *out_size)
{
    if (b->spec_idx < 0) {
        fprintf(stderr, "dispatch: '%s' has no resolved spec\n", b->id);
        return -1;
    }
    const lang_spec_t *spec = spec_registry_at(ctx->specs, b->spec_idx);
    if (!spec) {
        fprintf(stderr, "dispatch: '%s' spec not in registry\n", b->id);
        return -1;
    }
    /* Slice 5 of issue 312: there's now a single invoke entry point.
     * The per-input `input_native[]` array and the per-call
     * `output_native` flag carry the same-language-vs-cross-language
     * information the old `invoke_native` / `invoke_json` callback
     * split was meant to encode. The bridges (`native_to_json`,
     * `json_to_native`) stay on the spec interface for the few
     * places that consume them directly. */
    lang_invoke_fn fn = spec->invoke;
    if (!fn) {
        fprintf(stderr, "dispatch: '%s' spec missing invoke\n", b->id);
        return -1;
    }
    void *handle = pool_current_worker
        ? pool_current_worker->handles[b->spec_idx]
        : NULL;
    if (!handle) {
        fprintf(stderr, "dispatch: '%s' worker handle for spec[%d] is NULL\n",
                b->id, b->spec_idx);
        return -1;
    }

    /* Read inputs. */
    int n = b->n_inputs;
    char        *bufs[16]       = {0};
    ua_chunk_t  *buf_chunks[16] = {0};
    const void  *datas[16]      = {0};
    int          sizes[16]      = {0};
    int          input_native[16] = {0};
    if (n > (int)(sizeof bufs / sizeof bufs[0])) {
        fprintf(stderr, "dispatch: '%s' has %d inputs (cap = %d)\n",
                b->id, n, (int)(sizeof bufs / sizeof bufs[0]));
        return -1;
    }
    int failed = 0;
    int n_present = read_inputs(ctx, b, bufs, buf_chunks, datas, sizes,
                                input_native,
                                ctx->default_out_capacity, &failed);
    if (failed) {
        release_inputs(ctx, n, bufs, buf_chunks);
        return -1;
    }

    /* Verbose: emit the input payloads now, before the spec runs. */
    emit_input_events(ctx, task_id, bufs, sizes, n_present);

    /* Resolve the box's source file relative to map_dir. */
    char *ref_path = b->ref ? resolve_path(ctx, b->ref) : NULL;

    /* Slice 4.5 of issue 312: pick the box's output format once, at
     * call time. If every outgoing wire goes to a consumer in this
     * box's own language, the spec writes its fast native form;
     * otherwise it writes JSON so the cross-language consumers can
     * parse it. The decision is per-call, not per-edge — the spec
     * writes once, the push then routes that one format to every
     * consumer uniformly. Same-language consumers in a
     * mixed-fan-out box eat the JSON cost as the price of having a
     * cross-language sibling; the precision tradeoff is captured in
     * issue 312's "Resolved design choice" section. */
    int output_native = all_wires_native(b);

    int rc = fn(handle, b, ref_path ? ref_path : b->ref, b->fn,
                datas, sizes, input_native, n_present,
                output_native,
                out_buf, out_capacity, out_size);
    free(ref_path);
    release_inputs(ctx, n, bufs, buf_chunks);
    return rc;
}
/* }}} */

/* {{{ do_write_box() — write `value` input to `path`, emit "true" */
/* Convention (issue 229): inputs are named "path" (string) and
 * "value" (bytes). Either may carry a static literal supplied
 * via dispatch_push_literals.
 *
 * On success, the box pushes the boolean string `"true"` to its
 * outgoing connections. A `write` box with no downstream wire
 * still produces the boolean; the dispatch fans to zero consumers
 * and the value is discarded (unwired-output rule). */
static int do_write_box(dispatch_ctx_t *ctx, const box_t *b, int task_id,
                        char *out_buf, int out_capacity, int *out_size)
{
    char        *bufs[16]       = {0};
    ua_chunk_t  *buf_chunks[16] = {0};
    const void  *datas[16]      = {0};
    int          sizes[16]      = {0};
    int failed = 0;
    /* do_write_box doesn't invoke a language spec — it's a dispatch
     * primitive that writes a file. The per-input native flag isn't
     * meaningful here, so pass NULL. */
    int n_present = read_inputs(ctx, b, bufs, buf_chunks, datas, sizes,
                                /*input_native=*/NULL,
                                ctx->default_out_capacity, &failed);
    emit_input_events(ctx, task_id, bufs, sizes, n_present);
    int path_idx = -1, value_idx = -1;
    for (int i = 0; i < b->n_inputs; i++) {
        if (strcmp(b->inputs[i].name, "path")  == 0) path_idx  = i;
        if (strcmp(b->inputs[i].name, "value") == 0) value_idx = i;
    }
    if (failed || path_idx < 0 || value_idx < 0
            || path_idx >= n_present || value_idx >= n_present) {
        release_inputs(ctx, b->n_inputs, bufs, buf_chunks);
        return -1;
    }
    /* Make NUL-terminated copy of the path. */
    char *path = malloc((size_t)sizes[path_idx] + 1);
    if (!path) {
        release_inputs(ctx, b->n_inputs, bufs, buf_chunks);
        return -1;
    }
    memcpy(path, datas[path_idx], (size_t)sizes[path_idx]);
    path[sizes[path_idx]] = '\0';

    char *full = resolve_path(ctx, path);
    free(path);
    if (!full) {
        release_inputs(ctx, b->n_inputs, bufs, buf_chunks);
        return -1;
    }

    FILE *fp = fopen(full, "wb");
    free(full);
    int rc = 0;
    if (!fp) rc = -1;
    else {
        size_t want = (size_t)sizes[value_idx];
        if (fwrite(datas[value_idx], 1, want, fp) != want) rc = -1;
        fclose(fp);
    }
    release_inputs(ctx, b->n_inputs, bufs, buf_chunks);

    if (rc == 0 && out_buf && out_capacity >= 4) {
        memcpy(out_buf, "true", 4);
        *out_size = 4;
    } else {
        *out_size = 0;
    }
    return rc;
}
/* }}} */

/* {{{ dispatch_action() — the pool action body */
void dispatch_action(void *arg)
{
    dispatch_task_t *t = (dispatch_task_t *)arg;
    if (!t) return;
    dispatch_ctx_t *ctx = (dispatch_ctx_t *)t->ctx;
    const box_t *b = graph_box(ctx->graph, t->box_id);

    atomic_fetch_add_explicit(&ctx->tasks_dispatched, 1, memory_order_relaxed);

    int task_id    = t->task_id;
    int worker_idx = pool_current_worker ? pool_current_worker->thread_idx : -1;

    long start_us = mono_us();
    if (ctx->events)
        event_queue_task_start(ctx->events, now_secs(), task_id, worker_idx);

    int out_size = 0;
    int out_capacity = ctx->default_out_capacity > 0
                        ? ctx->default_out_capacity : 4096;
    /* The output buffer comes from the slot store's unified
     * allocator so it participates in the same refcount-driven
     * recycling as input buffers and slot cells — every byte that
     * carries a value through the dispatch runs through one heap. */
    ua_t *heap = slot_store_allocator(ctx->slots);
    ua_chunk_t *out_chunk = ua_alloc(heap, (size_t)out_capacity);
    char *out_buf = out_chunk ? ua_data(out_chunk) : NULL;
    int rc = -1;
    if (out_buf && b) {
        switch (b->kind) {
            case BOX_CALL:
                rc = do_call_box(ctx, b, task_id, out_buf, out_capacity, &out_size);
                break;
            case BOX_WRITE:
                rc = do_write_box(ctx, b, task_id,
                                  out_buf, out_capacity, &out_size);
                break;
            case BOX_READ:
                /* 244: read boxes are pull-on-demand value sources;
                 * dispatch_spawn_if_ready filters them out before
                 * they ever reach this point. Reaching here is a
                 * bug — log loudly. */
                fprintf(stderr, "dispatch: BUG — read box '%s' "
                                "reached dispatch_action\n", b->id);
                rc = -1;
                break;
        }
    }

    if (rc == 0 && b) {
        capture(ctx, t->box_id, out_buf, out_size);
        /* Verbose: emit the task's output payload before push (so
         * the log entries land in computation order). */
        if (ctx->log_values && ctx->events) {
            event_queue_task_output(ctx->events, now_secs(),
                                    task_id, out_buf, out_size);
        }
        /* Every kind pushes — even write, which emits its "true"
         * boolean for any downstream wires (unwired output is just
         * discarded, per the 229 unwired-output rule). */
        push_routed(ctx, b, out_buf, out_size);
        /* Iterator-style re-spawn: if this is a multi-spawn box and
         * any POP input still has queued values, schedule another
         * task on the same box. This is how an iterator "loops"
         * over its input queue without the upstream needing to
         * push twice. */
        if (b->multi_spawn && box_pop_ready(ctx, t->box_id)) {
            dispatch_spawn(ctx, t->box_id, 0);
        }
    } else if (b) {
        fprintf(stderr, "dispatch: box '%s' failed (rc=%d)\n", b->id, rc);
    }

    long end_us = mono_us();
    if (ctx->events) {
        event_queue_task_end(ctx->events, now_secs(), task_id, worker_idx,
                             end_us - start_us, out_size);
    }

    if (out_chunk) ua_unref(heap, out_chunk);
    free(t);
}
/* }}} */
