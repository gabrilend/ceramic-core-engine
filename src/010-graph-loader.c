/* src/010-graph-loader.c — C graph loader, implementation.
 *
 * Pipeline: open the map directory; parse meta.json; walk every
 * .json file under boxes/; parse each box; flatten input
 * declarations and outgoing connections per box; validate
 * per-box schema.
 *
 * Memory model: a single json_arena_t owned by the graph_t holds
 * every string and every parsed JSON tree across every file in the
 * map. Box / connection / input arrays are plain malloc — fixed
 * size after the load completes. graph_destroy frees both.
 *
 * Designed in issue 305. All seven phases now ship here: directory
 * walk, parse, schema, topology resolution + cycle rejection, slot
 * size-class enumeration, entry-box detection, language
 * enumeration. The reverse-scan over producers feeding a given
 * (box, port) is factored into scan_input_feeders so the
 * native-edge, large-value, slot-sizing, size-class, and entry-box
 * passes share one definition of "feeder".
 */

#include "010-graph-loader.h"
#include "009-slot-store.h"
#include "011-spec-registry.h"
#include "json.h"

#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Graph struct
 *
 * Box storage uses pointers-to-records so box addresses stay stable
 * across runtime growth (issue 319d). Each box_t is its own malloc;
 * `boxes` is an array of pointers indexed by box id. Growing the
 * pointer array uses copy-and-publish — allocate a new bigger
 * pointer array, memcpy the old pointers in, atomic-store the new
 * array, and stash the old array on stale_box_arrs so any in-flight
 * reader walking the old array completes safely.
 *
 * The hot read path (graph_box, graph_n_boxes) atomic-loads the
 * boxes pointer and n_boxes count; no mutex held. Mutations
 * (graph_add_box for runtime additions, graph_load for initial
 * population) take graph_mu.
 */
struct stale_arr {
    void              *ptr;
    struct stale_arr  *next;
};

/* Per-graph list of heap-allocated strings the loader produced
 * (e.g. issue 248 prefix-renamed sub-map ids). Arena strings come
 * from JSON parses; these don't, so they need their own free pass. */
struct owned_str {
    char              *str;
    struct owned_str  *next;
};

struct graph {
    json_arena_t *arena;

    const char   *name;
    const char   *description;
    const char   *entry_box_id;
    char         *map_dir;     /* malloc'd; remembered for dispatch (read boxes,
                                 * source-file resolution). */

    _Atomic(box_t **) boxes;        /* array of box pointers; stable per box */
    _Atomic uint32_t  n_boxes;      /* number of valid entries in `boxes` */
    _Atomic uint32_t  box_capacity; /* allocated length of `boxes`        */
    pthread_mutex_t   graph_mu;     /* serializes runtime additions       */
    struct stale_arr *stale_box_arrs;
    struct owned_str *owned_strs;   /* per-graph heap strings (issue 248) */

    int           n_languages;
    const char  **languages;   /* arena-owned strings */

    int           n_entry_boxes;
    int          *entry_box_ids;   /* indices into boxes[] */

    int           n_size_classes;
    int          *size_classes;    /* distinct input-slot cell widths */
};
/* }}} */

/* {{{ err_fmt() — malloc'd diagnostic string */
__attribute__((format(printf, 1, 2)))
static char *err_fmt(const char *fmt, ...)
{
    char buf[1024];
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

/* {{{ ends_with() — true iff `s` ends with `suffix` */
static int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    if (ls < lf) return 0;
    return memcmp(s + ls - lf, suffix, lf) == 0;
}
/* }}} */

/* {{{ input_feeders_t / scan_input_feeders() — one reverse-scan, four facts
 *
 * Three earlier loops in this file walked all boxes looking for
 * connections ending at a given (dst_box, port) — the same scan,
 * collecting different facts. They are now one helper. dst_lang may
 * be NULL when the caller doesn't care about the native count. */
typedef struct {
    int n_feeders;     /* total connections ending at (dst_box, port)        */
    int n_native;      /* feeders that are call boxes sharing dst_lang       */
    int n_non_read;    /* feeders that aren't BOX_READ value sources         */
    int has_variable;  /* any feeder declares output_capacity == 0           */
    int max_capacity;  /* max declared output_capacity across feeders (>= 0) */
} input_feeders_t;

static input_feeders_t scan_input_feeders(const struct graph *g,
                                          int dst_box, int port_idx,
                                          const char *dst_lang)
{
    input_feeders_t f = {0};
    for (int p = 0; p < g->n_boxes; p++) {
        const box_t *prod = g->boxes[p];
        for (int k = 0; k < prod->n_connections; k++) {
            const connection_t *c = &prod->connections[k];
            if (c->to_box_idx   != dst_box)  continue;
            if (c->to_input_idx != port_idx) continue;
            f.n_feeders++;
            if (prod->kind != BOX_READ) f.n_non_read++;
            if (dst_lang && prod->kind == BOX_CALL && prod->lang &&
                strcmp(prod->lang, dst_lang) == 0) {
                f.n_native++;
            }
            if (prod->output_capacity == 0) {
                f.has_variable = 1;
            } else if (prod->output_capacity > f.max_capacity) {
                f.max_capacity = prod->output_capacity;
            }
        }
    }
    return f;
}
/* }}} */

/* {{{ parse_routing() — translate JSON routing object into routing_t */
static int parse_routing(const json_node_t *r, routing_t *out,
                         const char *box_id, char **err)
{
    if (!r) {
        *err = err_fmt("box '%s': missing 'routing' field", box_id);
        return -1;
    }
    if (json_kind((json_node_t *)r) != JSON_OBJECT) {
        *err = err_fmt("box '%s': 'routing' is not an object", box_id);
        return -1;
    }
    json_node_t *kind_n = json_object_get(r, "kind");
    if (!kind_n || json_kind(kind_n) != JSON_STRING) {
        *err = err_fmt("box '%s': routing.kind missing or not a string", box_id);
        return -1;
    }
    const char *kind = json_string_value(kind_n);

    memset(out, 0, sizeof *out);

    if (strcmp(kind, "plain") == 0) {
        out->kind = ROUTING_PLAIN;
        out->n_outputs = 1;
        return 0;
    }
    if (strcmp(kind, "comparator") == 0) {
        out->kind = ROUTING_COMPARATOR;
        out->n_outputs = 3;          /* lt/eq/gt for legacy form */
        out->n_thresholds = 0;
        out->thresholds   = NULL;
        /* Issue 243 — preferred shape: a non-empty `thresholds`
         * array carves N+1 bands. Falls back to the single
         * `comparand` (legacy lt/eq/gt) when thresholds is absent.
         * If both are present, thresholds wins and comparand is
         * ignored. */
        json_node_t *t_arr = json_object_get(r, "thresholds");
        if (t_arr) {
            if (json_kind(t_arr) != JSON_ARRAY) {
                *err = err_fmt("box '%s': comparator 'thresholds' must be an array", box_id);
                return -1;
            }
            int nt = json_array_size(t_arr);
            if (nt == 0) {
                *err = err_fmt("box '%s': comparator 'thresholds' must be non-empty", box_id);
                return -1;
            }
            double *arr = malloc((size_t)nt * sizeof(double));
            if (!arr) { *err = err_fmt("out of memory"); return -1; }
            double prev = 0.0;
            for (int i = 0; i < nt; i++) {
                json_node_t *e = json_array_at(t_arr, i);
                if (!e || json_kind(e) != JSON_NUMBER) {
                    free(arr);
                    *err = err_fmt("box '%s': comparator thresholds[%d] is not a number",
                                   box_id, i);
                    return -1;
                }
                double v = json_number_value(e);
                if (i > 0 && v < prev) {
                    free(arr);
                    *err = err_fmt("box '%s': comparator thresholds must be non-decreasing "
                                   "(thresholds[%d]=%g < thresholds[%d]=%g)",
                                   box_id, i, v, i - 1, prev);
                    return -1;
                }
                arr[i] = v;
                prev   = v;
            }
            out->thresholds   = arr;
            out->n_thresholds = nt;
            out->n_outputs    = nt + 1;
            return 0;
        }
        json_node_t *c = json_object_get(r, "comparand");
        if (!c) {
            *err = err_fmt("box '%s': comparator routing missing 'comparand' or 'thresholds'", box_id);
            return -1;
        }
        if (json_kind(c) == JSON_NUMBER) {
            out->comparand = json_number_value(c);
        } else if (json_kind(c) == JSON_STRING) {
            /* Issue 233 example shows comparand as a string;
             * tolerate either form, parse the string as a double. */
            const char *s = json_string_value(c);
            char *endp = NULL;
            out->comparand = strtod(s, &endp);
            if (endp == s) {
                *err = err_fmt("box '%s': comparand '%s' is not a number", box_id, s);
                return -1;
            }
        } else {
            *err = err_fmt("box '%s': comparand must be number or string", box_id);
            return -1;
        }
        return 0;
    }
    if (strcmp(kind, "iterator") == 0) {
        out->kind = ROUTING_ITERATOR;
        json_node_t *n = json_object_get(r, "n_outputs");
        if (!n || json_kind(n) != JSON_NUMBER) {
            *err = err_fmt("box '%s': iterator routing missing 'n_outputs'", box_id);
            return -1;
        }
        out->n_outputs = (int)json_number_value(n);
        if (out->n_outputs < 1) {
            *err = err_fmt("box '%s': iterator n_outputs must be >= 1", box_id);
            return -1;
        }
        return 0;
    }
    if (strcmp(kind, "randomizer") == 0) {
        out->kind = ROUTING_RANDOMIZER;
        json_node_t *n = json_object_get(r, "n_outputs");
        if (!n || json_kind(n) != JSON_NUMBER) {
            *err = err_fmt("box '%s': randomizer routing missing 'n_outputs'", box_id);
            return -1;
        }
        out->n_outputs = (int)json_number_value(n);
        if (out->n_outputs < 1) {
            *err = err_fmt("box '%s': randomizer n_outputs must be >= 1", box_id);
            return -1;
        }
        return 0;
    }
    if (strcmp(kind, "weighted") == 0) {
        out->kind = ROUTING_WEIGHTED;
        json_node_t *w = json_object_get(r, "weights");
        if (!w || json_kind(w) != JSON_ARRAY) {
            *err = err_fmt("box '%s': weighted routing missing 'weights' array", box_id);
            return -1;
        }
        int n = json_array_size(w);
        if (n < 1) {
            *err = err_fmt("box '%s': weighted routing 'weights' must be non-empty", box_id);
            return -1;
        }
        out->n_outputs = n;
        /* The graph loader's arena is not exposed to this function;
         * stash a heap-allocated copy and recover it in graph_destroy
         * via a per-box free pass — wait, simpler: allocate via the
         * arena. We'll use the JSON arena owned by the graph. The
         * routing_t already holds a const pointer; cast away const
         * at allocation time. */
        /* Use plain malloc for simplicity; the per-box free in
         * graph_destroy frees it via a new field below. To avoid
         * widening box_t for one field, leak through the graph's
         * arena would be neater — but the arena pointer isn't
         * threaded through. Just malloc + free in graph_destroy.
         *
         * Actually the cleanest path: the JSON parse tree's array
         * already lives in the arena. Walk it and read values
         * directly at dispatch time. But routing_t wants double*
         * for fast access. Compromise: copy into a malloc'd buffer
         * here, free in graph_destroy. */
        double *vals = malloc((size_t)n * sizeof(double));
        if (!vals) { *err = err_fmt("out of memory"); return -1; }
        for (int i = 0; i < n; i++) {
            json_node_t *e = json_array_at(w, i);
            if (!e || json_kind(e) != JSON_NUMBER) {
                free(vals);
                *err = err_fmt("box '%s': weights[%d] is not a number", box_id, i);
                return -1;
            }
            vals[i] = json_number_value(e);
            if (vals[i] < 0) {
                free(vals);
                *err = err_fmt("box '%s': weights[%d] is negative", box_id, i);
                return -1;
            }
        }
        out->weights = vals;
        return 0;
    }

    if (strcmp(kind, "distributor") == 0) {
        /* Round-robin fan-out: each firing routes to the next branch
         * index (mod n_outputs). The dispatch layer uses an atomic
         * counter slot the same way it does for iterator routing. */
        out->kind = ROUTING_DISTRIBUTOR;
        json_node_t *n = json_object_get(r, "n_outputs");
        if (!n || json_kind(n) != JSON_NUMBER) {
            *err = err_fmt("box '%s': distributor routing missing 'n_outputs'", box_id);
            return -1;
        }
        out->n_outputs = (int)json_number_value(n);
        if (out->n_outputs < 1) {
            *err = err_fmt("box '%s': distributor n_outputs must be >= 1", box_id);
            return -1;
        }
        return 0;
    }
    if (strcmp(kind, "nonlinearity") == 0) {
        /* Issue 250 — value-transforming routing. Emits a scoring
         * response (the smoothed [0,1] or [-1,1] number) on a
         * single output port. The Lua schema already validated the
         * shape string and number types; here we just thread the
         * fields onto the routing struct.
         *
         * Presence vs absence of min / max / midpoint is the
         * mode-flag for each side — supplied means "fixed; clamp
         * past it"; absent means "track it via the atomic running
         * cells, decay toward midpoint via EMA on every fire." */
        out->kind = ROUTING_NONLINEARITY;
        out->n_outputs = 1;
        out->nl_shape = NL_CONFIDENCE;     /* default if shape missing/unknown */
        out->nl_steepness = 1.0;
        out->nl_min_is_fixed = 0;
        out->nl_max_is_fixed = 0;
        out->nl_mid_is_fixed = 0;
        out->nl_fixed_min = 0.0;
        out->nl_fixed_max = 1.0;
        out->nl_fixed_mid = 0.0;
        /* Running cells initialised so the first observed value
         * always widens both sides. Using sentinel bit patterns
         * (+inf for min, -inf for max) makes the first compare
         * deterministic. */
        union { double d; uint64_t u; } pos_inf, neg_inf;
        pos_inf.d =  (double)INFINITY;
        neg_inf.d = -(double)INFINITY;
        atomic_init(&out->nl_running_min, pos_inf.u);
        atomic_init(&out->nl_running_max, neg_inf.u);

        json_node_t *sh = json_object_get(r, "shape");
        if (!sh || json_kind(sh) != JSON_STRING) {
            *err = err_fmt("box '%s': nonlinearity routing requires "
                           "'shape' string (decision / confidence / calibration)",
                           box_id);
            return -1;
        }
        const char *s = json_string_value(sh);
        if      (strcmp(s, "confidence")  == 0) out->nl_shape = NL_CONFIDENCE;
        else if (strcmp(s, "decision")    == 0) out->nl_shape = NL_DECISION;
        else if (strcmp(s, "calibration") == 0) out->nl_shape = NL_CALIBRATION;
        else {
            *err = err_fmt("box '%s': nonlinearity 'shape' = '%s' must be "
                           "'decision' / 'confidence' / 'calibration'", box_id, s);
            return -1;
        }

        json_node_t *mn = json_object_get(r, "min");
        if (mn && json_kind(mn) == JSON_NUMBER) {
            out->nl_fixed_min = json_number_value(mn);
            out->nl_min_is_fixed = 1;
        }
        json_node_t *mx = json_object_get(r, "max");
        if (mx && json_kind(mx) == JSON_NUMBER) {
            out->nl_fixed_max = json_number_value(mx);
            out->nl_max_is_fixed = 1;
        }
        if (out->nl_min_is_fixed && out->nl_max_is_fixed &&
            out->nl_fixed_min >= out->nl_fixed_max) {
            *err = err_fmt("box '%s': nonlinearity 'min' (%g) must be less than "
                           "'max' (%g)", box_id, out->nl_fixed_min, out->nl_fixed_max);
            return -1;
        }
        json_node_t *md = json_object_get(r, "midpoint");
        if (md && json_kind(md) == JSON_NUMBER) {
            out->nl_fixed_mid = json_number_value(md);
            out->nl_mid_is_fixed = 1;
        }
        json_node_t *kk = json_object_get(r, "k");
        if (kk && json_kind(kk) == JSON_NUMBER) {
            double v = json_number_value(kk);
            if (v <= 0.0) {
                *err = err_fmt("box '%s': nonlinearity 'k' (%g) must be positive",
                               box_id, v);
                return -1;
            }
            out->nl_steepness = v;
        }
        return 0;
    }

    *err = err_fmt("box '%s': unknown routing.kind '%s' "
                   "(supported: plain, comparator, iterator, "
                   "randomizer, weighted, distributor, nonlinearity)",
                   box_id, kind);
    return -1;
}
/* }}} */

