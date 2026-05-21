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
 *        - BOX_READ — emit a value. If the box has an inline literal
 *          `value`, emit it as-is; otherwise read the file at
 *          `box->path` (resolved relative to map_dir) and use its
 *          contents.
 *        - BOX_WRITE — write the `value` input to the path from the
 *          `path` input (or `box->path` if a static literal). After
 *          a successful write, emit the boolean `"true"` downstream
 *          for chain-after-write graphs; unwired output discards.
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

/* {{{ box_is_ready() — every required input has a value */
static int box_is_ready(const dispatch_ctx_t *ctx, int box_id)
{
    const box_t *b = graph_box(ctx->graph, box_id);
    if (!b) return 0;
    if (b->n_inputs == 0) return 1;
    if (!b->input_slot_ids) return 0;        /* runtime not attached */
    for (int i = 0; i < b->n_inputs; i++) {
        if (b->inputs[i].optional) continue;
        if (!slot_has_value(ctx->slots, b->input_slot_ids[i])) return 0;
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
            if (slot_push(ctx->slots, b->input_slot_ids[j],
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
        if (mode == SLOT_MODE_POP) {
            got = slot_pop(ctx->slots, b->input_slot_ids[i],
                           bufs[i], per_buf_cap);
        } else {
            got = slot_peek(ctx->slots, b->input_slot_ids[i],
                            bufs[i], per_buf_cap);
        }
        if (got < 0) {
            ua_unref(heap, buf_chunks[i]);
            buf_chunks[i] = NULL;
            bufs[i] = NULL;
            if (!b->inputs[i].optional) { *failed = 1; return n_present; }
            continue;
        }
        datas[n_present] = bufs[i];
        sizes[n_present] = got;
        n_present++;
    }
    return n_present;
}
/* }}} */

/* {{{ push_one_connection() — push to a single connection's slot */
/* The `tag` is the ordering hint for cell-tagged consumer slots
 * (issue 304's iterator-ordering rule). For pushes from
 * non-iterator producers it's 0; for iterator pushes the routing
 * code passes the counter value so parallel iterator tasks
 * preserve invocation order at the consumer. */
static int push_one_connection(dispatch_ctx_t *ctx, const box_t *b,
                               const connection_t *c,
                               const void *out_bytes, int out_size,
                               uint32_t tag)
{
    if (c->to_box_idx < 0) return 0;
    const box_t *dst = graph_box(ctx->graph, c->to_box_idx);
    if (!dst || !dst->input_slot_ids) return 0;
    if (c->to_input_idx < 0 || c->to_input_idx >= dst->n_inputs) return 0;
    if (slot_push(ctx->slots, dst->input_slot_ids[c->to_input_idx],
                  out_bytes, out_size, tag) != 0) {
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
    /* Pick the invoke path per issue 312's same-language fast-path
     * rules:
     *   - same-language chain ⇒ invoke_native (if the spec supplies one)
     *   - cross-language wire ⇒ invoke_json   (if the spec supplies one)
     *   - either way: fall back to the canonical `invoke` when the
     *     spec hasn't specialised. Our current specs all use the
     *     fallback, so behaviour is unchanged; the resolution is
     *     here so future specs can opt in without touching the
     *     dispatch layer. */
    lang_invoke_fn fn;
    if (b->use_native_invoke && spec->invoke_native) fn = spec->invoke_native;
    else if (!b->use_native_invoke && spec->invoke_json) fn = spec->invoke_json;
    else                                              fn = spec->invoke;
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
    if (n > (int)(sizeof bufs / sizeof bufs[0])) {
        fprintf(stderr, "dispatch: '%s' has %d inputs (cap = %d)\n",
                b->id, n, (int)(sizeof bufs / sizeof bufs[0]));
        return -1;
    }
    int failed = 0;
    int n_present = read_inputs(ctx, b, bufs, buf_chunks, datas, sizes,
                                ctx->default_out_capacity, &failed);
    if (failed) {
        release_inputs(ctx, n, bufs, buf_chunks);
        return -1;
    }

    /* Verbose: emit the input payloads now, before the spec runs. */
    emit_input_events(ctx, task_id, bufs, sizes, n_present);

    /* Resolve the box's source file relative to map_dir. */
    char *ref_path = b->ref ? resolve_path(ctx, b->ref) : NULL;

    int rc = fn(handle, ref_path ? ref_path : b->ref, b->fn,
                datas, sizes, n_present,
                out_buf, out_capacity, out_size);
    free(ref_path);
    release_inputs(ctx, n, bufs, buf_chunks);
    return rc;
}
/* }}} */

/* {{{ do_read_box() — emit the box's value */
/* Precedence (issue 229):
 *   1. Inline `value` literal — copied verbatim, no file IO.
 *   2. Static `path` field — read that file from disk.
 *   3. `path` input wire — would arrive in the box's input slot;
 *      not exercised by current fixtures but supported by reading
 *      slot[port="path"] when no static path is set.
 *   4. Otherwise, error — the box has nothing to emit. */
static int do_read_box(dispatch_ctx_t *ctx, const box_t *b,
                       char *out_buf, int out_capacity, int *out_size)
{
    /* (1) inline literal */
    if (b->value) {
        int n = (int)strlen(b->value);
        if (n > out_capacity) {
            fprintf(stderr, "dispatch: read box '%s' literal (%d bytes) "
                            "exceeds output capacity %d\n",
                    b->id, n, out_capacity);
            return -1;
        }
        memcpy(out_buf, b->value, (size_t)n);
        *out_size = n;
        return 0;
    }

    /* (2) static path. The (3) "path-from-input" form is left as a
     * future slice — the inputs[] slot needs to be peeked first.
     * Today's fixtures all use the static path. */
    const char *path_str = b->path;
    if (!path_str) {
        fprintf(stderr, "dispatch: read box '%s' has neither value "
                        "nor path\n", b->id);
        return -1;
    }
    char *full = resolve_path(ctx, path_str);
    if (!full) return -1;
    FILE *fp = fopen(full, "rb");
    if (!fp) {
        fprintf(stderr, "dispatch: read box '%s': cannot open '%s'\n",
                b->id, full);
        free(full);
        return -1;
    }
    int total = 0;
    while (total < out_capacity) {
        size_t r = fread(out_buf + total, 1,
                         (size_t)(out_capacity - total), fp);
        if (r == 0) break;
        total += (int)r;
    }
    fclose(fp);
    free(full);
    *out_size = total;
    return 0;
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
    int n_present = read_inputs(ctx, b, bufs, buf_chunks, datas, sizes,
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
            case BOX_READ:
                rc = do_read_box(ctx, b, out_buf, out_capacity, &out_size);
                break;
            case BOX_WRITE:
                rc = do_write_box(ctx, b, task_id,
                                  out_buf, out_capacity, &out_size);
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
