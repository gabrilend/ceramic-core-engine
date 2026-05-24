/* src/010-graph-loader.h — C graph loader (phase 3), public API.
 *
 * What it is, in a sentence: reads a map directory off disk, parses
 * every JSON file in it, and produces an in-memory graph_t that the
 * dispatch layer (issue 304) and the pool runner (issue 301) walk
 * to drive a run.
 *
 * Designed in issue 305. Aligned to the unified routing schema
 * from issue 233 — every call box carries a routing.kind, and the
 * legacy `comparand` / `iterator_outputs` fields are gone. The data
 * and file_write box kinds from issue 229 are dispatch-layer
 * primitives — they have no function to invoke.
 *
 * Memory model: every string the graph_t holds (box ids, refs,
 * function names, branch tags, paths, input port names) lives in a
 * single json_arena_t owned by the graph_t. graph_destroy frees the
 * arena, the box / connection / input arrays, and the graph_t in
 * one call.
 *
 * Errors: graph_load returns NULL on failure and sets *err to a
 * malloc'd diagnostic of the form "<file>:<line>: <message>". The
 * caller frees *err. The hard-crash policy of the rest of phase 3
 * (issue 303) applies upstream — graph_load's caller decides
 * whether to abort or surface the error.
 *
 * Pipeline: directory walk, JSON parse, per-box schema validation,
 * topology resolution (string refs → integer indices), non-iterator
 * cycle rejection, per-edge same-language classification (issue 312),
 * entry-box detection. graph_attach_runtime then allocates input
 * slots sized per the feeders' declared output_capacity, resolves
 * each call box's language spec, and enumerates the distinct
 * languages and slot size classes the map uses.
 *
 * Errors out of any phase abort the load and return NULL with a
 * diagnostic via *err.
 */

#ifndef SORAMECH_GRAPH_LOADER_H
#define SORAMECH_GRAPH_LOADER_H

#include <stddef.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Box kinds */
typedef enum {
    BOX_CALL,    /* runs a function via a language spec                */
    BOX_READ,    /* value source — inline literal or file at `path`    */
    BOX_WRITE,   /* file sink — writes one input, emits "true" downstream */
} box_kind_t;
/* }}} */

/* {{{ Routing kinds (issue 233 unified schema) */
typedef enum {
    ROUTING_PLAIN,
    ROUTING_COMPARATOR,
    ROUTING_ITERATOR,
    ROUTING_RANDOMIZER,
    ROUTING_WEIGHTED,
    ROUTING_DISTRIBUTOR,
} routing_kind_t;

typedef struct {
    routing_kind_t kind;
    int            n_outputs;   /* iterator / randomizer / weighted / distributor */
    double         comparand;   /* comparator */
    /* Weighted routing: arena-owned double array of n_outputs
     * entries. Normalised at runtime. NULL for other kinds. */
    const double  *weights;
} routing_t;
/* }}} */

/* {{{ Input port declaration */
typedef struct {
    const char *name;       /* port name; arena-owned     */
    const char *type;       /* "string" by convention     */
    const char *literal;    /* NULL or arena-owned value  */
    int         optional;   /* issue 230 optional flag    */
    /* Per-port custom translation shim (issue 246). When non-NULL,
     * names a file relative to the map dir that contains a
     * user-written shim. The shim runs at the start of the
     * consumer's task and replaces the default decode for THIS
     * port only — other ports keep the default path. The shim's
     * language is implied by the box's `lang` field; the file is
     * compiled / loaded by the appropriate spec's machinery. */
    const char *custom_translation;
} input_decl_t;
/* }}} */

/* {{{ Outgoing connection */
/* String refs are preserved for diagnostics; resolved integer
 * indices are filled in by the topology pass and let the dispatch
 * layer (issue 304) avoid string lookups at runtime. */
typedef struct {
    const char *to_box;       /* destination box id (arena-owned)        */
    const char *to_input;     /* destination port name                   */
    const char *from_branch;  /* NULL for plain output; else branch name */
    int         to_box_idx;   /* resolved index into graph->boxes        */
    int         to_input_idx; /* resolved index into target's inputs[]   */
} connection_t;
/* }}} */

/* {{{ Box */
/* Tagged so the forward declaration `typedef struct box box_t;` in
 * langs/lang-spec.h matches and the compile callback can accept a
 * `const box_t *` parameter. */
