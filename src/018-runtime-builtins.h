/* src/018-runtime-builtins.h — runtime self-construction primitives.
 *
 * Two C functions that language specs (Lua first, C/Bash to follow)
 * wrap and expose to user code, letting a running box construct new
 * boxes and wire them into the live graph:
 *
 *   runtime_create_box(spec_json) -> box_id
 *   runtime_connect(connection_json) -> 0/-1
 *
 * Designed in issue 319 (the headline self-construction feature);
 * landed in issue 319d.
 *
 * Argument shape: the JSON text passed in mirrors the on-disk box
 * JSON schema exactly. Anything you can write into a `boxes/<id>.json`
 * file, you can pass to runtime_create_box. Anything you can write
 * as an entry inside that file's `connections[]` array, you can
 * pass to runtime_connect. The contract is: the API is the schema.
 *
 * Plumbing: language specs need access to the graph / slot store /
 * spec registry to call these. Rather than thread those pointers
 * through every spec invocation, we keep them in a thread-local
 * "active context" that dispatch_action sets before invoking the
 * spec and clears after. The runtime builtins read the thread-local
 * to find the targets.
 */
#ifndef SORAMECH_018_RUNTIME_BUILTINS_H
#define SORAMECH_018_RUNTIME_BUILTINS_H

#include <stddef.h>

struct graph;
struct slot_store;
struct spec_registry;

/* {{{ Active runtime context (thread-local)
 *
 * Set by dispatch_action immediately before invoking a spec, cleared
 * immediately after. Spec code (including the language bindings that
 * call into runtime_create_box / runtime_connect) reads these to find
 * the graph and runtime resources. */
void runtime_set_active_context(struct graph         *g,
                                struct slot_store    *s,
                                struct spec_registry *r);
void runtime_clear_active_context(void);

struct graph         *runtime_active_graph(void);
struct slot_store    *runtime_active_slots(void);
struct spec_registry *runtime_active_specs(void);
/* }}} */

/* {{{ runtime_create_box()
 *
 * Parses `spec_json` as a box schema entry, instantiates the box at
 * runtime, allocates input slots, registers it in the active graph.
 * Fills `out_id` with the new box's id (auto-generated if the JSON
 * didn't supply one). Returns 0 on success, -1 on error with *err
 * set to a malloc'd diagnostic (caller frees).
 *
 * Slice 1 scope (this commit):
 *   - kind: "call" only
 *   - lang: "lua" only
 *   - routing.kind: "plain" only
 *   - inputs: array of { name, type }
 *
 * Other kinds, langs, and routing variants surface clear errors
 * for now; they'll land in 319e. */
int runtime_create_box(const char *spec_json, int spec_len,
                       char *out_id, size_t out_id_size,
                       char **err);
/* }}} */

/* {{{ runtime_connect()
 *
 * Parses `conn_json` as a connection entry, looks up from_box /
 * to_box / to_input, appends the connection to the producer's
 * connections array via box_add_connection. Returns 0 on success,
 * -1 on error with *err set to a malloc'd diagnostic. */
int runtime_connect(const char *conn_json, int conn_len,
                    char **err);
/* }}} */

#endif /* SORAMECH_018_RUNTIME_BUILTINS_H */