/* {{{ parse_inputs() — input port array */
static int parse_inputs(json_node_t *node, box_t *box,
                        const char *box_id, char **err)
{
    if (!node) {
        box->n_inputs = 0;
        box->inputs = NULL;
        return 0;
    }
    if (json_kind(node) != JSON_ARRAY) {
        *err = err_fmt("box '%s': 'inputs' is not an array", box_id);
        return -1;
    }
    int n = json_array_size(node);
    box->n_inputs = n;
    if (n == 0) { box->inputs = NULL; return 0; }

    box->inputs = calloc((size_t)n, sizeof(input_decl_t));
    if (!box->inputs) {
        *err = err_fmt("out of memory");
        return -1;
    }

    for (int i = 0; i < n; i++) {
        json_node_t *item = json_array_at(node, i);
        if (!item || json_kind(item) != JSON_OBJECT) {
            *err = err_fmt("box '%s': input port %d is not an object", box_id, i);
            return -1;
        }
        json_node_t *name_n = json_object_get(item, "name");
        if (!name_n || json_kind(name_n) != JSON_STRING) {
            *err = err_fmt("box '%s': input port %d missing 'name'", box_id, i);
            return -1;
        }
        box->inputs[i].name = json_string_value(name_n);

        json_node_t *type_n = json_object_get(item, "type");
        box->inputs[i].type = (type_n && json_kind(type_n) == JSON_STRING)
                                ? json_string_value(type_n) : "string";

        /* `value`: only string literals supported in this iteration. */
        json_node_t *val_n = json_object_get(item, "value");
        box->inputs[i].literal = (val_n && json_kind(val_n) == JSON_STRING)
                                    ? json_string_value(val_n) : NULL;

        json_node_t *opt_n = json_object_get(item, "optional");
        box->inputs[i].optional = (opt_n && json_kind(opt_n) == JSON_BOOL)
                                    ? json_bool_value(opt_n) : 0;

        /* Issue 246: optional per-port custom translation shim. The
         * value is a file path relative to the map dir; absence
         * means "use the default decode for this port." */
        json_node_t *xlate_n = json_object_get(item, "custom_translation");
        box->inputs[i].custom_translation =
            (xlate_n && json_kind(xlate_n) == JSON_STRING)
                ? json_string_value(xlate_n) : NULL;
    }
    return 0;
}
/* }}} */

/* {{{ parse_outputs() — output port array, issue 248 BOX_MAP only
 *
 * Mirrors parse_inputs but with the smaller field set output ports
 * carry — name and type. No literal, no optional, no custom shim;
 * an output port is just a labelled wire endpoint on the encap
 * box. Absent or empty array means the encap exposes no outputs,
 * which is legal (a sub-map that's purely side-effecting from the
 * parent's perspective). */
static int parse_outputs(json_node_t *node, box_t *box,
                         const char *box_id, char **err)
{
    if (!node) {
        box->n_outputs = 0;
        box->outputs   = NULL;
        return 0;
    }
    if (json_kind(node) != JSON_ARRAY) {
        *err = err_fmt("box '%s': 'outputs' is not an array", box_id);
        return -1;
    }
    int n = json_array_size(node);
    box->n_outputs = n;
    if (n == 0) { box->outputs = NULL; return 0; }

    box->outputs = calloc((size_t)n, sizeof(input_decl_t));
    if (!box->outputs) {
        *err = err_fmt("out of memory");
        return -1;
    }
    for (int i = 0; i < n; i++) {
        json_node_t *item = json_array_at(node, i);
        if (!item || json_kind(item) != JSON_OBJECT) {
            *err = err_fmt("box '%s': output port %d is not an object",
                           box_id, i);
            return -1;
        }
        json_node_t *name_n = json_object_get(item, "name");
        if (!name_n || json_kind(name_n) != JSON_STRING) {
            *err = err_fmt("box '%s': output port %d missing 'name'",
                           box_id, i);
            return -1;
        }
        box->outputs[i].name = json_string_value(name_n);
        json_node_t *type_n = json_object_get(item, "type");
        box->outputs[i].type = (type_n && json_kind(type_n) == JSON_STRING)
                                 ? json_string_value(type_n) : "string";
    }
    return 0;
}
/* }}} */

/* {{{ parse_connections() — outgoing connection array */
static int parse_connections(json_node_t *node, box_t *box,
                             const char *box_id, char **err)
{
    if (!node) {
        box->n_connections = 0;
        box->connections = NULL;
        return 0;
    }
    if (json_kind(node) != JSON_ARRAY) {
        *err = err_fmt("box '%s': 'connections' is not an array", box_id);
        return -1;
    }
    int n = json_array_size(node);
    box->n_connections = n;
    if (n == 0) { box->connections = NULL; return 0; }

    box->connections = calloc((size_t)n, sizeof(connection_t));
    if (!box->connections) {
        *err = err_fmt("out of memory");
        return -1;
    }

    for (int i = 0; i < n; i++) {
        json_node_t *c = json_array_at(node, i);
        if (!c || json_kind(c) != JSON_OBJECT) {
            *err = err_fmt("box '%s': connection %d is not an object", box_id, i);
            return -1;
        }
        json_node_t *to_box_n   = json_object_get(c, "to_box");
        json_node_t *to_input_n = json_object_get(c, "to_input");
        if (!to_box_n || json_kind(to_box_n) != JSON_STRING) {
            *err = err_fmt("box '%s': connection %d missing 'to_box'", box_id, i);
            return -1;
        }
        if (!to_input_n || json_kind(to_input_n) != JSON_STRING) {
            *err = err_fmt("box '%s': connection %d missing 'to_input'", box_id, i);
            return -1;
        }
        box->connections[i].to_box   = json_string_value(to_box_n);
        box->connections[i].to_input = json_string_value(to_input_n);

        /* from_branch is optional: NULL for plain output ports, or
         * "lt"/"eq"/"gt" for comparator, or branch name for
         * iterator. The validation that the branch matches the
         * routing kind lives in phase 4 (topology). */
        json_node_t *fb = json_object_get(c, "from_branch");
        box->connections[i].from_branch =
            (fb && json_kind(fb) == JSON_STRING) ? json_string_value(fb) : NULL;
    }
    return 0;
}
/* }}} */

/* {{{ parse_external_binding() — optional `external` block on a box
 *
 * Issue 248. Present on `read` boxes inside a sub-map that should
 * source their value from the encapsulating map's input, and on
 * `write` boxes that should surface their value back to the
 * encapsulating map's output. Absent → external.kind = EXTERNAL_NONE
 * (the box behaves as a normal local read/write).
 *
 * Shape (any of the three is valid):
 *
 *   { "kind": "positional", "index": 0 }
 *   { "kind": "numbered",   "index": 7 }
 *   { "kind": "named",      "name":  "count" }
 */
