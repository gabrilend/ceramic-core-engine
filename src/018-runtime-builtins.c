/* src/018-runtime-builtins.c — runtime self-construction primitives.
 *
 * See 018-runtime-builtins.h for the contract. This file does the
 * actual parsing-and-instantiating; the heavy lifting is in the
 * existing graph_add_box / box_add_connection / slot_alloc /
 * spec_registry primitives. This module is the glue.
 *
 * Issue 319d slice 1 supports kind=call, lang=lua, routing.kind=plain
 * with simple { name, type } inputs. Other shapes return an error
 * naming what's not yet supported.
 */
#include "018-runtime-builtins.h"
#include "010-graph-loader.h"
#include "009-slot-store.h"
#include "011-spec-registry.h"
#include "013-jsonl-events.h"
#include "014-event-queue.h"
#include "017-box-id.h"
#include "json.h"
#include "lang-spec.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* {{{ Thread-local active context */
static __thread struct graph         *tls_graph;
static __thread struct slot_store    *tls_slots;
static __thread struct spec_registry *tls_specs;
static __thread struct event_queue   *tls_events;

void runtime_set_active_context(struct graph         *g,
                                struct slot_store    *s,
                                struct spec_registry *r,
                                struct event_queue   *e)
{
    tls_graph  = g;
    tls_slots  = s;
    tls_specs  = r;
    tls_events = e;
}

void runtime_clear_active_context(void)
{
    tls_graph  = NULL;
    tls_slots  = NULL;
    tls_specs  = NULL;
    tls_events = NULL;
}

struct graph         *runtime_active_graph(void)  { return tls_graph; }
struct slot_store    *runtime_active_slots(void)  { return tls_slots; }
struct spec_registry *runtime_active_specs(void)  { return tls_specs; }
struct event_queue   *runtime_active_events(void) { return tls_events; }

/* now_secs: monotonic timestamp for event emission. Matches the
 * choice the pool runner uses so all events share one clock. */
static double rt_now_secs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ Small error-formatting helper */
__attribute__((format(printf, 2, 3)))
static int set_err(char **err_out, const char *fmt, ...)
{
    if (!err_out) return -1;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) { *err_out = NULL; return -1; }
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    char *out = malloc(len + 1);
    if (out) { memcpy(out, buf, len); out[len] = '\0'; }
    *err_out = out;
    return -1;
}
/* }}} */

/* {{{ Resolve a language name to its index in the spec registry */
static int find_spec_idx(struct spec_registry *r, const char *lang)
{
    int n = spec_registry_size(r);
    for (int i = 0; i < n; i++) {
        const lang_spec_t *s = spec_registry_at(r, i);
        if (s && s->name && strcmp(s->name, lang) == 0) return i;
    }
    return -1;
}
/* }}} */

/* {{{ Strdup-into-arena helper */
static const char *arena_strdup(json_arena_t *a, const char *s)
{
    /* The arena exposes parse/value helpers but not a raw strdup;
     * for runtime-created boxes we don't strictly need the arena
     * to own the string — graph_destroy doesn't free per-box id /
     * lang / etc. strings on the runtime path. But to keep
     * lifetimes uniform we copy via plain malloc and lean on
     * graph_destroy to free runtime-created boxes' owned strings.
     *
     * Slice-1 simplification: we malloc each string individually
     * and rely on the box's free at graph_destroy. Each string
     * field that runtime-created boxes own is freed there. */
    (void)a;
    size_t len = strlen(s);
    char  *p   = malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);
    return p;
}
/* }}} */

/* {{{ Build input declarations from the JSON inputs array */
static int build_inputs(const json_node_t *inputs_node,
                        input_decl_t **out_inputs, int *out_n,
                        char **err_out)
{
    if (!inputs_node) {
        *out_inputs = NULL;
        *out_n      = 0;
        return 0;
    }
    if (json_kind((json_node_t *)inputs_node) != JSON_ARRAY) {
        return set_err(err_out, "create_box: 'inputs' must be an array");
    }
    int n = json_array_size(inputs_node);
    if (n == 0) {
        *out_inputs = NULL;
        *out_n      = 0;
        return 0;
    }
    input_decl_t *arr = calloc((size_t)n, sizeof *arr);
    if (!arr) return set_err(err_out, "create_box: out of memory");

    for (int i = 0; i < n; i++) {
        json_node_t *e = json_array_at(inputs_node, i);
        if (!e || json_kind(e) != JSON_OBJECT) {
            free(arr);
            return set_err(err_out, "create_box: inputs[%d] is not an object", i);
        }
        json_node_t *name_n = json_object_get(e, "name");
        json_node_t *type_n = json_object_get(e, "type");
        if (!name_n || json_kind(name_n) != JSON_STRING) {
            free(arr);
            return set_err(err_out, "create_box: inputs[%d].name missing", i);
        }
        arr[i].name = arena_strdup(NULL, json_string_value(name_n));
        arr[i].type = (type_n && json_kind(type_n) == JSON_STRING)
                          ? arena_strdup(NULL, json_string_value(type_n))
                          : NULL;
        arr[i].literal = NULL;
        arr[i].optional = 0;
        /* Issue 246: per-port custom translation shim — optional. */
        json_node_t *xlate_n = json_object_get(e, "custom_translation");
        arr[i].custom_translation =
            (xlate_n && json_kind(xlate_n) == JSON_STRING)
                ? arena_strdup(NULL, json_string_value(xlate_n))
                : NULL;
    }
    *out_inputs = arr;
    *out_n      = n;
    return 0;
}
/* }}} */

