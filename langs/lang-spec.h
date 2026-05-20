/* langs/lang-spec.h — public interface for SoraMech language specs.
 *
 * What it is, in a sentence: the C contract that every language
 * spec (Lua, C, Bash, anything user-added) implements. The pool
 * runner discovers specs by dlopen()ing langs/<name>/spec.so at
 * startup and reading the exported soramech_lang_spec symbol.
 *
 * Designed in issue 303. The init barrier (issue 301) calls init()
 * for every spec on every worker before unblocking task dispatch;
 * the dispatch action (issue 304) calls invoke() per task; the
 * compile-button pipeline (issue 309) calls compile() per source
 * file for languages whose compile pointer is non-NULL.
 *
 * Errors: a nonzero return from any callback is fatal. The dispatch
 * layer prints the error context and aborts. There is no recovery,
 * no retry, no per-task failed state. This is intentional.
 *
 * Issue 312 extends this contract with optional fast-path
 * callbacks: `invoke_native` and `invoke_json` are specialised
 * variants the dispatch layer prefers when wire classification
 * says they apply; `native_to_json` and `json_to_native` are
 * bridges used at language boundaries. Specs that don't set them
 * fall back to `invoke` for every call.
 */

#ifndef SORAMECH_LANG_SPEC_H
#define SORAMECH_LANG_SPEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Shared signatures */
/* Invoke signature — shared by `invoke`, `invoke_native`, and
 * `invoke_json`. The dispatch layer picks which one to call based
 * on the wire-format classification in issue 312. */
typedef int (*lang_invoke_fn)(void *handle,
                              const char *file_path,
                              const char *fn_name,
                              const void **input_data,
                              const int   *input_sizes,
                              int          n_inputs,
                              void        *out_buf,
                              int          out_buf_capacity,
                              int         *out_size);

/* Wire-format bridge: convert one direction of native ↔ JSON.
 * Used when a value crosses a language boundary. Spec sets one
 * or both; if NULL, the dispatch layer treats native and JSON as
 * identical byte streams (the current uniform-byte default).
 *
 * `handle` is the worker's per-language handle (the same one
 * `invoke` receives) — bridges may need it to reach a runtime-side
 * scratch area: Lua reads/writes via the lua_State stack, C may
 * keep a typed-pointer table, Bash may need its sub-process. Specs
 * that don't need it ignore the argument.
 *
 * Stack-protocol convention for `native_to_json`: the producer
 * leaves its native value on top of its runtime's working store
 * (Lua: top of stack); the bridge consumes it. For `json_to_native`:
 * the bridge pushes the reconstructed value onto the same store.
 * `src` / `dst` carry only the JSON side. */
typedef int (*lang_bridge_fn)(void *handle,
                              const void *src, int src_size,
                              void *dst, int dst_capacity, int *dst_size);
/* }}} */

/* {{{ lang_spec_t */
typedef struct lang_spec {
    const char *name;       /* "lua", "bash", "c", "python", ... */
    const char *file_ext;   /* ".lua", ".sh", ".c", ".py"        */

    /* init: once per worker thread at pool startup. Returns an
     * opaque handle stored in the worker context; the dispatch
     * layer passes this handle back to invoke() for every call. */
    void *(*init)(int worker_idx);

    /* teardown: once per worker at pool shutdown. Frees the handle
     * and any per-worker resources (e.g. lua_State, dlopen caches,
     * persistent socket connections). */
    void  (*teardown)(void *handle);

    /* compile: optional. Called once per source file at map-load
     * (or at editor compile-button time). For languages that don't
     * compile (Lua, Bash), leave this NULL. For C, it runs
     * gcc -shared -fPIC -o out_path src_path. Returns 0 on
     * success, nonzero on failure. */
    int   (*compile)(const char *src_path, const char *out_path);

    /* invoke: per box call. Inputs are arrays of (bytes, size) pairs,
     * already read from slots by the dispatch layer. Returns the
     * function's output by writing to out_buf and setting *out_size.
     * Returns 0 on success, nonzero (fatal) on error.
     *
     * This is the canonical entry point; every spec implements it.
     * The dispatch layer falls back to it when the specialised
     * paths below are not set. */
    lang_invoke_fn invoke;

    /* invoke_native — optional fast path used when every wire
     * adjacent to this box (input and output) stays inside this
     * spec's language. Bytes on the wire are this language's
     * native serialisation (Lua tables via msgpack, C struct
     * memcpy, etc.) and the encode/decode round trip is skipped.
     * NULL means the spec hasn't specialised; dispatch falls
     * back to `invoke`. Issue 312. */
    lang_invoke_fn invoke_native;

    /* invoke_json — optional universal-interop path. Bytes on the
     * wire are JSON. NULL means the spec hasn't specialised;
     * dispatch falls back to `invoke`. Issue 312. */
    lang_invoke_fn invoke_json;

    /* Bridges between native and JSON wire forms. Used when a
     * value crosses a language boundary — the producer writes
     * native, the dispatch layer converts via the producer's
     * native_to_json, hands the JSON bytes to the consumer's
     * json_to_native. NULL means the spec hasn't specialised;
     * dispatch treats native and JSON as identical bytes. Issue
     * 312. */
    lang_bridge_fn native_to_json;
    lang_bridge_fn json_to_native;
} lang_spec_t;
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_LANG_SPEC_H */