static int parse_external_binding(json_node_t *ext_n, box_t *box,
                                  const char *box_id, char **err)
{
    box->external.kind  = EXTERNAL_NONE;
    box->external.index = -1;
    box->external.name  = NULL;

    if (!ext_n) return 0;
    if (json_kind(ext_n) != JSON_OBJECT) {
        *err = err_fmt("box '%s': 'external' is not an object", box_id);
        return -1;
    }
    json_node_t *kn = json_object_get(ext_n, "kind");
    if (!kn || json_kind(kn) != JSON_STRING) {
        *err = err_fmt("box '%s': external.kind missing or not a string", box_id);
        return -1;
    }
    const char *ks = json_string_value(kn);
    if (strcmp(ks, "positional") == 0) box->external.kind = EXTERNAL_POSITIONAL;
    else if (strcmp(ks, "numbered") == 0) box->external.kind = EXTERNAL_NUMBERED;
    else if (strcmp(ks, "named")    == 0) box->external.kind = EXTERNAL_NAMED;
    else {
        *err = err_fmt("box '%s': external.kind '%s' not recognized "
                       "(positional | numbered | named)", box_id, ks);
        return -1;
    }
    if (box->external.kind == EXTERNAL_NAMED) {
        json_node_t *nn = json_object_get(ext_n, "name");
        if (!nn || json_kind(nn) != JSON_STRING) {
            *err = err_fmt("box '%s': external.kind=named requires 'name' string",
                           box_id);
            return -1;
        }
        box->external.name = json_string_value(nn);
    } else {
        json_node_t *in = json_object_get(ext_n, "index");
        if (!in || json_kind(in) != JSON_NUMBER) {
            *err = err_fmt("box '%s': external.kind=%s requires 'index' number",
                           box_id, ks);
            return -1;
        }
        box->external.index = (int)json_number_value(in);
        if (box->external.index < 0) {
            *err = err_fmt("box '%s': external.index must be >= 0", box_id);
            return -1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ parse_box_file() — read one boxes/<id>.json into a box_t */
static int parse_box_file(graph_t *g, box_t *box,
                          const char *path, char **err)
{
    int err_line = 0;
    const char *err_msg = NULL;
    json_node_t *n = json_parse_file(g->arena, path, &err_line, &err_msg);
    if (!n) {
        *err = err_fmt("%s:%d: %s", path, err_line, err_msg ? err_msg : "parse error");
        return -1;
    }
    if (json_kind(n) != JSON_OBJECT) {
        *err = err_fmt("%s: top-level must be a JSON object", path);
        return -1;
    }

    memset(box, 0, sizeof *box);
    box->spec_idx          = -1;   /* unresolved until graph_attach_runtime */
    box->counter_slot_id   = -1;   /* only iterator boxes allocate one      */
    box->multi_spawn       = 0;    /* set by graph_attach_runtime           */

    /* id */
    json_node_t *id_n = json_object_get(n, "id");
    if (!id_n || json_kind(id_n) != JSON_STRING) {
        *err = err_fmt("%s: missing or non-string 'id'", path);
        return -1;
    }
    box->id = json_string_value(id_n);

    /* kind */
    json_node_t *kind_n = json_object_get(n, "kind");
    if (!kind_n || json_kind(kind_n) != JSON_STRING) {
        *err = err_fmt("%s: box '%s' missing 'kind'", path, box->id);
        return -1;
    }
    const char *kind_str = json_string_value(kind_n);
    if      (strcmp(kind_str, "call")       == 0) box->kind = BOX_CALL;
    else if (strcmp(kind_str, "read")       == 0) box->kind = BOX_READ;
    else if (strcmp(kind_str, "write")      == 0) box->kind = BOX_WRITE;
    else if (strcmp(kind_str, "create_box") == 0) box->kind = BOX_CREATE_BOX;
    else if (strcmp(kind_str, "connect")    == 0) box->kind = BOX_CONNECT;
    else if (strcmp(kind_str, "map")        == 0) box->kind = BOX_MAP;
    else {
        *err = err_fmt("%s: box '%s': unknown kind '%s'", path, box->id, kind_str);
        return -1;
    }

    /* Kind-specific required fields */
    if (box->kind == BOX_CALL) {
        json_node_t *ref_n = json_object_get(n, "ref");
        if (!ref_n || json_kind(ref_n) != JSON_STRING) {
            *err = err_fmt("%s: call box '%s' missing 'ref'", path, box->id);
            return -1;
        }
        box->ref = json_string_value(ref_n);

        json_node_t *fn_n = json_object_get(n, "fn");
        if (fn_n && json_kind(fn_n) == JSON_STRING) {
            box->fn = json_string_value(fn_n);
        }

        json_node_t *lang_n = json_object_get(n, "lang");
        if (lang_n && json_kind(lang_n) == JSON_STRING) {
            box->lang = json_string_value(lang_n);
        }

        json_node_t *r_n = json_object_get(n, "routing");
        if (parse_routing(r_n, &box->routing, box->id, err) != 0) return -1;

        json_node_t *oc_n = json_object_get(n, "output_capacity");
        if (oc_n && json_kind(oc_n) == JSON_NUMBER) {
            box->output_capacity = (int)json_number_value(oc_n);
        }

        /* Declared return type. Optional; absence means "raw bytes,
         * format up to the spec." Specs that care (the C spec's
         * JSON-output branch, for example) read this on the
         * output_native=0 path. */
        json_node_t *ret_n = json_object_get(n, "returns");
        if (ret_n && json_kind(ret_n) == JSON_STRING) {
            box->returns = json_string_value(ret_n);
        }

        /* Per-box compile hints (issue 307 items 3+4). All optional;
         * any spec that ignores them gets the default behaviour. The
         * arena owns the strings and arrays — graph_destroy doesn't
         * free them. */
        json_node_t *cf_n = json_object_get(n, "cflags");
        if (cf_n && json_kind(cf_n) == JSON_STRING) {
            box->cflags = json_string_value(cf_n);
        }
        /* The pointer array is malloc'd (and freed in graph_destroy);
         * the string contents are owned by the JSON arena and stay
         * alive for the run. Same pattern as the routing weights
         * array elsewhere in this file. */
        json_node_t *ll_n = json_object_get(n, "link_libs");
        if (ll_n && json_kind(ll_n) == JSON_ARRAY) {
            int sz = json_array_size(ll_n);
            if (sz > 0) {
                const char **arr = malloc((size_t)sz * sizeof(*arr));
                if (!arr) {
                    *err = err_fmt("%s: oom for link_libs", box->id);
                    return -1;
                }
                for (int i = 0; i < sz; i++) {
                    json_node_t *e = json_array_at(ll_n, i);
                    if (!e || json_kind(e) != JSON_STRING) {
                        free(arr);
                        *err = err_fmt("%s: link_libs[%d] is not a string",
                                       box->id, i);
                        return -1;
                    }
                    arr[i] = json_string_value(e);
                }
                box->link_libs   = arr;
                box->n_link_libs = sz;
            }
        }
        json_node_t *hd_n = json_object_get(n, "headers");
        if (hd_n && json_kind(hd_n) == JSON_ARRAY) {
            int sz = json_array_size(hd_n);
            if (sz > 0) {
                const char **arr = malloc((size_t)sz * sizeof(*arr));
                if (!arr) {
                    *err = err_fmt("%s: oom for headers", box->id);
                    return -1;
                }
                for (int i = 0; i < sz; i++) {
                    json_node_t *e = json_array_at(hd_n, i);
                    if (!e || json_kind(e) != JSON_STRING) {
                        free(arr);
                        *err = err_fmt("%s: headers[%d] is not a string",
                                       box->id, i);
                        return -1;
                    }
                    arr[i] = json_string_value(e);
                }
                box->headers   = arr;
                box->n_headers = sz;
            }
        }
    } else if (box->kind == BOX_READ) {
        /* read accepts EITHER an inline `value` literal OR a `path`
         * field. value takes precedence at run time (the canvas
         * hides the path input port when value is set, per issue
         * 229). When neither is set, the box may still be valid if
         * its `path` arrives at runtime via the input wire — that's
         * checked in dispatch, not here. */
        json_node_t *val_n  = json_object_get(n, "value");
        json_node_t *path_n = json_object_get(n, "path");
        if (val_n && json_kind(val_n) == JSON_STRING) {
            box->value = json_string_value(val_n);
        }
        if (path_n && json_kind(path_n) == JSON_STRING) {
            box->path = json_string_value(path_n);
        }
    } else if (box->kind == BOX_MAP) {
        /* Issue 248 — encapsulated sub-map. `ref` names the sub-map
         * directory relative to the parent map directory (or absolute).
         * The encapsulation pass loads it and splices its boxes in.
         *
         * `outputs` (optional) declares the encap box's output ports.
         * Parent wires leaving the encap carry `from_branch` set to
         * one of these names; the encapsulation pass uses the name to
         * find the externally-consumed write box inside the sub-map.
         * Absent / empty means the encap exposes no outputs, which is
         * legal — a sub-map that's purely side-effecting from the
         * parent's perspective. */
        json_node_t *ref_n = json_object_get(n, "ref");
        if (!ref_n || json_kind(ref_n) != JSON_STRING) {
            *err = err_fmt("%s: map box '%s' missing 'ref'", path, box->id);
            return -1;
        }
        box->ref = json_string_value(ref_n);
        if (parse_outputs(json_object_get(n, "outputs"),
                          box, box->id, err) != 0) return -1;
    }
    /* write boxes: nothing additional beyond inputs. */

    /* Issue 248 — externally-supplied (read) or externally-consumed (write)
     * binding. The block is meaningful on read and write boxes; tolerated
     * (parsed and stored) on others so the editor can carry the field
     * without the loader rejecting it. */
    if (parse_external_binding(json_object_get(n, "external"),
                               box, box->id, err) != 0) return -1;

    if (parse_inputs(json_object_get(n, "inputs"),
                     box, box->id, err) != 0) return -1;
    if (parse_connections(json_object_get(n, "connections"),
                          box, box->id, err) != 0) return -1;

    return 0;
}
/* }}} */

/* {{{ count_json_files_in_dir() — first pass, count .json entries */
static int count_json_files_in_dir(const char *dir, char **err)
{
    DIR *d = opendir(dir);
    if (!d) {
        *err = err_fmt("cannot open directory '%s'", dir);
        return -1;
    }
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (ends_with(e->d_name, ".json")) n++;
    }
    closedir(d);
    return n;
}
/* }}} */

/* {{{ load_meta() — parse map_dir/meta.json into the graph */
static int load_meta(graph_t *g, const char *map_dir, char **err)
{
    char path[4096];
    int w = snprintf(path, sizeof path, "%s/meta.json", map_dir);
    if (w < 0 || w >= (int)sizeof path) {
        *err = err_fmt("meta.json path too long");
        return -1;
    }

    int err_line = 0; const char *err_msg = NULL;
    json_node_t *meta = json_parse_file(g->arena, path, &err_line, &err_msg);
    if (!meta) {
        *err = err_fmt("%s:%d: %s", path, err_line, err_msg ? err_msg : "parse error");
        return -1;
    }
    if (json_kind(meta) != JSON_OBJECT) {
        *err = err_fmt("%s: top-level must be a JSON object", path);
        return -1;
    }

    json_node_t *name = json_object_get(meta, "name");
    if (!name || json_kind(name) != JSON_STRING) {
        *err = err_fmt("%s: missing 'name'", path);
        return -1;
    }
    g->name = json_string_value(name);

    json_node_t *entry = json_object_get(meta, "entry_box_id");
    if (entry && json_kind(entry) == JSON_STRING) {
        g->entry_box_id = json_string_value(entry);
    }

    json_node_t *desc = json_object_get(meta, "description");
    if (desc && json_kind(desc) == JSON_STRING) {
        g->description = json_string_value(desc);
    }

    return 0;
}
/* }}} */

/* {{{ load_boxes() — walk boxes/, parse every .json into g->boxes */
static int load_boxes(graph_t *g, const char *map_dir, char **err)
{
    char dir[4096];
    int w = snprintf(dir, sizeof dir, "%s/boxes", map_dir);
    if (w < 0 || w >= (int)sizeof dir) {
        *err = err_fmt("boxes path too long");
        return -1;
    }

    int n = count_json_files_in_dir(dir, err);
    if (n < 0) return -1;

    /* Allocate a pointer-array (the index that maps id → box record)
     * sized for the initial load count, with the same generous
     * capacity slot store uses (issue 319b) so runtime add_box calls
     * have room without immediate growth. Each box record is its own
     * malloc so its address stays stable across pointer-array
     * growth. */
    uint32_t initial_cap = (uint32_t)n;
    if (initial_cap < 16u) initial_cap = 16u;
    box_t **box_ptrs = calloc((size_t)initial_cap, sizeof(box_t *));
    if (!box_ptrs) { *err = err_fmt("out of memory"); return -1; }
    atomic_store_explicit(&g->boxes,        box_ptrs,    memory_order_release);
    atomic_store_explicit(&g->box_capacity, initial_cap, memory_order_release);
    atomic_store_explicit(&g->n_boxes,      (uint32_t)n, memory_order_release);
    if (n == 0) return 0;
    for (int i = 0; i < n; i++) {
        box_ptrs[i] = calloc(1, sizeof(box_t));
        if (!box_ptrs[i]) { *err = err_fmt("out of memory"); return -1; }
    }

    DIR *d = opendir(dir);
    if (!d) { *err = err_fmt("cannot open directory '%s'", dir); return -1; }

    int i = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')              continue;
        if (!ends_with(e->d_name, ".json"))   continue;

        char path[4096];
        int wp = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (wp < 0 || wp >= (int)sizeof path) {
            closedir(d);
            *err = err_fmt("box path too long");
            return -1;
        }

        if (parse_box_file(g, g->boxes[i], path, err) != 0) {
            closedir(d);
            return -1;
        }
        i++;
    }
    closedir(d);

    /* Validate id uniqueness (O(n^2) scan; n is small). */
    for (int j = 0; j < n; j++) {
        for (int k = j + 1; k < n; k++) {
            if (strcmp(g->boxes[j]->id, g->boxes[k]->id) == 0) {
                *err = err_fmt("duplicate box id '%s'", g->boxes[j]->id);
                return -1;
            }
        }
    }

    return 0;
}
/* }}} */

/* {{{ box_is_iterator() — true iff this is a call box whose routing
 * kind cuts cycles. Cycles passing through an iterator are
 * legitimate (the iterator's input queue eventually empties and
 * the cycle terminates); cycles that don't are deadlocks at
 * load time and must be rejected. */
static int box_is_iterator(const box_t *b)
{
    return b->kind == BOX_CALL && b->routing.kind == ROUTING_ITERATOR;
}
/* }}} */

/* {{{ dfs_cycle() — recursive DFS for non-iterator cycle detection */
/* colors: 0 = white (not seen), 1 = gray (on current stack),
 * 2 = black (finished). Returns 0 on success, -1 if a cycle was
 * found; *culprit_box is the box where the back-edge was detected. */
static int dfs_cycle(const graph_t *g, int box_id, char *color,
                     int *culprit_box)
{
    color[box_id] = 1;
    const box_t *src = g->boxes[box_id];

    /* Skip outgoing edges from iterator boxes. The iterator naturally
     * terminates on input-queue empty, so cycles that loop through
     * one are legal. Skipping is the cleanest way to express
     * "this edge doesn't propagate cycle reachability." */
    if (!box_is_iterator(src)) {
        for (int i = 0; i < src->n_connections; i++) {
            int dst = src->connections[i].to_box_idx;
            if (color[dst] == 1) { *culprit_box = box_id; return -1; }
            if (color[dst] == 0) {
                if (dfs_cycle(g, dst, color, culprit_box) != 0) return -1;
            }
        }
    }
    color[box_id] = 2;
    return 0;
}
/* }}} */

/* {{{ detect_cycles() — phase 4 part 2 */
static int detect_cycles(const graph_t *g, char **err)
{
    if (g->n_boxes <= 0) return 0;
    char *color = calloc((size_t)g->n_boxes, 1);
    if (!color) { *err = err_fmt("out of memory"); return -1; }

    for (int i = 0; i < g->n_boxes; i++) {
        if (color[i] != 0) continue;
        int culprit = -1;
        if (dfs_cycle(g, i, color, &culprit) != 0) {
            *err = err_fmt("non-iterator cycle detected (back-edge from "
                           "box '%s'); cycles must pass through an "
                           "iterator-routing box",
                           g->boxes[culprit]->id);
            free(color);
            return -1;
        }
    }
    free(color);
    return 0;
}
/* }}} */

/* {{{ cache_read_box_values() — issue 244
 *
 * Every BOX_READ's downstream value lives on its box record from
 * graph_load onward. Inline `value` literals are strdup'd; `path`
 * fields trigger a one-time file read whose bytes become the cache.
 * Once cached the read box never re-reads — its value is the run's
 * source of truth. */
static int cache_read_box_values(graph_t *g, char **err)
{
    for (int i = 0; i < g->n_boxes; i++) {
        box_t *b = g->boxes[i];
        if (b->kind != BOX_READ) continue;
        /* Issue 248 — externally-supplied read boxes are orphaned by
         * the encapsulation inlining pass (their downstream wires
         * were spliced through to the parent's producer). They have
         * no `value` / `path` because the value comes from outside,
         * and they never fire — skip the cache step so the "neither
         * value nor path" check below doesn't reject them. */
        if (b->external.kind != EXTERNAL_NONE) continue;

        if (b->value) {
            int n = (int)strlen(b->value);
            b->cached_value = malloc((size_t)n + 1);
            if (!b->cached_value) { *err = err_fmt("oom"); return -1; }
            memcpy(b->cached_value, b->value, (size_t)n);
            b->cached_value[n] = '\0';
            b->cached_size = n;
            continue;
        }
        if (!b->path) {
            /* No value, no path — error surfaces only if a consumer
             * actually tries to pull from this box, but flagging at
             * load is louder. Future iterations may allow `path`
             * inputs that arrive at run time; for now an unconfigured
             * read box is a load failure. */
            *err = err_fmt("read box '%s' has neither 'value' nor 'path'", b->id);
            return -1;
        }

        /* Resolve `path` relative to the map directory and slurp the
         * file. The graph holds onto these bytes for the life of
         * the run. */
        const char *base = g->map_dir;
        size_t blen = base ? strlen(base) : 0;
        size_t plen = strlen(b->path);
        char *full = malloc(blen + 1 + plen + 1);
        if (!full) { *err = err_fmt("oom"); return -1; }
        if (b->path[0] == '/') {
            memcpy(full, b->path, plen + 1);
        } else if (blen > 0) {
            memcpy(full, base, blen);
            full[blen] = '/';
            memcpy(full + blen + 1, b->path, plen + 1);
        } else {
            memcpy(full, b->path, plen + 1);
        }

        FILE *fp = fopen(full, "rb");
        if (!fp) {
            *err = err_fmt("read box '%s': cannot open '%s'", b->id, full);
            free(full);
            return -1;
        }
        free(full);
        fseek(fp, 0, SEEK_END);
        long len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (len < 0) {
            *err = err_fmt("read box '%s': ftell failed", b->id);
            fclose(fp);
            return -1;
        }
        b->cached_value = malloc((size_t)len + 1);
        if (!b->cached_value) {
            *err = err_fmt("oom");
            fclose(fp);
            return -1;
        }
        size_t got = fread(b->cached_value, 1, (size_t)len, fp);
        fclose(fp);
        b->cached_value[got] = '\0';
        b->cached_size = (int)got;
    }
    return 0;
}
/* }}} */

/* {{{ build_read_predecessor_lists() — issue 244
 *
 * For every call / write box, walk each input port and collect the
 * indices of any BOX_READ producers feeding it. The list lives on
 * the consumer's box record so dispatch can pull on demand without
 * a full graph scan. Reuses scan_input_feeders only indirectly —
 * we need the producer indices, not just counts, and the existing
 * helper doesn't expose them. The walk is small enough that an
 * inline loop is clearer than threading a callback through. */
static int build_read_predecessor_lists(graph_t *g, char **err)
{
    for (int i = 0; i < g->n_boxes; i++) {
        box_t *b = g->boxes[i];
        /* The kinds that run as tasks. BOX_CREATE_BOX and BOX_CONNECT
         * join the list with the box-kind path for runtime self-
         * construction (319 design-correction). BOX_READ never runs
         * as a task (it's pull-on-demand per issue 244). */
        if (b->kind != BOX_CALL && b->kind != BOX_WRITE &&
            b->kind != BOX_CREATE_BOX && b->kind != BOX_CONNECT) continue;
        if (b->n_inputs <= 0) continue;

        b->n_read_predecessors    = calloc((size_t)b->n_inputs, sizeof(int));
        b->read_predecessor_ids   = calloc((size_t)b->n_inputs, sizeof(int *));
        b->read_pred_counter_slot = malloc((size_t)b->n_inputs * sizeof(int));
        if (!b->n_read_predecessors || !b->read_predecessor_ids ||
            !b->read_pred_counter_slot) {
            *err = err_fmt("oom");
            return -1;
        }
        for (int p = 0; p < b->n_inputs; p++) b->read_pred_counter_slot[p] = -1;

        for (int port = 0; port < b->n_inputs; port++) {
            /* First pass: count. */
            int n = 0;
            for (int p = 0; p < g->n_boxes; p++) {
                const box_t *prod = g->boxes[p];
                if (prod->kind != BOX_READ) continue;
                for (int k = 0; k < prod->n_connections; k++) {
                    const connection_t *c = &prod->connections[k];
                    if (c->to_box_idx == i && c->to_input_idx == port) n++;
                }
            }
            if (n == 0) continue;

            int *ids = malloc((size_t)n * sizeof(int));
            if (!ids) { *err = err_fmt("oom"); return -1; }

            /* Second pass: fill. */
            int idx = 0;
            for (int p = 0; p < g->n_boxes; p++) {
                const box_t *prod = g->boxes[p];
                if (prod->kind != BOX_READ) continue;
                for (int k = 0; k < prod->n_connections; k++) {
                    const connection_t *c = &prod->connections[k];
                    if (c->to_box_idx == i && c->to_input_idx == port) {
                        ids[idx++] = p;
                    }
                }
            }
            b->n_read_predecessors[port]  = n;
            b->read_predecessor_ids[port] = ids;
        }
    }
    return 0;
}
/* }}} */

/* {{{ detect_entry_boxes() — phase 6
 *
 * Fills g->entry_box_ids with the indices of boxes the pool runner
 * should submit as initial tasks. A box qualifies when:
 *   - it is BOX_CALL or BOX_WRITE (the kinds that run as tasks —
 *     BOX_READ is a value source, never a task), AND
 *   - every non-optional input port has zero non-read feeders.
 *     Read boxes (literal-or-file value sources) don't count as
 *     upstream computation; a box fed only by reads can fire
 *     immediately on startup. Optional ports are ignored.
 *
 * A zero-input call box passes the loop vacuously and qualifies.
 *
 * The scan reuses scan_input_feeders so its definition of "feeder"
 * matches the native-edge and large-value passes — single source of
 * truth for "what feeds (box, port)". */
static int detect_entry_boxes(graph_t *g, char **err)
{
    if (g->n_boxes <= 0) {
        g->n_entry_boxes = 0;
        g->entry_box_ids = NULL;
        return 0;
    }
    int *ids = malloc((size_t)g->n_boxes * sizeof(int));
    if (!ids) { *err = err_fmt("out of memory"); return -1; }

    int count = 0;
    for (int i = 0; i < g->n_boxes; i++) {
        const box_t *b = g->boxes[i];
        /* The kinds that run as tasks. BOX_CREATE_BOX and BOX_CONNECT
         * join the list with the box-kind path for runtime self-
         * construction (319 design-correction). BOX_READ never runs
         * as a task (it's pull-on-demand per issue 244). */
        if (b->kind != BOX_CALL && b->kind != BOX_WRITE &&
            b->kind != BOX_CREATE_BOX && b->kind != BOX_CONNECT) continue;

        int qualifies = 1;
        for (int port = 0; port < b->n_inputs; port++) {
            if (b->inputs[port].optional) continue;
            input_feeders_t f = scan_input_feeders(g, i, port, NULL);
            if (f.n_non_read > 0) { qualifies = 0; break; }
        }
        if (qualifies) ids[count++] = i;
    }
    g->n_entry_boxes = count;
    g->entry_box_ids = (count > 0) ? ids : (free(ids), NULL);
    return 0;
}
/* }}} */

/* {{{ resolve_topology() — phase 4 (partial: endpoints only) */
/* Walks every outgoing connection on every box. Looks up `to_box`
 * as a box id and `to_input` as one of that box's declared input
 * port names; fills in the integer indices. Errors out with a
 * precise message on either miss. Cycle detection lands in a
 * later iteration. */
static int resolve_topology(graph_t *g, char **err)
{
    for (int i = 0; i < g->n_boxes; i++) {
        box_t *src = g->boxes[i];
        for (int j = 0; j < src->n_connections; j++) {
            connection_t *c = &src->connections[j];
            c->to_box_idx   = -1;
            c->to_input_idx = -1;

            int found_box = -1;
            for (int k = 0; k < g->n_boxes; k++) {
                if (strcmp(g->boxes[k]->id, c->to_box) == 0) { found_box = k; break; }
            }
            if (found_box < 0) {
                *err = err_fmt("box '%s' has a connection to nonexistent box '%s'",
                               src->id, c->to_box);
                return -1;
            }
            c->to_box_idx = found_box;

            const box_t *dst = g->boxes[found_box];
            int found_in = -1;
            for (int k = 0; k < dst->n_inputs; k++) {
                if (strcmp(dst->inputs[k].name, c->to_input) == 0) {
                    found_in = k; break;
                }
            }
            if (found_in < 0) {
                *err = err_fmt("box '%s' has a connection to box '%s' on "
                               "nonexistent input '%s'",
                               src->id, dst->id, c->to_input);
                return -1;
            }
            c->to_input_idx = found_in;
        }
    }
    return 0;
}
/* }}} */

/* {{{ track_owned_str() — remember a heap string to free at destroy */
static int track_owned_str(graph_t *g, char *s)
{
    struct owned_str *o = malloc(sizeof *o);
    if (!o) return -1;
    o->str  = s;
    o->next = g->owned_strs;
    g->owned_strs = o;
    return 0;
}
/* }}} */

/* {{{ find_input_port_index() — look up a named input port on a box */
static int find_input_port_index(const box_t *b, const char *port_name)
{
    for (int i = 0; i < b->n_inputs; i++) {
        if (strcmp(b->inputs[i].name, port_name) == 0) return i;
    }
    return -1;
}
/* }}} */

/* {{{ find_output_port_index() — encap output-port name → index
 *
 * Mirrors find_input_port_index but walks the encap's outputs[] list
 * instead. A NULL or empty `from_branch` is treated as "the first
 * output port" — a friendly default for the common single-output
 * case, so the editor can leave the field unset on encap boxes that
 * declare just one output. Returns -1 if no match and the encap has
 * more than one declared output (forcing the caller to surface a
 * precise error). */
static int find_output_port_index(const box_t *encap, const char *port_name)
{
    if (encap->n_outputs <= 0) return -1;
    if (!port_name || !port_name[0]) return 0;
    for (int i = 0; i < encap->n_outputs; i++) {
        if (strcmp(encap->outputs[i].name, port_name) == 0) return i;
    }
    return -1;
}
/* }}} */

/* {{{ find_ext_consumed_for_port() — locate the sub-map's write box
 *
 * Mirror of find_ext_supplied_for_port for the output side. Given the
 * encap box and one of its output port indices, scan the sub-map's
 * boxes for the externally-consumed write box that binds to it. The
 * matching rules are symmetric with the input side:
 *
 *   positional / numbered — W.external.index == port_idx
 *   named                 — W.external.name  == encap.outputs[port_idx].name
 *
 * Returns NULL if no write box matches — the caller turns that into
 * a precise load-time error so a sub-map missing an ext-consumed
 * port doesn't silently drop the wire. */
static box_t *find_ext_consumed_for_port(box_t **sub_boxes, int sub_n,
                                         const box_t *encap, int port_idx)
{
    if (port_idx < 0 || port_idx >= encap->n_outputs) return NULL;
    const char *port_name = encap->outputs[port_idx].name;
    for (int i = 0; i < sub_n; i++) {
        box_t *w = sub_boxes[i];
        if (w->kind != BOX_WRITE) continue;
        if (w->external.kind == EXTERNAL_NONE) continue;
        if (w->external.kind == EXTERNAL_NAMED) {
            if (w->external.name && port_name &&
                strcmp(w->external.name, port_name) == 0) return w;
        } else {
            if (w->external.index == port_idx) return w;
        }
    }
    return NULL;
}
/* }}} */

/* {{{ find_ext_supplied_for_port() — locate the sub-map's data box
 *
 * Given the encapsulating BOX_MAP and one of its input port indices,
 * scan the sub-map's boxes for the externally-supplied read box that
 * binds to it. Matching rules (issue 248):
 *
 *   positional / numbered — D.external.index == port_idx
 *   named                 — D.external.name  == encap.inputs[port_idx].name
 *
 * The two index-based kinds collapse to the same lookup here; the
 * editor preserves the user-meaningful distinction even though the
 * loader doesn't need it. */
static box_t *find_ext_supplied_for_port(box_t **sub_boxes, int sub_n,
                                         const box_t *encap, int port_idx)
{
    const char *port_name = encap->inputs[port_idx].name;
    for (int i = 0; i < sub_n; i++) {
        box_t *d = sub_boxes[i];
        if (d->kind != BOX_READ) continue;
        if (d->external.kind == EXTERNAL_NONE) continue;
        if (d->external.kind == EXTERNAL_NAMED) {
            if (d->external.name && port_name &&
                strcmp(d->external.name, port_name) == 0) return d;
        } else {
            if (d->external.index == port_idx) return d;
        }
    }
    return NULL;
}
/* }}} */

/* {{{ load_sub_map_boxes() — parse every .json under sub_dir/boxes
 *
 * Just the box layer — no meta.json, no topology resolution. Boxes go
 * into a fresh malloc'd array which the caller takes ownership of
 * (and either splices into the parent or frees on error). Strings are
 * arena-allocated in g->arena, same as the parent. */
static int load_sub_map_boxes(graph_t *g, const char *sub_dir,
                              box_t ***out_boxes, int *out_n, char **err)
{
    char boxes_dir[4096];
    int w = snprintf(boxes_dir, sizeof boxes_dir, "%s/boxes", sub_dir);
    if (w < 0 || w >= (int)sizeof boxes_dir) {
        *err = err_fmt("sub-map boxes path too long");
        return -1;
    }

    int n = count_json_files_in_dir(boxes_dir, err);
    if (n < 0) return -1;
    if (n == 0) { *out_boxes = NULL; *out_n = 0; return 0; }

    box_t **arr = calloc((size_t)n, sizeof(box_t *));
    if (!arr) { *err = err_fmt("out of memory"); return -1; }
    for (int i = 0; i < n; i++) {
        arr[i] = calloc(1, sizeof(box_t));
        if (!arr[i]) {
            for (int k = 0; k < i; k++) free(arr[k]);
            free(arr);
            *err = err_fmt("out of memory");
            return -1;
        }
    }

    DIR *d = opendir(boxes_dir);
    if (!d) {
        for (int k = 0; k < n; k++) free(arr[k]);
        free(arr);
        *err = err_fmt("cannot open directory '%s'", boxes_dir);
        return -1;
    }
    int i = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')            continue;
        if (!ends_with(e->d_name, ".json")) continue;
        char path[4096];
        int wp = snprintf(path, sizeof path, "%s/%s", boxes_dir, e->d_name);
        if (wp < 0 || wp >= (int)sizeof path) {
            closedir(d);
            for (int k = 0; k < n; k++) free(arr[k]);
            free(arr);
            *err = err_fmt("sub-map box path too long");
            return -1;
        }
        if (parse_box_file(g, arr[i], path, err) != 0) {
            closedir(d);
            for (int k = 0; k < n; k++) free(arr[k]);
            free(arr);
            return -1;
        }
        i++;
    }
    closedir(d);
    *out_boxes = arr;
    *out_n     = n;
    return 0;
}
/* }}} */