/* {{{ runtime_create_box() */
int runtime_create_box(const char *spec_json, int spec_len,
                       char *out_id, size_t out_id_size,
                       char **err_out)
{
    if (err_out) *err_out = NULL;

    struct graph         *g     = runtime_active_graph();
    struct slot_store    *slots = runtime_active_slots();
    struct spec_registry *specs = runtime_active_specs();
    if (!g || !slots || !specs) {
        return set_err(err_out, "create_box: no active runtime context "
                                "(must be called from within a spec invoke)");
    }
    if (!spec_json || spec_len <= 0) {
        return set_err(err_out, "create_box: spec is empty");
    }
    if (!out_id || out_id_size < BOX_ID_GEN_BUF_SIZE) {
        return set_err(err_out, "create_box: out_id buffer too small "
                                "(need at least %u bytes)",
                       (unsigned)BOX_ID_GEN_BUF_SIZE);
    }

    /* Parse the JSON spec. We use a scratch arena that we destroy
     * at the end of the function — the box record's string fields
     * are deep-copied via malloc above, so the arena's lifetime
     * doesn't need to outlive the parse. */
    json_arena_t *arena = json_arena_create();
    if (!arena) return set_err(err_out, "create_box: out of memory (arena)");

    /* json_parse expects null-terminated input. Make a local copy
     * so we don't require the caller to null-terminate. */
    char *spec_dup = malloc((size_t)spec_len + 1);
    if (!spec_dup) { json_arena_destroy(arena); return set_err(err_out, "create_box: out of memory"); }
    memcpy(spec_dup, spec_json, (size_t)spec_len);
    spec_dup[spec_len] = '\0';

    int json_err_off = 0;
    const char *json_err_msg = NULL;
    json_node_t *root = json_parse(arena, spec_dup, &json_err_off, &json_err_msg);
    free(spec_dup);
    if (!root || json_kind(root) != JSON_OBJECT) {
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: spec is not a JSON object (%s)",
                       json_err_msg ? json_err_msg : "parse failed");
    }

    /* Required: kind. Slice 1 — only "call" is supported. */
    json_node_t *kind_n = json_object_get(root, "kind");
    if (!kind_n || json_kind(kind_n) != JSON_STRING ||
        strcmp(json_string_value(kind_n), "call") != 0) {
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: only kind='call' is supported "
                                "in this slice (issue 319d)");
    }

    /* Required: lang. Issue 319e extends supported langs to any
     * spec that's already registered AND whose per-worker state
     * has been initialised on the current worker. The "already
     * initialised" constraint surfaces as a runtime error when
     * the new box gets dispatched (worker handle NULL); the
     * compile pipeline's spec filter is what decides which langs
     * a worker init's, and that filter runs at graph load.
     * Workaround: include at least one static box of each lang
     * you intend to create_box at runtime. Lifting this is a
     * follow-on (on-demand spec init). */
    json_node_t *lang_n = json_object_get(root, "lang");
    if (!lang_n || json_kind(lang_n) != JSON_STRING) {
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: 'lang' string required");
    }
    const char *lang = json_string_value(lang_n);
    int spec_idx = find_spec_idx(specs, lang);
    if (spec_idx < 0) {
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: lang='%s' not registered "
                                "in the spec registry", lang);
    }

    /* Required: ref + fn */
    json_node_t *ref_n = json_object_get(root, "ref");
    json_node_t *fn_n  = json_object_get(root, "fn");
    if (!ref_n || json_kind(ref_n) != JSON_STRING ||
        !fn_n  || json_kind(fn_n)  != JSON_STRING) {
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: 'ref' and 'fn' strings required");
    }

    /* Optional id; auto-generate when absent. */
    char id_buf[BOX_ID_GEN_BUF_SIZE];
    const char *id_source;
    json_node_t *id_n = json_object_get(root, "id");
    if (id_n && json_kind(id_n) == JSON_STRING) {
        id_source = json_string_value(id_n);
    } else {
        if (box_id_generate(id_buf, sizeof id_buf) != 0) {
            json_arena_destroy(arena);
            return set_err(err_out, "create_box: id generator failed");
        }
        id_source = id_buf;
    }

    /* Build inputs. */
    input_decl_t *inputs = NULL;
    int           n_inputs = 0;
    if (build_inputs(json_object_get(root, "inputs"),
                     &inputs, &n_inputs, err_out) != 0) {
        json_arena_destroy(arena);
        return -1;
    }

    /* Allocate the box record. */
    box_t *b = calloc(1, sizeof *b);
    if (!b) {
        free(inputs);
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: out of memory (box)");
    }
    b->id   = arena_strdup(NULL, id_source);
    b->kind = BOX_CALL;
    b->lang = arena_strdup(NULL, lang);
    b->ref  = arena_strdup(NULL, json_string_value(ref_n));
    b->fn   = arena_strdup(NULL, json_string_value(fn_n));
    b->returns = NULL;
    b->routing.kind      = ROUTING_PLAIN;
    b->routing.n_outputs = 1;
    b->output_capacity = 256;   /* slice-1 default; 0 (variable) not exposed yet */
    b->n_inputs  = n_inputs;
    b->inputs    = inputs;
    atomic_init(&b->n_connections, 0);
    atomic_init(&b->connections,   NULL);
    b->spec_idx  = spec_idx;
    b->counter_slot_id = -1;
    b->multi_spawn     = 0;

    /* Allocate one input slot per input port. The slot store's
     * 319b growth machinery handles the runtime-add path. */
    if (n_inputs > 0) {
        b->input_slot_ids   = malloc((size_t)n_inputs * sizeof(int));
        b->input_slot_modes = malloc((size_t)n_inputs * sizeof(int));
        if (!b->input_slot_ids || !b->input_slot_modes) {
            free(b->input_slot_ids);
            free(b->input_slot_modes);
            free((void *)b->id);
            free((void *)b->lang);
            free((void *)b->ref);
            free((void *)b->fn);
            free(inputs);
            free(b);
            json_arena_destroy(arena);
            return set_err(err_out, "create_box: out of memory (slot arrays)");
        }
        for (int i = 0; i < n_inputs; i++) {
            int sid = slot_alloc(slots, b->output_capacity, 1, 0);
            if (sid < 0) {
                /* On failure, free what we've allocated; the slot
                 * store's earlier-allocated slots stay around until
                 * graph teardown (no slot_free in this slice — 315
                 * territory). */
                free(b->input_slot_ids);
                free(b->input_slot_modes);
                free((void *)b->id);
                free((void *)b->lang);
                free((void *)b->ref);
                free((void *)b->fn);
                free(inputs);
                free(b);
                json_arena_destroy(arena);
                return set_err(err_out, "create_box: slot_alloc failed for input %d", i);
            }
            b->input_slot_ids[i]   = sid;
            b->input_slot_modes[i] = 0;  /* SLOT_MODE_PEEK */
            /* Issue 311 + 319: log each runtime slot allocation so
             * the JSONL transcript shows when the new box's input
             * slots came into existence. Opt-in via SORAMECH_LOG_SLOTS
             * (the queue silently ignores the call when no writer
             * was opened with that env var). */
            event_queue_slot_alloc(tls_events, rt_now_secs(), sid,
                                   b->output_capacity, 1,
                                   b->id, b->inputs[i].name);
        }
    }

    /* Append the box to the graph. */
    int new_idx = graph_add_box(g, b);
    if (new_idx < 0) {
        /* Free the box record but not the slots (no slot_free in
         * this slice). */
        free(b->input_slot_ids);
        free(b->input_slot_modes);
        free((void *)b->id);
        free((void *)b->lang);
        free((void *)b->ref);
        free((void *)b->fn);
        free(inputs);
        free(b);
        json_arena_destroy(arena);
        return set_err(err_out, "create_box: graph_add_box failed");
    }

    /* Fill the caller's out_id buffer. */
    snprintf(out_id, out_id_size, "%s", b->id);

    /* Issue 311 + 319: the JSONL transcript records every runtime
     * graph mutation so the trace from create_box → push fan-out is
     * inspectable after the fact. The event fires AFTER graph_add_box
     * so the publish is already visible to any downstream walker. */
    event_queue_box_create(tls_events, rt_now_secs(),
                           b->id, "call", b->lang, b->ref, b->fn);

    json_arena_destroy(arena);
    return 0;
}
/* }}} */

