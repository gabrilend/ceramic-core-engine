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
    BOX_CALL,         /* runs a function via a language spec        */
    BOX_DATA,         /* file source — dispatch reads `path` at run */
    BOX_FILE_WRITE,   /* file sink  — dispatch writes inputs at run */
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
    /* For routing kinds that need them later — weights, thresholds —
     * the parser leaves their fields zero in this iteration and adds
     * them when those kinds are implemented. */
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

/* {{{ Outgoing connection (string refs; resolved in phase 4) */
typedef struct {
    const char *to_box;       /* destination box id (arena-owned)        */
    const char *to_input;     /* destination port name                   */
    const char *from_branch;  /* NULL for plain output; else branch name */
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

    /* Data boxes */
    const char    *path;          /* file path (data only)          */

    /* Common */
    int            n_inputs;
    input_decl_t  *inputs;
    int            n_connections;
    connection_t  *connections;
} box_t;
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
int          graph_n_boxes     (const graph_t *g);
const box_t *graph_box         (const graph_t *g, int i);
const box_t *graph_box_by_id   (const graph_t *g, const char *id);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_GRAPH_LOADER_H */