/* {{{ inline_one_encapsulation()
 *
 * Splice the sub-map at encap->ref into the parent graph, replacing
 * the BOX_MAP at encap_idx with its sub-map boxes. Both directions
 * of rewiring run here: parent wires landing on the encap's input
 * ports get redirected through ext-supplied read boxes, and parent
 * wires leaving the encap's output ports get appended to the matching
 * ext-consumed write box's connections array.
 *
 * Steps:
 *   1. Resolve sub_dir relative to parent's map_dir.
 *   2. Load sub-map's boxes into a temporary array (separate parse).
 *   3. Prefix-rename every sub-box id with `<encap_id>__<sub_id>`,
 *      then rewrite each sub-box's connections.to_box strings that
 *      reference sibling sub-boxes to the renamed form.
 *   4. INPUT-side: for each parent producer, walk its connections;
 *      any that target encap_id get replaced (1→N) by the
 *      corresponding ext-supplied read box's downstream targets.
 *   5. Append sub-boxes to parent's box array (grow capacity if
 *      needed) — appending must precede the output-side splice so
 *      the write boxes we're attaching wires to are graph-resident.
 *   6. OUTPUT-side: for each wire on the encap's own connections
 *      array, look up the from_branch in the encap's outputs[]
 *      declaration, find the matching ext-consumed write box, and
 *      append the wire to that write box's connections array.
 *   7. Orphan the encap box (clear its now-spliced connections; kind
 *      stays BOX_MAP so the dispatch's no-op case never confuses it
 *      for a live producer).
 */