typedef struct box {
    const char    *id;
    box_kind_t     kind;

    /* Call boxes */
    const char    *lang;          /* "lua" / "c" / "bash" / NULL    */
    const char    *ref;           /* source file (call only)        */
    const char    *fn;            /* function name (call only)      */
    const char    *returns;       /* declared output type: "string"/"int"/
                                   * "double"/"bool"/"bytes"/"json"/"void"
                                   * or NULL when the user hasn't said.
                                   * Read by specs on the JSON-output
                                   * path (issue 307 item 2). */
    routing_t      routing;       /* call only                      */
    int            output_capacity; /* 0 means variable-size        */

    /* Per-box compile hints, optional, currently used by the C spec
     * (issue 307). Lua / Bash boxes leave these at NULL / 0. All
     * point into the graph's arena; no separate ownership. */
    const char    *cflags;        /* extra flags appended to `cc`   */
    int            n_link_libs;
    const char   **link_libs;     /* each becomes `-l<name>`        */
    int            n_headers;
    const char   **headers;       /* `#include`d by generated wrappers */

    /* Read boxes */
    const char    *path;          /* file path (read only; ignored if value set) */
    const char    *value;         /* inline literal; if set, read emits this directly */

    /* Read-box value cache (issue 244): the bytes a downstream consumer
     * pulls when it needs this read box's value. Populated at graph
     * load — inline `value` is strdup'd; file `path` is read and the
     * file's bytes are owned here. Always present on BOX_READ; NULL on
     * call / write boxes. Freed by graph_destroy. */
    char          *cached_value;
    int            cached_size;

    /* Common */
    int            n_inputs;
    input_decl_t  *inputs;
    /* Connections grow at runtime via runtime_connect (issue 319d).
     * Writers allocate a new array, copy + append, atomic-store the
     * new connections pointer, then atomic-store the new count;
     * old arrays are parked on the graph's stale list and freed at
     * graph_destroy. Readers see either (old, old) / (new, old) /
     * (new, new) — never (old, new) — so iteration is always safe
     * against a concurrent grow. Implicit atomic loads via C11
     * cover the common access patterns. */
    _Atomic int                n_connections;
    _Atomic(connection_t *)    connections;

    /* Runtime state — populated by graph_attach_runtime, left at
     * defaults (-1 / NULL / 0) by graph_load alone. */
    int            spec_idx;       /* index into the spec registry; -1 if no spec  */
    int           *input_slot_ids; /* n_inputs entries; -1 if not allocated yet    */
    int           *input_slot_modes;/* 0 = PEEK (1-cell), 1 = POP (N-cell)         */
    int            counter_slot_id;/* atomic-counter slot for iterator routing; -1 */
    int            multi_spawn;    /* 1 if the box may fire many times in one run  */

    /* 312 per-edge fast-path classification.
     *
     * One bit per input port: true iff the producer feeding that
     * port shares this box's language. Drives the consumer's
     * decoder choice on single-ring slots and seeds the
     * `input_native[i]` flag that the spec's invoke consults.
     *
     * One bit per outgoing connection (not per output port — a
     * single output may fan to consumers of different languages):
     * true iff the consumer on the other end of that connection
     * shares this box's language. When set, the producer pushes
     * native bytes to the consumer slot's `ring_native`; when
     * cleared, the producer calls `native_to_json` first and
     * pushes JSON to `ring_json`. */
    int           *input_edge_native;   /* n_inputs entries, or NULL              */
    int           *output_edge_native;  /* n_connections entries, or NULL         */

    /* 244 per-port read-box predecessor lists.
     *
     * A consumer's input port may be fed by one or more BOX_READ
     * value sources. Under the pull-on-demand model these don't push
     * at startup; instead, when the consumer fires and finds the
     * slot empty, the dispatch layer pulls the next read box's
     * cached value into the input buffer directly. With more than
     * one predecessor on the same port the dispatch rotates via the
     * counter slot (atomic increment, mod count).
     *
     * Allocated by graph_load: n_inputs entries each; entries with
     * zero read predecessors hold NULL / 0 / -1. Freed by
     * graph_destroy. */
    int           *n_read_predecessors;    /* n_inputs entries     */
    int          **read_predecessor_ids;   /* per-port box-index lists */
    int           *read_pred_counter_slot; /* per-port; -1 unless >1 preds */
} box_t;

/* Slot mode constants used by box_t.input_slot_modes[]. */
#define SLOT_MODE_PEEK 0
#define SLOT_MODE_POP  1
/* }}} */