/* {{{ runtime_connect() */
int runtime_connect(const char *conn_json, int conn_len, char **err_out)
{
    if (err_out) *err_out = NULL;

    struct graph *g = runtime_active_graph();
    if (!g) return set_err(err_out, "connect: no active runtime context");
    if (!conn_json || conn_len <= 0) {
        return set_err(err_out, "connect: connection JSON is empty");
    }

    json_arena_t *arena = json_arena_create();
    if (!arena) return set_err(err_out, "connect: out of memory (arena)");

    char *conn_dup = malloc((size_t)conn_len + 1);
    if (!conn_dup) { json_arena_destroy(arena); return set_err(err_out, "connect: out of memory"); }
    memcpy(conn_dup, conn_json, (size_t)conn_len);
    conn_dup[conn_len] = '\0';
    int json_err_off = 0;
    const char *json_err_msg = NULL;
    json_node_t *root = json_parse(arena, conn_dup, &json_err_off, &json_err_msg);
    free(conn_dup);
    if (!root || json_kind(root) != JSON_OBJECT) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: JSON is not an object (%s)",
                       json_err_msg ? json_err_msg : "parse failed");
    }

    json_node_t *from_n = json_object_get(root, "from_box");
    json_node_t *to_n   = json_object_get(root, "to_box");
    json_node_t *port_n = json_object_get(root, "to_input");
    json_node_t *branch_n = json_object_get(root, "from_branch");
    if (!from_n || json_kind(from_n) != JSON_STRING ||
        !to_n   || json_kind(to_n)   != JSON_STRING ||
        !port_n || json_kind(port_n) != JSON_STRING) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: from_box / to_box / to_input "
                                "strings required");
    }

    const char *from_id = json_string_value(from_n);
    const char *to_id   = json_string_value(to_n);
    const char *port    = json_string_value(port_n);

    int from_idx = graph_box_index(g, from_id);
    int to_idx   = graph_box_index(g, to_id);
    if (from_idx < 0) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: no box named '%s'", from_id);
    }
    if (to_idx < 0) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: no box named '%s'", to_id);
    }

    const box_t *to_box_c = graph_box(g, to_idx);
    if (!to_box_c) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: graph_box('%s') returned NULL", to_id);
    }

    /* Resolve the to_input name to an index in the destination's
     * inputs[] array. */
    int port_idx = -1;
    for (int i = 0; i < to_box_c->n_inputs; i++) {
        if (to_box_c->inputs[i].name &&
            strcmp(to_box_c->inputs[i].name, port) == 0) {
            port_idx = i;
            break;
        }
    }
    if (port_idx < 0) {
        json_arena_destroy(arena);
        return set_err(err_out, "connect: box '%s' has no input port '%s'",
                       to_id, port);
    }

    /* Build the connection record. Strings are malloc'd so they
     * outlive this parse arena. */
    connection_t c = {0};
    /* The to_box / to_input strings are stored on the connection
     * for diagnostics; cast away the const since we own these
     * malloc'd copies. */
    char *to_box_copy   = malloc(strlen(to_id) + 1); strcpy(to_box_copy, to_id);
    char *to_input_copy = malloc(strlen(port)  + 1); strcpy(to_input_copy, port);
    c.to_box       = to_box_copy;
    c.to_input     = to_input_copy;
    c.from_branch  = NULL;
    if (branch_n && json_kind(branch_n) == JSON_STRING) {
        const char *br = json_string_value(branch_n);
        char *br_copy = malloc(strlen(br) + 1); strcpy(br_copy, br);
        c.from_branch = br_copy;
    }
    c.to_box_idx   = to_idx;
    c.to_input_idx = port_idx;

    /* graph_box returns const; we need a non-const pointer for
     * box_add_connection. Re-fetch via the boxes array (we just
     * verified from_idx is valid). */
    box_t **boxes = (box_t **)((void *)graph_box(g, 0) ? NULL : NULL);
    /* The above trick is wrong — graph_box returns box_t* directly.
     * The const is in our function's signature, not in the storage.
     * Just cast. */
    box_t *from_box = (box_t *)graph_box(g, from_idx);
    if (!from_box) {
        free((void *)c.to_box);
        free((void *)c.to_input);
        free((void *)c.from_branch);
        json_arena_destroy(arena);
        return set_err(err_out, "connect: graph_box('%s') returned NULL", from_id);
    }
    (void)boxes;

    int rc = box_add_connection(g, from_box, c);
    if (rc != 0) {
        json_arena_destroy(arena);
        free((void *)c.to_box);
        free((void *)c.to_input);
        free((void *)c.from_branch);
        return set_err(err_out, "connect: box_add_connection failed");
    }
    /* Issue 311 + 319: log the runtime wire so the JSONL transcript
     * records the connect → push order. Emitted AFTER the
     * connection is appended (and atomic-published) so any reader
     * acting on this event sees a consistent producer record. */
    event_queue_wire_add(tls_events, rt_now_secs(),
                         from_id,
                         c.from_branch,
                         to_id,
                         port);
    json_arena_destroy(arena);
    return 0;
}
/* }}} */