static int inline_one_encapsulation(graph_t *g, int encap_idx, char **err)
{
    box_t **boxes = atomic_load_explicit(&g->boxes, memory_order_relaxed);
    box_t  *encap = boxes[encap_idx];

    if (!encap->ref) {
        *err = err_fmt("box '%s': BOX_MAP missing 'ref'", encap->id);
        return -1;
    }

    char sub_dir[4096];
    if (encap->ref[0] == '/') {
        int w = snprintf(sub_dir, sizeof sub_dir, "%s", encap->ref);
        if (w < 0 || w >= (int)sizeof sub_dir) {
            *err = err_fmt("encap '%s': sub-map path too long", encap->id);
            return -1;
        }
    } else {
        int w = snprintf(sub_dir, sizeof sub_dir, "%s/%s",
                         g->map_dir, encap->ref);
        if (w < 0 || w >= (int)sizeof sub_dir) {
            *err = err_fmt("encap '%s': sub-map path too long", encap->id);
            return -1;
        }
    }

    box_t **sub_boxes = NULL;
    int     sub_n     = 0;
    if (load_sub_map_boxes(g, sub_dir, &sub_boxes, &sub_n, err) != 0) {
        return -1;
    }

    /* Save the pre-rename ids so we can rewrite intra-sub-map
     * connection refs before clobbering the ids on the box records. */
    const char **old_ids = calloc((size_t)sub_n, sizeof(const char *));
    char       **new_ids = calloc((size_t)sub_n, sizeof(char *));
    if ((sub_n > 0 && !old_ids) || (sub_n > 0 && !new_ids)) {
        free(old_ids); free(new_ids);
        for (int k = 0; k < sub_n; k++) free(sub_boxes[k]);
        free(sub_boxes);
        *err = err_fmt("out of memory");
        return -1;
    }

    const char *prefix = encap->id;
    size_t      plen   = strlen(prefix);
    for (int i = 0; i < sub_n; i++) {
        old_ids[i] = sub_boxes[i]->id;
        size_t need = plen + 2 + strlen(old_ids[i]) + 1;
        char *nid = malloc(need);
        if (!nid) {
            for (int k = 0; k < i; k++) free(new_ids[k]);
            free(old_ids); free(new_ids);
            for (int k = 0; k < sub_n; k++) free(sub_boxes[k]);
            free(sub_boxes);
            *err = err_fmt("out of memory");
            return -1;
        }
        snprintf(nid, need, "%s__%s", prefix, old_ids[i]);
        new_ids[i] = nid;
    }

    /* Rewrite intra-sub-map connection refs to use the renamed ids.
     * Connections that already target an outside-the-sub-map id (which
     * shouldn't happen at this layer, but guard anyway) are left alone. */
    for (int i = 0; i < sub_n; i++) {
        int n_c = atomic_load_explicit(&sub_boxes[i]->n_connections,
                                       memory_order_relaxed);
        connection_t *cs =
            atomic_load_explicit(&sub_boxes[i]->connections,
                                 memory_order_relaxed);
        for (int k = 0; k < n_c; k++) {
            for (int j = 0; j < sub_n; j++) {
                if (strcmp(cs[k].to_box, old_ids[j]) == 0) {
                    cs[k].to_box = new_ids[j];
                    break;
                }
            }
        }
    }

    /* Adopt the renamed ids onto the sub-box records. From this point
     * on, the sub-boxes look like normal parent boxes with namespaced
     * ids. The malloc'd new_ids strings move into the graph's
     * owned-strings list. */
    for (int i = 0; i < sub_n; i++) {
        sub_boxes[i]->id = new_ids[i];
        if (track_owned_str(g, new_ids[i]) != 0) {
            /* Leak the rest of new_ids on this rare failure; we're
             * about to error out anyway. */
            free(old_ids); free(new_ids);
            *err = err_fmt("out of memory");
            return -1;
        }
    }
    free(new_ids);    /* the strings live on; only the temp array goes */

    /* INPUT-side rewiring. Each parent producer's connection that
     * targets (encap_id, port) becomes one-or-more new connections
     * targeting the matching ext-supplied data box's downstreams. */
    uint32_t cur_n = atomic_load_explicit(&g->n_boxes, memory_order_relaxed);
    for (uint32_t p = 0; p < cur_n; p++) {
        box_t *prod = g->boxes[p];
        if (prod == encap) continue;

        int n_c = atomic_load_explicit(&prod->n_connections,
                                       memory_order_relaxed);
        if (n_c == 0) continue;
        connection_t *cs =
            atomic_load_explicit(&prod->connections, memory_order_relaxed);

        /* First pass: compute the new array's size. Each connection
         * to encap explodes to N entries where N = matched data box's
         * out-degree (potentially 0; that prunes the wire). */
        int new_cap = 0;
        int touches_encap = 0;
        for (int k = 0; k < n_c; k++) {
            if (strcmp(cs[k].to_box, encap->id) != 0) {
                new_cap += 1;
                continue;
            }
            touches_encap = 1;
            int port_idx = find_input_port_index(encap, cs[k].to_input);
            if (port_idx < 0) {
                *err = err_fmt("encap '%s': producer '%s' wires to unknown "
                               "port '%s'",
                               encap->id, prod->id, cs[k].to_input);
                free(old_ids);
                return -1;
            }
            box_t *D = find_ext_supplied_for_port(sub_boxes, sub_n,
                                                  encap, port_idx);
            if (!D) {
                *err = err_fmt("encap '%s': port '%s' has no matching "
                               "externally-supplied data box in sub-map",
                               encap->id, cs[k].to_input);
                free(old_ids);
                return -1;
            }
            new_cap += atomic_load_explicit(&D->n_connections,
                                            memory_order_relaxed);
        }
        if (!touches_encap) continue;

        /* Second pass: build the new array. */
        connection_t *new_cs =
            (new_cap > 0) ? calloc((size_t)new_cap, sizeof(connection_t)) : NULL;
        if (new_cap > 0 && !new_cs) {
            free(old_ids);
            *err = err_fmt("out of memory");
            return -1;
        }
        int new_n = 0;
        for (int k = 0; k < n_c; k++) {
            if (strcmp(cs[k].to_box, encap->id) != 0) {
                new_cs[new_n] = cs[k];
                new_cs[new_n].to_box_idx   = -1;
                new_cs[new_n].to_input_idx = -1;
                new_n++;
                continue;
            }
            int port_idx = find_input_port_index(encap, cs[k].to_input);
            box_t *D = find_ext_supplied_for_port(sub_boxes, sub_n,
                                                  encap, port_idx);
            int dn = atomic_load_explicit(&D->n_connections,
                                          memory_order_relaxed);
            connection_t *dc =
                atomic_load_explicit(&D->connections, memory_order_relaxed);
            for (int j = 0; j < dn; j++) {
                new_cs[new_n].to_box      = dc[j].to_box;
                new_cs[new_n].to_input    = dc[j].to_input;
                new_cs[new_n].from_branch = cs[k].from_branch;
                new_cs[new_n].to_box_idx   = -1;
                new_cs[new_n].to_input_idx = -1;
                new_n++;
            }
        }

        free(cs);
        atomic_store_explicit(&prod->connections, new_cs,
                              memory_order_release);
        atomic_store_explicit(&prod->n_connections, new_n,
                              memory_order_release);
    }
    free(old_ids);

    /* Orphan each ext-supplied data box: its outgoing connections
     * (now spliced into the parent producer's array) must be cleared
     * so build_read_predecessor_lists doesn't double-count them as
     * a feeder of the same consumer the parent producer is also
     * feeding. The string pointers inside the original connection
     * entries are arena-owned and remain valid for the parent's
     * copies; only the array itself goes. */
    for (int i = 0; i < sub_n; i++) {
        box_t *d = sub_boxes[i];
        if (d->kind != BOX_READ) continue;
        if (d->external.kind == EXTERNAL_NONE) continue;
        connection_t *dc =
            atomic_load_explicit(&d->connections, memory_order_relaxed);
        if (dc) free(dc);
        atomic_store_explicit(&d->connections, NULL, memory_order_release);
        atomic_store_explicit(&d->n_connections, 0, memory_order_release);
    }

    /* Grow the parent's box pointer array if needed and append the
     * sub-boxes. No atomic publish ordering needed at load time
     * (single-threaded), but we keep the same shape as graph_add_box
     * for consistency. */
    uint32_t cap = atomic_load_explicit(&g->box_capacity,
                                        memory_order_relaxed);
    uint32_t want = cur_n + (uint32_t)sub_n;
    if (want > cap) {
        uint32_t new_cap = cap ? cap : 16u;
        while (new_cap < want) new_cap *= 2u;
        box_t **new_arr = calloc((size_t)new_cap, sizeof(box_t *));
        if (!new_arr) {
            for (int k = 0; k < sub_n; k++) free(sub_boxes[k]);
            free(sub_boxes);
            *err = err_fmt("out of memory");
            return -1;
        }
        box_t **old_arr =
            atomic_load_explicit(&g->boxes, memory_order_relaxed);
        if (old_arr) {
            memcpy(new_arr, old_arr, cur_n * sizeof(box_t *));
            struct stale_arr *st = malloc(sizeof *st);
            if (st) { st->ptr = old_arr; st->next = g->stale_box_arrs;
                      g->stale_box_arrs = st; }
        }
        atomic_store_explicit(&g->boxes, new_arr, memory_order_release);
        atomic_store_explicit(&g->box_capacity, new_cap,
                              memory_order_release);
    }
    box_t **arr = atomic_load_explicit(&g->boxes, memory_order_relaxed);
    for (int i = 0; i < sub_n; i++) {
        arr[cur_n + (uint32_t)i] = sub_boxes[i];
    }
    atomic_store_explicit(&g->n_boxes, cur_n + (uint32_t)sub_n,
                          memory_order_release);
    /* Note: free(sub_boxes) is deferred to after the output-side
     * splice below — that splice still indexes into sub_boxes[] to
     * locate the matching ext-consumed write box. The records the
     * pointers point at have been published into g->boxes and are
     * not freed here. */

    /* OUTPUT-side rewiring. Walk every wire the parent drew leaving
     * the encap box; each one's from_branch names an output port on
     * the encap, which maps to an externally-consumed write box
     * inside the sub-map. Splice the wire onto that write box's
     * connections array so that when the write box fires, its value
     * surfaces to whichever parent consumers the user wired.
     *
     * The wires are originally on the encap's own connections array
     * (the encap is the producer). After the splice they live on the
     * matching write box's connections array; the encap's array is
     * cleared. */
    int n_encap_out = atomic_load_explicit(&encap->n_connections,
                                           memory_order_relaxed);
    connection_t *encap_out =
        atomic_load_explicit(&encap->connections, memory_order_relaxed);
    for (int k = 0; k < n_encap_out; k++) {
        connection_t *c = &encap_out[k];
        int port_idx = find_output_port_index(encap, c->from_branch);
        if (port_idx < 0) {
            *err = err_fmt("encap '%s': outgoing wire to '%s.%s' references "
                           "unknown output port '%s'",
                           encap->id, c->to_box, c->to_input,
                           c->from_branch ? c->from_branch : "(default)");
            return -1;
        }
        box_t *W = find_ext_consumed_for_port(sub_boxes, sub_n,
                                              encap, port_idx);
        if (!W) {
            *err = err_fmt("encap '%s': output port '%s' has no matching "
                           "externally-consumed write box in sub-map",
                           encap->id, encap->outputs[port_idx].name);
            return -1;
        }

        /* The find returns a sub_boxes[] pointer, but the sub-boxes
         * were just appended to g->boxes — same record, same heap
         * address. Append the encap wire to the write box's
         * connections via copy-and-grow. */
        int w_n = atomic_load_explicit(&W->n_connections,
                                       memory_order_relaxed);
        connection_t *w_old =
            atomic_load_explicit(&W->connections, memory_order_relaxed);
        connection_t *w_new = calloc((size_t)(w_n + 1), sizeof(connection_t));
        if (!w_new) {
            *err = err_fmt("out of memory");
            return -1;
        }
        if (w_n > 0) memcpy(w_new, w_old, (size_t)w_n * sizeof(connection_t));
        w_new[w_n].to_box       = c->to_box;
        w_new[w_n].to_input     = c->to_input;
        w_new[w_n].from_branch  = NULL;       /* write boxes have one output */
        w_new[w_n].to_box_idx   = -1;
        w_new[w_n].to_input_idx = -1;
        if (w_old) free(w_old);
        atomic_store_explicit(&W->connections, w_new, memory_order_release);
        atomic_store_explicit(&W->n_connections, w_n + 1,
                              memory_order_release);
    }
    if (encap_out) {
        free(encap_out);
        atomic_store_explicit(&encap->connections, NULL,
                              memory_order_release);
        atomic_store_explicit(&encap->n_connections, 0,
                              memory_order_release);
    }
    /* sub_boxes pointer-array can now go — the records it pointed at
     * are reachable via g->boxes and stay alive for the run. */
    free(sub_boxes);
    /* Strip the encap's `ref` so inline_encapsulations' find-loop
     * doesn't pick this same record up on the next iteration and
     * inline its sub-map a second time. The record stays in the
     * box list as an inert BOX_MAP; the no-op dispatch case
     * handles the (should-not-happen) reach. */
    encap->ref = NULL;

    return 0;
}
/* }}} */

