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
 * Issue 312 adds per-input / per-output format flags to `invoke`
 * (so a spec can decode each cell as native or JSON based on the
 * per-edge classification the loader assigned) and a pair of
 * bridges, `native_to_json` / `json_to_native`, used by the
 * dispatch's push path at language boundaries and by data boxes
 * (issue 317) that author values in their language's native
 * idioms. There is one invoke entry point per spec; the
 * format-branch lives inside that single function.
 */

#ifndef SORAMECH_LANG_SPEC_H
#define SORAMECH_LANG_SPEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration. `box_t` lives in src/010-graph-loader.h and
 * carries the full per-box record (input declarations, language,
 * compile-time hints like `cflags` / `link_libs` / `headers`).
 * Spec implementations include the loader header when they want to
 * read those fields; the bare forward declaration here keeps the
 * spec interface stand-alone for callers that only need the typedef
 * names. */
typedef struct box box_t;

/* {{{ Invoke signature
 *
 * One entry point per spec. The dispatch layer hands the spec a
 * per-input format flag and a per-output format flag that the
 * spec consults internally — no separate native / JSON variants.
 *
 * Per-input format flag (`input_native[i]`): non-zero iff the
 * bytes in `input_data[i]` are this spec's native form; zero iff
 * they are JSON from a cross-language producer. The spec
 * dispatches internally — its decoder for that input picks the
 * native or JSON path based on the flag.
 *
 * Per-output format flag (`output_native`): non-zero iff the
 * dispatch wants the spec to write native bytes to `out_buf`,
 * zero iff it wants JSON. The spec produces one form per call;
 * cross-language fan-out is handled by the dispatch layer, which
 * converts via `native_to_json` once and pushes the JSON to every
 * cross-language consumer.
 *
 * On non-dual-ring slots (single-language inputs or single-language
 * outputs only), the dispatch passes NULL for `input_native` and
 * 0 for `output_native`; the spec falls back to its default
 * decode (typically JSON, which Lua's existing bridge handles).
 * `input_native` may be NULL even when `n_inputs > 0` — specs
 * must check before dereferencing. */
typedef int (*lang_invoke_fn)(void *handle,
                              const box_t *box,
                              const char *file_path,
                              const char *fn_name,
                              const void **input_data,
                              const int   *input_sizes,
                              const int   *input_native,
                              int          n_inputs,
                              int          output_native,
                              void        *out_buf,
                              int          out_buf_capacity,
                              int         *out_size);

/* `box` is the box record being invoked, or NULL when the caller has
 * no box context (e.g. test fixtures driving the spec directly).
 * Specs that need declared return type, compile hints, or future
 * sentinel-emission decisions read fields off `box`. Specs that
 * don't need it ignore the parameter. The pointer is valid only for
 * the duration of the call. */

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
     * gcc -shared -fPIC -o out_path src_path, plus any per-box
     * compile flags / link libraries / headers the box declared.
     *
     * `box` is the box record for the box being compiled, or NULL
     * if the caller is compiling a source file with no box context
     * (e.g. the C spec's internal lazy-compile path that maps a raw
     * `.c` ref to a `/tmp/<hash>.so` on the fly). When non-NULL,
     * the spec may read box->cflags / box->link_libs / box->headers
     * to customise the compile. Returns 0 on success, nonzero on
     * failure. */
    int   (*compile)(const char *src_path, const char *out_path,
                     const box_t *box);

    /* invoke: per box call. Inputs are arrays of (bytes, size) pairs,
     * already read from slots by the dispatch layer. The per-input
     * `input_native[i]` flag tells the spec whether each cell is in
     * the spec's native byte form (cell came from a same-language
     * producer via `ring_native`) or in JSON (came from a
     * cross-language producer via `ring_json`). The `output_native`
     * flag tells the spec which form to write; cross-language
     * fan-out is materialised by the dispatch via `native_to_json`
     * before pushing the JSON ring. Returns 0 on success, nonzero
     * (fatal) on error. Issue 312. */
    lang_invoke_fn invoke;

    /* Bridges between this spec's native byte form and JSON. Used
     * by the dispatch's push path when a producer writes a
     * cross-language wire (call `native_to_json` once on the
     * producer side, push JSON to the consumer's `ring_json`) and
     * by data boxes that author values in their language's native
     * idioms (issue 317). `native_to_json` reads from the
     * producer's runtime working store (e.g. lua_State stack);
     * `json_to_native` writes into the consumer's runtime working
     * store. The buffer-oriented signature accommodates stateless
     * specs (C, Bash) that treat the value form as bytes; stateful
     * specs (Lua, future Python / Ruby) ignore the unused half. */
    lang_bridge_fn native_to_json;
    lang_bridge_fn json_to_native;
} lang_spec_t;
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_LANG_SPEC_H */
