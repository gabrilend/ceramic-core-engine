/* src/011-spec-registry.h — language spec registry, public API.
 *
 * What it is, in a sentence: scans langs/<name>/ at startup,
 * dlopens every spec.so it finds, and exposes a name- and
 * extension-keyed lookup to the rest of the runtime.
 *
 * Designed in issue 303. The pool runner (issue 301) consults the
 * registry during per-worker init — for every language the map
 * uses, the worker's per-language handle is initialised via the
 * spec's `init` callback. The dispatch layer (issue 304) looks the
 * spec up at runtime to invoke a box's function.
 *
 * Lifetime: created at process startup, alive for the run, freed
 * at shutdown. dlclose() is called on every handle in destroy.
 *
 * Errors: spec_registry_load returns NULL and sets *err on failure
 * (cannot open langs_dir, dlopen failure, missing required symbol).
 * The caller frees *err.
 */

#ifndef SORAMECH_SPEC_REGISTRY_H
#define SORAMECH_SPEC_REGISTRY_H

#include "lang-spec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spec_registry spec_registry_t;

/* Open every langs_dir/<name>/spec.so and read its
 * soramech_lang_spec symbol. Returns NULL on failure with *err set. */
spec_registry_t *spec_registry_load(const char *langs_dir, char **err);

/* dlclose every handle, free the registry. Safe on NULL. */
void             spec_registry_destroy(spec_registry_t *r);

int              spec_registry_size(const spec_registry_t *r);

/* Look up by spec.name ("lua", "bash", "c"). NULL if not found. */
const lang_spec_t *spec_registry_get(const spec_registry_t *r, const char *name);

/* Look up by file_ext (".lua", ".sh", ".c"). Used by the graph
 * loader / dispatch when a box's lang isn't explicit. NULL if not
 * found. */
const lang_spec_t *spec_registry_for_ext(const spec_registry_t *r, const char *file_ext);

/* Enumerate registered specs by index for diagnostics / iteration. */
const lang_spec_t *spec_registry_at(const spec_registry_t *r, int i);

/* {{{ Per-worker init / teardown
 *
 * The pool's `worker_main` calls these once per worker between TLS
 * setup and the init barrier. For each spec in the registry, calls
 * spec->init(worker_idx) and stashes the returned handle in the
 * caller-provided handles array. Index in handles corresponds to
 * index in the registry (use `spec_registry_at(r, i)` to get the
 * spec back). Specs whose init pointer is NULL get a NULL handle.
 *
 * Returns the number of handles filled (== registry size) on
 * success, -1 on the first spec init that returns NULL when its
 * init pointer was non-NULL.
 *
 * teardown_worker walks the array in reverse and calls each
 * spec's teardown with the corresponding handle. Safe to call
 * with a partially-populated handles array. */
int  spec_registry_init_worker     (spec_registry_t *r, int worker_idx,
                                    void **handles, int handles_capacity);

/* Filtered variant: only specs whose `name` matches one of the
 * provided language strings get init'd. Specs not in the filter
 * are left with NULL in the handles slot. Pass `langs = NULL` to
 * init every spec (same as the unfiltered version).
 *
 * The runner uses this with the language list the graph loader
 * built (issue 305), so a Lua-only map doesn't pay the bash
 * subprocess startup cost on every worker. */
int  spec_registry_init_worker_filtered(spec_registry_t *r, int worker_idx,
                                        void **handles, int handles_capacity,
                                        const char **langs, int n_langs);

void spec_registry_teardown_worker (spec_registry_t *r,
                                    void **handles, int n_handles);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_SPEC_REGISTRY_H */