/* {{{ inline_encapsulations() — splice every BOX_MAP into the graph
 *
 * Iterates until no BOX_MAP remains. Sub-maps that themselves contain
 * a BOX_MAP get picked up on the next iteration once their boxes are
 * in the parent list. A high iteration bound guards against
 * pathological recursion (which would also imply a cycle, caught
 * later by detect_cycles in the flat form). */
static int inline_encapsulations(graph_t *g, char **err)
{
    for (int iter = 0; iter < 1024; iter++) {
        uint32_t n = atomic_load_explicit(&g->n_boxes,
                                          memory_order_relaxed);
        box_t **boxes =
            atomic_load_explicit(&g->boxes, memory_order_relaxed);
        int found = -1;
        for (uint32_t i = 0; i < n; i++) {
            /* `ref` cleared after inlining → skip already-handled boxes.
             * Without this, the orphaned BOX_MAP record keeps matching
             * and the pass loops forever inlining the same sub-map. */
            if (boxes[i]->kind == BOX_MAP && boxes[i]->ref) {
                found = (int)i; break;
            }
        }
        if (found < 0) return 0;
        if (inline_one_encapsulation(g, found, err) != 0) return -1;
    }
    *err = err_fmt("inline_encapsulations: exceeded recursion limit (1024) "
                   "— likely a sub-map cycle");
    return -1;
}
/* }}} */

