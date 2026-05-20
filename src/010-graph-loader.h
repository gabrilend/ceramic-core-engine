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
 * What's implemented now: phases 1 (directory walk), 2 (JSON
 * parse), and the first part of 3 (per-box schema validation —
 * kind valid, id unique, kind-specific required fields present,
 * routing schema validated).
 *
 * What's deferred to follow-ons within 305:
 *  - Phase 4: topology validation. Connections currently store
 *    string refs (to_box / to_input); resolving those to integer
 *    indices, verifying both endpoints exist, and detecting
 *    non-iterator cycles all land in the next iteration.
 *  - Phase 5: slot size class enumeration.
 *  - Phase 6: entry-box detection.
 *  - Phase 7: language enumeration.
 *
 * None of those block 301 / 303 / 306 from starting.
 */

#ifndef SORAMECH_GRAPH_LOADER_H
#define SORAMECH_GRAPH_LOADER_H

#include <stddef.h>

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
    ROUTING_RANDOMIZER,    /* not yet parsed; reserved */
    ROUTING_WEIGHTED,      /* not yet parsed; reserved */
    ROUTING_DISTRIBUTOR,   /* not yet parsed; reserved */
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
typedef struct {
    const char    *id;
    box_kind_t     kind;

    /* Call boxes */
    const char    *lang;          /* "lua" / "c" / "bash" / NULL    */
    const char    *ref;           /* source file (call only)        */
    const char    *fn;            /* function name (call only)      */
    routing_t      routing;       /* call only                      */
    int            output_capacity; /* 0 means variable-size        */

    /* Read boxes */
    const char    *path;          /* file path (read only; ignored if value set) */
    const char    *value;         /* inline literal; if set, read emits this directly */

    /* Common */
    int            n_inputs;
    input_decl_t  *inputs;
    int            n_connections;
    connection_t  *connections;

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
     * port shares this box's language. When set, the dispatch
     * passes the slot's bytes to invoke_native as-is; when cleared
     * the dispatch calls json_to_native first.
     *
     * One bit per outgoing connection (not per output port — a
     * single output may fan to consumers of different languages):
     * true iff the consumer on the other end of that connection
     * shares this box's language. When set, push native bytes
     * directly; when cleared, call native_to_json first and push
     * JSON. */
    int           *input_edge_native;   /* n_inputs entries, or NULL              */
    int           *output_edge_native;  /* n_connections entries, or NULL         */

    /* The earlier per-box approximation, retained until the
     * dispatch fully switches to per-edge consultation. Computed as
     * (all input_edge_native bits set) && (all output_edge_native
     * bits set) — i.e. true only for boxes inside a same-language
     * island with no cross-language neighbours. */
    int            use_native_invoke;
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
/* }}} */

/* {{{ Runtime attach (phase 5 + 7)
 *
 * Allocates one slot per input port for every box and resolves each
 * call box's language spec from the registry. After this call, the
 * dispatch layer can read inputs via `box->input_slot_ids[i]` and
 * invoke the spec via `spec_registry_at(r, box->spec_idx)`.
 *
 * Slot defaults for this iteration:
 *   - cell_capacity = `default_cell_bytes` (or 4096 if 0)
 *   - n_cells = 1 (peek mode)
 *   - flags = 0
 *
 * Iterator-fed wires and the multi-cell pop mode land in a follow-on
 * — they need compile-time wire classification (issue 305 phase 5
 * proper) that this iteration ducks. Returns 0 on success, -1 with
 * *err set on the first resolution or allocation failure. */
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
