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
 * The fast-path callbacks (invoke_native, native_to_json,
 * json_to_native) from issue 312 are not in this initial header —
 * they land when 312 is implemented.
 */

#ifndef SORAMECH_LANG_SPEC_H
#define SORAMECH_LANG_SPEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
     * Returns 0 on success, nonzero (fatal) on error. */
    int   (*invoke)(void *handle,
                    const char *file_path,
                    const char *fn_name,
                    const void **input_data,
                    const int   *input_sizes,
                    int          n_inputs,
                    void        *out_buf,
                    int          out_buf_capacity,
                    int         *out_size);
} lang_spec_t;
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_LANG_SPEC_H */