/* {{{ graph_load() */
graph_t *graph_load(const char *map_dir, char **err)
{
    if (err) *err = NULL;
    if (!map_dir) {
        if (err) *err = err_fmt("graph_load: NULL map_dir");
        return NULL;
    }

    graph_t *g = calloc(1, sizeof *g);
    if (!g) { if (err) *err = err_fmt("out of memory"); return NULL; }
    pthread_mutex_init(&g->graph_mu, NULL);
    atomic_init(&g->boxes,        NULL);
    atomic_init(&g->n_boxes,      0u);
    atomic_init(&g->box_capacity, 0u);
    g->stale_box_arrs = NULL;

    g->arena = json_arena_create();
    if (!g->arena) {
        if (err) *err = err_fmt("out of memory");
        free(g);
        return NULL;
    }

    g->map_dir = strdup(map_dir);
    if (!g->map_dir) {
        if (err) *err = err_fmt("out of memory");
        graph_destroy(g);
        return NULL;
    }

    if (load_meta(g, map_dir, err)            != 0) { graph_destroy(g); return NULL; }
    if (load_boxes(g, map_dir, err)           != 0) { graph_destroy(g); return NULL; }
    /* Issue 248 — splice any BOX_MAP into the parent's flat box list
     * before topology resolution. After this pass the graph is one
     * flat program with namespaced sub-map ids; resolve_topology
     * never has to know encapsulation existed. */
    if (inline_encapsulations(g, err)         != 0) { graph_destroy(g); return NULL; }
    if (resolve_topology(g, err)              != 0) { graph_destroy(g); return NULL; }
    if (detect_cycles(g, err)                 != 0) { graph_destroy(g); return NULL; }
    if (cache_read_box_values(g, err)         != 0) { graph_destroy(g); return NULL; }
    if (build_read_predecessor_lists(g, err)  != 0) { graph_destroy(g); return NULL; }
    if (detect_entry_boxes(g, err)            != 0) { graph_destroy(g); return NULL; }

    /* Compute the same-language fast path flag per call box (issue
     * 312). A box is "native-eligible" iff every adjacent call box —
     * any producer feeding its inputs AND any consumer reading its
     * output — shares the box's language. Read boxes and
     * write boxes don't constrain (they emit/consume JSON-shaped
     * bytes that any spec interprets through its bridges).
     *
     * Computed in graph_load (not graph_attach_runtime) because
     * it's purely topology-derived and doesn't need the slot
     * store or registry. The dispatch reads these per-cell to
     * pick the producer's push ring (native vs JSON) and to set
     * the consumer's `input_native[i]` flag at read time. */
    for (int i = 0; i < g->n_boxes; i++) {
        box_t *b = g->boxes[i];
        b->input_edge_native  = NULL;
        b->output_edge_native = NULL;
        if (b->kind != BOX_CALL || !b->lang) continue;

        /* Allocate per-port input bits and per-connection output
         * bits. malloc rather than calloc — we fill every entry
         * explicitly below. */
        if (b->n_inputs > 0) {
            b->input_edge_native = malloc((size_t)b->n_inputs * sizeof(int));
            if (!b->input_edge_native) { if (err) *err = err_fmt("oom"); return NULL; }
            for (int j = 0; j < b->n_inputs; j++) b->input_edge_native[j] = 0;
        }
        if (b->n_connections > 0) {
            b->output_edge_native = malloc((size_t)b->n_connections * sizeof(int));
            if (!b->output_edge_native) { if (err) *err = err_fmt("oom"); return NULL; }
            for (int j = 0; j < b->n_connections; j++) b->output_edge_native[j] = 0;
        }

        /* Outgoing edges: for each connection, the consumer's lang
         * is native iff it's also a call box in this box's
         * language. Read / write consumers are not native
         * (they don't carry a language). */
        int all_outs_native = 1;
        for (int j = 0; j < b->n_connections; j++) {
            int dst = b->connections[j].to_box_idx;
            if (dst < 0) { all_outs_native = 0; continue; }
            const box_t *c = g->boxes[dst];
            int native = (c->kind == BOX_CALL && c->lang
                          && strcmp(c->lang, b->lang) == 0);
            b->output_edge_native[j] = native;
            if (!native) all_outs_native = 0;
        }

        (void)all_outs_native;   /* slice 5 of 312 removed the per-box bit */

        /* Incoming edges: for each input port the port is native iff
         * it has at least one feeder AND every feeder is a call box
         * sharing this box's language. A mixed-language fan-in
         * downgrades the port to JSON — the slot can carry only one
         * format and JSON is the common denominator.
         *
         * NOTE (issue 318 follow-on): this per-edge classification is
         * the FALLBACK on single-ring slots. For dual-ring slots
         * (issue 312), the consumer's read path uses the per-cell
         * which_ring tag returned by slot_pop_ordered instead. The
         * dual-ring path is what makes intra-Lua $lang_opaque survive
         * a producer that was forced to JSON by a cross-language
         * sibling consumer — each cell carries its own format flag,
         * so the consumer doesn't have to trust a per-edge
         * approximation that lies when the producer's per-call
         * output_native is 0. */
        for (int port = 0; port < b->n_inputs; port++) {
            input_feeders_t f = scan_input_feeders(g, i, port, b->lang);
            b->input_edge_native[port] = (f.n_feeders > 0 &&
                                          f.n_native == f.n_feeders) ? 1 : 0;
        }
    }
    return g;
}
/* }}} */

/* {{{ graph_destroy() */
void graph_destroy(graph_t *g)
{
    if (!g) return;
    box_t **boxes = atomic_load_explicit(&g->boxes, memory_order_relaxed);
    uint32_t n    = atomic_load_explicit(&g->n_boxes, memory_order_relaxed);
    if (boxes) {
        for (uint32_t i = 0; i < n; i++) {
            box_t *b = boxes[i];
            if (!b) continue;
            free(b->inputs);
            /* Issue 248 — only BOX_MAP records ever allocate outputs[];
             * other kinds leave it NULL, and free(NULL) is fine. */
            free(b->outputs);
            free(b->connections);
            free(b->input_slot_ids);
            free(b->input_slot_modes);
            free(b->input_edge_native);
            free(b->output_edge_native);
            free((double *)b->routing.weights);
            free((double *)b->routing.thresholds);   /* issue 243 */
            /* per-box compile hint pointer arrays (strings inside
             * are arena-owned; only the array itself is heap) */
            free((void *)b->link_libs);
            free((void *)b->headers);
            /* 244 read-box cache + predecessor lists */
            free(b->cached_value);
            if (b->read_predecessor_ids) {
                for (int j = 0; j < b->n_inputs; j++) {
                    free(b->read_predecessor_ids[j]);
                }
                free(b->read_predecessor_ids);
            }
            free(b->n_read_predecessors);
            free(b->read_pred_counter_slot);
            free(b);
        }
        free(boxes);
    }
    /* Free any stale pointer-arrays parked by runtime growth. The
     * box records they pointed to are the same records already
     * freed above (growth never deep-copies records). */
    struct stale_arr *st = g->stale_box_arrs;
    while (st) {
        struct stale_arr *next = st->next;
        free(st->ptr);
        free(st);
        st = next;
    }
    pthread_mutex_destroy(&g->graph_mu);
    /* Free per-graph heap strings (issue 248 prefix-renamed ids). */
    struct owned_str *os = g->owned_strs;
    while (os) {
        struct owned_str *next = os->next;
        free(os->str);
        free(os);
        os = next;
    }
    free(g->languages);    /* element strings live in the arena */
    free(g->entry_box_ids);
    free(g->size_classes);
    free(g->map_dir);
    json_arena_destroy(g->arena);
    free(g);
}
/* }}} */

/* {{{ Runtime mutation — graph_add_box() (issue 319d)
 *
 * Appends a pre-built box to the graph's pointer index. The pointer
 * array grows via copy-and-publish (allocate at 2x capacity, memcpy
 * the old pointers in, atomic-store the new array, park the old on
 * stale_box_arrs for destroy-time reclamation). The new box's count
 * is published last with release semantics so a reader seeing
 * count = N also sees box N-1 fully published into the index. */
int graph_add_box(graph_t *g, box_t *box)
{
    if (!g || !box) return -1;
    pthread_mutex_lock(&g->graph_mu);

    uint32_t n   = atomic_load_explicit(&g->n_boxes,      memory_order_relaxed);
    uint32_t cap = atomic_load_explicit(&g->box_capacity, memory_order_relaxed);

    if (n + 1u > cap) {
        uint32_t new_cap = cap == 0 ? 16u : cap * 2u;
        box_t **new_boxes = calloc((size_t)new_cap, sizeof(box_t *));
        if (!new_boxes) {
            pthread_mutex_unlock(&g->graph_mu);
            return -1;
        }
        box_t **old_boxes = atomic_load_explicit(&g->boxes, memory_order_relaxed);
        if (old_boxes && n > 0) {
            memcpy(new_boxes, old_boxes, (size_t)n * sizeof(box_t *));
        }
        struct stale_arr *st = malloc(sizeof *st);
        if (!st) { free(new_boxes); pthread_mutex_unlock(&g->graph_mu); return -1; }
        st->ptr  = old_boxes;
        st->next = g->stale_box_arrs;
        g->stale_box_arrs = st;
        atomic_store_explicit(&g->boxes,        new_boxes, memory_order_release);
        atomic_store_explicit(&g->box_capacity, new_cap,   memory_order_release);
    }

    box_t **boxes = atomic_load_explicit(&g->boxes, memory_order_relaxed);
    boxes[n] = box;
    atomic_store_explicit(&g->n_boxes, n + 1u, memory_order_release);

    pthread_mutex_unlock(&g->graph_mu);
    return (int)n;
}
/* }}} */

/* {{{ Runtime mutation — box_add_connection() (issue 319d)
 *
 * Appends one connection to box `b`'s connections array via the
 * same copy-and-publish discipline used for boxes: allocate a
 * new connections[] sized current+1, copy the old in, append the
 * new connection, atomic-store the new connections pointer, then
 * atomic-store the new count. The old connections array is parked
 * on stale_box_arrs (the list is generic, not box-specific) and
 * freed at graph_destroy. */
int box_add_connection(graph_t *g, box_t *b, connection_t conn)
{
    if (!g || !b) return -1;
    pthread_mutex_lock(&g->graph_mu);

    int n_old = atomic_load_explicit(&b->n_connections, memory_order_relaxed);
    connection_t *old = atomic_load_explicit(&b->connections, memory_order_relaxed);

    connection_t *new_conns = malloc((size_t)(n_old + 1) * sizeof(connection_t));
    if (!new_conns) { pthread_mutex_unlock(&g->graph_mu); return -1; }
    if (old && n_old > 0) {
        memcpy(new_conns, old, (size_t)n_old * sizeof(connection_t));
    }
    new_conns[n_old] = conn;

    if (old) {
        struct stale_arr *st = malloc(sizeof *st);
        if (!st) { free(new_conns); pthread_mutex_unlock(&g->graph_mu); return -1; }
        st->ptr  = old;
        st->next = g->stale_box_arrs;
        g->stale_box_arrs = st;
    }

    atomic_store_explicit(&b->connections,   new_conns, memory_order_release);
    atomic_store_explicit(&b->n_connections, n_old + 1, memory_order_release);

    pthread_mutex_unlock(&g->graph_mu);
    return 0;
}
/* }}} */

/* {{{ Accessors */
const char *graph_name(const graph_t *g)         { return g ? g->name : NULL; }
const char *graph_description(const graph_t *g)  { return g ? g->description : NULL; }
const char *graph_entry_box_id(const graph_t *g) { return g ? g->entry_box_id : NULL; }
const char *graph_map_dir(const graph_t *g)      { return g ? g->map_dir : NULL; }
int         graph_n_boxes(const graph_t *g)
{
    return g ? (int)atomic_load_explicit(&g->n_boxes, memory_order_acquire) : 0;
}

const box_t *graph_box(const graph_t *g, int i)
{
    if (!g || i < 0) return NULL;
    uint32_t n = atomic_load_explicit(&g->n_boxes, memory_order_acquire);
    if ((uint32_t)i >= n) return NULL;
    box_t **boxes = atomic_load_explicit(&g->boxes, memory_order_acquire);
    return boxes[i];
}

const box_t *graph_box_by_id(const graph_t *g, const char *id)
{
    if (!g || !id) return NULL;
    for (int i = 0; i < g->n_boxes; i++) {
        if (strcmp(g->boxes[i]->id, id) == 0) return g->boxes[i];
    }
    return NULL;
}

int graph_box_index(const graph_t *g, const char *id)
{
    if (!g || !id) return -1;
    for (int i = 0; i < g->n_boxes; i++) {
        if (strcmp(g->boxes[i]->id, id) == 0) return i;
    }
    return -1;
}

int graph_n_languages(const graph_t *g) { return g ? g->n_languages : 0; }

const char *graph_language(const graph_t *g, int i)
{
    if (!g || i < 0 || i >= g->n_languages) return NULL;
    return g->languages[i];
}

int graph_n_entry_boxes(const graph_t *g) { return g ? g->n_entry_boxes : 0; }

int graph_entry_box(const graph_t *g, int i)
{
    if (!g || i < 0 || i >= g->n_entry_boxes) return -1;
    return g->entry_box_ids[i];
}

int graph_n_size_classes(const graph_t *g) { return g ? g->n_size_classes : 0; }

int graph_size_class(const graph_t *g, int i)
{
    if (!g || i < 0 || i >= g->n_size_classes) return -1;
    return g->size_classes[i];
}
/* }}} */

