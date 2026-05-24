/* langs/c/soramech.h — runtime self-construction API for C boxes.
 *
 * User C box code that wants to spawn a new box at runtime or
 * connect wires between boxes can `#include "soramech.h"` and call
 * these functions. The symbols are resolved at dlopen time —
 * soramech-pool is linked with -rdynamic so the dlopen'd box.so
 * binaries find the runner's runtime_* implementations.
 *
 * Designed in issue 319 (Q1.5 / Q2 / Q4 resolutions); landed in
 * issue 319e (C slice of the bindings).
 *
 * Argument shape: both functions take JSON text matching the on-disk
 * box JSON schema. For `create_box`, pass an object shaped like an
 * entry in `boxes/<id>.json`. For `connect`, pass an object shaped
 * like an entry in `connections[]`.
 *
 * Failure handling: both return -1 on any error with `*err` set to
 * a malloc'd diagnostic string (caller frees). Error semantics
 * follow the project-wide rule: callers should treat -1 as a hard
 * fault, not a normal-flow result.
 *
 * These names are aliases for the runtime_* symbols in
 * src/018-runtime-builtins.h — the runner owns the implementation;
 * this header just gives user box code a cleaner, project-namespaced
 * way to call it.
 */
#ifndef SORAMECH_LANG_C_SORAMECH_H
#define SORAMECH_LANG_C_SORAMECH_H

#include <stddef.h>

/* Minimum buffer size for create_box's out_id parameter. Holds
 * "auto_" + 8 hex chars + null. */
#define SORAMECH_BOX_ID_BUF_SIZE 14u

/* Forward declarations of the runner-side implementations. The
 * actual definitions live in src/018-runtime-builtins.c and are
 * exported to dlopen'd plugins via -rdynamic. */
int runtime_create_box(const char *spec_json, int spec_len,
                       char *out_id, size_t out_id_size,
                       char **err);
int runtime_connect(const char *conn_json, int conn_len,
                    char **err);

/* User-friendly aliases. */
#define soramech_create_box runtime_create_box
#define soramech_connect    runtime_connect

#endif /* SORAMECH_LANG_C_SORAMECH_H */