/* {{{ Graph */
typedef struct graph graph_t;

/* Read a map directory and build the graph. Returns NULL on any
 * failure and sets *err to a malloc'd "<file>:<line>: <message>"
 * diagnostic. The caller frees *err. */
graph_t *graph_load(const char *map_dir, char **err);

/* Free the graph and every byte it owns. Safe on NULL. */
void     graph_destroy(graph_t *g);

/* Accessors. */
const char  *graph_name        (const graph_t *g);
const char  *graph_description (const graph_t *g);
const char  *graph_entry_box_id(const graph_t *g);
const char  *graph_map_dir     (const graph_t *g);
int          graph_n_boxes     (const graph_t *g);
const box_t *graph_box         (const graph_t *g, int i);
const box_t *graph_box_by_id   (const graph_t *g, const char *id);

/* Find a box's index by id; -1 if missing. */
int          graph_box_index   (const graph_t *g, const char *id);

/* Distinct language names used by call boxes in the map (filled
 * in by graph_attach_runtime). The pool's spec init can use this
 * to skip workers for specs the map doesn't actually use. */
int          graph_n_languages (const graph_t *g);
const char  *graph_language    (const graph_t *g, int i);

/* Entry-box set: indices into graph->boxes of boxes the pool runner
 * submits as initial tasks. A box qualifies when it is BOX_CALL or
 * BOX_WRITE AND every non-optional input is fed only by BOX_READ
 * value sources (or has zero inputs). Filled in by graph_load. */
int          graph_n_entry_boxes(const graph_t *g);
int          graph_entry_box    (const graph_t *g, int i);

/* Distinct input-slot cell sizes the graph uses. Filled in by
 * graph_attach_runtime after the per-port slot widths are picked.
 * The slot store can use this to pre-populate free lists. */
int          graph_n_size_classes(const graph_t *g);
int          graph_size_class    (const graph_t *g, int i);
/* }}} */

/* {{{ Runtime mutation primitives (issue 319d)
 *
 * graph_add_box appends a pre-built box record to the graph's
 * index. The caller owns the box record's construction — fills in
 * id / kind / lang / ref / fn / inputs / etc. — and hands the
 * malloc'd `box` pointer to this function. After the call the box
 * is owned by the graph; graph_destroy will free it. Returns the
 * new box's index (>= 0) or -1 on allocation failure.
 *
 * box_add_connection appends a connection entry to the given box's
 * `connections` array via atomic copy-and-publish: a new array is
 * allocated at current_n+1 entries, the old entries are copied in,
 * the new entry is appended, and the new array pointer is published
 * before the new count. The old array is parked on the graph's
 * stale-list and freed at graph_destroy. Concurrent dispatch reads
 * of the producer's connections see either the pre-append snapshot
 * or the post-append snapshot, never a half-published state.
 *
 * Single-spawn box invariant: the dispatch never reads a producer's
 * connections concurrently from two workers (single-spawn CAS guard
 * in dispatch). The common case for box_add_connection is a spec
 * calling `connect()` from inside its own invoke; the producer
 * isn't firing on any other worker at that moment.
 *
 * Returns 0 on success, -1 on allocation failure. */
int graph_add_box       (graph_t *g, box_t *box);
int box_add_connection  (graph_t *g, box_t *b, connection_t conn);
/* }}} */

/* {{{ Runtime attach
 *
 * Allocates one input slot per input port for every box and resolves
 * each call box's language spec from the registry. After this call,
 * the dispatch layer can read inputs via `box->input_slot_ids[i]` and
 * invoke the spec via `spec_registry_at(r, box->spec_idx)`.
 *
 * Per-port slot sizing: cell_capacity = max(declared output_capacity
 * across feeders, default_cell_bytes). The LARGE_VALUE flag is set if
 * any feeder has output_capacity == 0 (variable-size). Multi-spawn
 * boxes (any box reachable from an iterator-routing call box) get an
 * N-cell pop ring; single-spawn boxes get a 1-cell peek slot.
 *
 * Returns 0 on success, -1 with *err set on the first resolution or
 * allocation failure. */
struct slot_store;        /* opaque forward decl */
struct spec_registry;     /* opaque forward decl */
int graph_attach_runtime(graph_t *g,
                         struct slot_store    *slots,
                         struct spec_registry *specs,
                         int                   default_cell_bytes,
                         char                **err);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_GRAPH_LOADER_H */