/* {{{ resolve_spec_for_box() — find the spec index by box->lang */
/* Returns the spec's index in the registry, or -1 if not found. */
static int resolve_spec_for_box(const box_t *box, spec_registry_t *r)
{
    if (!box || !r) return -1;
    if (box->kind != BOX_CALL) return -1;
    if (box->lang) {
        for (int i = 0; i < spec_registry_size(r); i++) {
            const lang_spec_t *s = spec_registry_at(r, i);
            if (strcmp(s->name, box->lang) == 0) return i;
        }
    }
    /* Fallback: derive from ref extension if lang isn't explicit.
     * Simple search for the last '.' and match against file_ext. */
    if (box->ref) {
        const char *dot = strrchr(box->ref, '.');
        if (dot) {
            for (int i = 0; i < spec_registry_size(r); i++) {
                const lang_spec_t *s = spec_registry_at(r, i);
                if (s->file_ext && strcmp(s->file_ext, dot) == 0) return i;
            }
        }
    }
    return -1;
}
/* }}} */

/* {{{ propagate_multi_spawn() — BFS forward from iterators */
/* A box is "multi_spawn" if it's an iterator OR if any producer
 * feeding one of its inputs is itself multi_spawn. The marker
 * propagates forward through the connection graph. */
#define MULTI_SPAWN_RING_CELLS 16

static void propagate_multi_spawn(graph_t *g)
{
    /* Seed: every iterator-routing call box is multi_spawn. */
    for (int i = 0; i < g->n_boxes; i++) {
        box_t *b = g->boxes[i];
        b->multi_spawn = (b->kind == BOX_CALL &&
                          b->routing.kind == ROUTING_ITERATOR) ? 1 : 0;
    }
    /* Fixed-point: as long as any new box gets marked, iterate. The
     * graph is small (≤ a few hundred boxes in practice), so the
     * O(n × edges) bound is fine. */
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < g->n_boxes; i++) {
            const box_t *b = g->boxes[i];
            if (!b->multi_spawn) continue;
            for (int j = 0; j < b->n_connections; j++) {
                int dst = b->connections[j].to_box_idx;
                if (dst < 0) continue;
                if (!g->boxes[dst]->multi_spawn) {
                    g->boxes[dst]->multi_spawn = 1;
                    changed = 1;
                }
            }
        }
    }
}
/* }}} */

/* {{{ graph_attach_runtime() — phase 5 + 7 */
int graph_attach_runtime(graph_t *g,
                         struct slot_store *slots,
                         struct spec_registry *specs,
                         int default_cell_bytes,
                         char **err)
{
    if (!g || !slots) {
        if (err) *err = err_fmt("graph_attach_runtime: NULL graph or slots");
        return -1;
    }
    if (default_cell_bytes <= 0) default_cell_bytes = 4096;

    /* Mark multi-spawn boxes via forward BFS from iterators so we
     * can pick the right slot mode for each input port below. */
    propagate_multi_spawn(g);

    for (int i = 0; i < g->n_boxes; i++) {
        box_t *b = g->boxes[i];

        /* Allocate slots per input port. Multi-spawn boxes get
         * N-cell pop rings so producers' multiple pushes accumulate
         * and each task drains one cell. Single-spawn boxes get
         * 1-cell peek slots (the legacy fast path). */
        if (b->n_inputs > 0) {
            b->input_slot_ids   = malloc((size_t)b->n_inputs * sizeof(int));
            b->input_slot_modes = malloc((size_t)b->n_inputs * sizeof(int));
            if (!b->input_slot_ids || !b->input_slot_modes) {
                if (err) *err = err_fmt("out of memory allocating input slots");
                return -1;
            }
            int n_cells = b->multi_spawn ? MULTI_SPAWN_RING_CELLS : 1;
            int mode    = b->multi_spawn ? SLOT_MODE_POP : SLOT_MODE_PEEK;
            /* Multi-spawn rings tag each cell so pops serve in
             * producer-supplied order (issue 304). Iterators pass
             * their counter as the tag; non-iterator producers pass
             * 0 and effectively get FIFO ordering. */
            int base_flags = b->multi_spawn ? SLOT_FLAG_TAGGED : 0;
            for (int j = 0; j < b->n_inputs; j++) {
                /* One reverse-scan, four facts: variable-size
                 * detection (LARGE_VALUE flag) and max declared
                 * capacity (cell width) both come from the same
                 * walk. Read boxes default to output_capacity 0
                 * since they emit arbitrary file contents; a call
                 * box opts in by setting output_capacity to 0. */
                input_feeders_t fdr = scan_input_feeders(g, i, j, NULL);
                int slot_flags = base_flags;
                if (fdr.has_variable) slot_flags |= SLOT_FLAG_LARGE_VALUE;

                int cell_bytes = (fdr.max_capacity > default_cell_bytes)
                                    ? fdr.max_capacity : default_cell_bytes;

                /* Slice 3 of issue 312: call-box input ports become
                 * dual-ring slots so producers can write native or
                 * JSON per per-edge classification. DUAL_RING doesn't
                 * combine with LARGE_VALUE / TAGGED yet, so ports
                 * requiring those fall back to single-ring. */
                if (b->kind == BOX_CALL && b->lang &&
                    !(slot_flags & (SLOT_FLAG_LARGE_VALUE | SLOT_FLAG_TAGGED))) {
                    slot_flags |= SLOT_FLAG_DUAL_RING;
                }

                slot_id_t id = slot_alloc((slot_store_t *)slots,
                                          cell_bytes, n_cells,
                                          slot_flags);
                if (id == SLOT_INVALID) {
                    if (err) *err = err_fmt("box '%s': failed to allocate "
                                            "slot for input '%s'",
                                            b->id, b->inputs[j].name);
                    return -1;
                }
                b->input_slot_ids[j]   = (int)id;
                b->input_slot_modes[j] = mode;
            }
        }

        /* Resolve the spec index for call boxes. Non-call boxes
         * stay at -1; the dispatch layer handles them directly. */
        b->spec_idx = -1;
        if (b->kind == BOX_CALL && specs) {
            b->spec_idx = resolve_spec_for_box(b, (spec_registry_t *)specs);
            if (b->spec_idx < 0) {
                if (err) *err = err_fmt("box '%s': no spec found for "
                                        "lang='%s' / ref='%s'",
                                        b->id, b->lang ? b->lang : "(null)",
                                        b->ref ? b->ref : "(null)");
                return -1;
            }
        }

        /* Iterator boxes need an atomic-counter slot so the dispatch
         * layer can pick the output branch via slot_read_inc with
         * concurrent-safe semantics. */
        b->counter_slot_id = -1;
        if (b->kind == BOX_CALL &&
            (b->routing.kind == ROUTING_ITERATOR    ||
             b->routing.kind == ROUTING_RANDOMIZER  ||
             b->routing.kind == ROUTING_WEIGHTED    ||
             b->routing.kind == ROUTING_DISTRIBUTOR)) {
            slot_id_t cid = slot_alloc((slot_store_t *)slots, 0, 0,
                                       SLOT_FLAG_ATOMIC_COUNTER);
            if (cid == SLOT_INVALID) {
                if (err) *err = err_fmt("box '%s': could not allocate "
                                        "atomic-counter slot", b->id);
                return -1;
            }
            b->counter_slot_id = (int)cid;
        }

        /* 244 round-robin counter slots. A consumer port with more
         * than one read-box predecessor rotates among them; the
         * atomic counter lets parallel attempt-tasks each grab a
         * distinct index. Ports with zero or one read predecessor
         * don't need a counter (no predecessor → no pull; one
         * predecessor → always index 0). */
        if (b->n_read_predecessors) {
            for (int j = 0; j < b->n_inputs; j++) {
                if (b->n_read_predecessors[j] <= 1) continue;
                slot_id_t cid = slot_alloc((slot_store_t *)slots, 0, 0,
                                           SLOT_FLAG_ATOMIC_COUNTER);
                if (cid == SLOT_INVALID) {
                    if (err) *err = err_fmt("box '%s': could not allocate "
                                            "read-rotation counter for input '%s'",
                                            b->id, b->inputs[j].name);
                    return -1;
                }
                b->read_pred_counter_slot[j] = (int)cid;
            }
        }
    }

    /* Enumerate distinct languages used by call boxes. Stored as
     * a small array of arena-owned string pointers; the pool init
     * can use it to skip specs no box in this map uses. */
    free(g->languages);
    g->languages   = NULL;
    g->n_languages = 0;
    if (g->n_boxes > 0) {
        g->languages = calloc((size_t)g->n_boxes, sizeof(const char *));
        if (!g->languages) {
            if (err) *err = err_fmt("out of memory");
            return -1;
        }
        for (int i = 0; i < g->n_boxes; i++) {
            const box_t *b = g->boxes[i];
            if (b->kind != BOX_CALL || !b->lang) continue;
            int seen = 0;
            for (int k = 0; k < g->n_languages; k++) {
                if (strcmp(g->languages[k], b->lang) == 0) { seen = 1; break; }
            }
            if (!seen) g->languages[g->n_languages++] = b->lang;
        }
    }

    /* Distinct slot cell widths used across input ports. Each port's
     * cell width was picked from max(feeder output_capacity,
     * default_cell_bytes), so the same reverse-scan helper drives
     * both the slot allocation above and this enumeration. The
     * slot store's free-list pre-population (issue 302) can use
     * this set to amortise allocations per class. */
    free(g->size_classes);
    g->size_classes   = NULL;
    g->n_size_classes = 0;
    int total_ports = 0;
    for (int i = 0; i < g->n_boxes; i++) total_ports += g->boxes[i]->n_inputs;
    if (total_ports > 0) {
        g->size_classes = calloc((size_t)total_ports, sizeof(int));
        if (!g->size_classes) {
            if (err) *err = err_fmt("out of memory");
            return -1;
        }
        for (int i = 0; i < g->n_boxes; i++) {
            const box_t *b = g->boxes[i];
            for (int j = 0; j < b->n_inputs; j++) {
                input_feeders_t fdr = scan_input_feeders(g, i, j, NULL);
                int width = (fdr.max_capacity > default_cell_bytes)
                                ? fdr.max_capacity : default_cell_bytes;
                int seen = 0;
                for (int k = 0; k < g->n_size_classes; k++) {
                    if (g->size_classes[k] == width) { seen = 1; break; }
                }
                if (!seen) g->size_classes[g->n_size_classes++] = width;
            }
        }
    }

    /* Issue 318 / 7 — compile-time wire validation. Walk every
     * outgoing edge of every call box; for cross-language edges,
     * verify the producer's sentinel_emit_mask is a subset of the
     * consumer's sentinel_reconstruct_mask. Mismatches surface as
     * stderr warnings — they're not fatal because the producer's
     * encode may never actually emit a sentinel for a given call,
     * and erroring at graph load on a possibility that may never
     * happen would block too many legitimate graphs. The warning
     * gives the user information; behaviour stays unchanged. */
    if (specs) {
        for (int i = 0; i < g->n_boxes; i++) {
            const box_t *p = g->boxes[i];
            if (p->kind != BOX_CALL || p->spec_idx < 0) continue;
            const lang_spec_t *p_spec = spec_registry_at(specs, p->spec_idx);
            if (!p_spec || p_spec->sentinel_emit_mask == 0) continue;
            for (int j = 0; j < p->n_connections; j++) {
                int dst = p->connections[j].to_box_idx;
                if (dst < 0) continue;
                const box_t *c = g->boxes[dst];
                if (c->kind != BOX_CALL || c->spec_idx < 0) continue;
                if (p->lang && c->lang &&
                    strcmp(p->lang, c->lang) == 0) continue; /* same-lang */
                const lang_spec_t *c_spec = spec_registry_at(specs, c->spec_idx);
                if (!c_spec) continue;
                unsigned int gap = p_spec->sentinel_emit_mask &
                                  ~c_spec->sentinel_reconstruct_mask;
                if (gap != 0) {
                    fprintf(stderr,
                        "wire-validate: '%s' (%s) → '%s' (%s) may emit "
                        "sentinel kinds the consumer cannot reconstruct "
                        "(producer emit=0x%x, consumer reconstruct=0x%x, "
                        "gap=0x%x). Attach a custom_translation shim on the "
                        "destination port (issue 246) if the producer's "
                        "values trip this case.\n",
                        p->id, p->lang ? p->lang : "?",
                        c->id, c->lang ? c->lang : "?",
                        p_spec->sentinel_emit_mask,
                        c_spec->sentinel_reconstruct_mask,
                        gap);
                }
            }
        }
    }

    return 0;
}
/* }}} */
