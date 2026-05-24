/* langs/c/spec.c — C language spec for SoraMech.
 *
 * Per-worker handle is a small dlopen cache. `compile` runs gcc
 * to turn a box source file into a .so; `invoke` dlopens the .so
 * (caching the handle), dlsyms the named function, and calls it
 * through a fixed signature.
 *
 * Designed in issue 307. C box functions follow a single
 * convention that matches the spec's `invoke` shape minus the
 * per-worker handle:
 *
 *     int box_fn(const void **inputs, const int *sizes, int n,
 *                void *out_buf, int out_capacity, int *out_size);
 *
 * Return 0 on success, nonzero (fatal) on failure. This iteration
 * does not generate per-box wrappers from declared signatures
 * (issue 307's optional advanced mode) — every C box uses the
 * fixed signature above. Wrapper generation can land as a follow-on
 * once we have a box whose declared types diverge from the byte-
 * array shape.
 */

#include "lang-spec.h"
#include "json.h"
#include "010-graph-loader.h"
#include "020-sentinels.h"  /* SENTINEL_MASK_* — issue 318 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define C_CACHE_SIZE 32

/* {{{ Per-worker dlopen cache */
typedef struct {
    char *path;     /* strdup'd; identifies the cache entry         */
    void *dl;       /* dlopen handle                                */
} c_cache_entry_t;

typedef struct {
    c_cache_entry_t entries[C_CACHE_SIZE];
    int             count;
} c_handle_t;

typedef int (*c_box_fn_t)(const void **inputs, const int *sizes, int n,
                          void *out_buf, int out_capacity, int *out_size);
/* }}} */

/* {{{ c_init() */
static void *c_init(int worker_idx)
{
    (void)worker_idx;
    c_handle_t *h = calloc(1, sizeof *h);
    return h;
}
/* }}} */

/* {{{ c_teardown() */
static void c_teardown(void *handle)
{
    if (!handle) return;
    c_handle_t *h = (c_handle_t *)handle;
    for (int i = 0; i < h->count; i++) {
        if (h->entries[i].dl) dlclose(h->entries[i].dl);
        free(h->entries[i].path);
    }
    free(h);
}
/* }}} */

/* {{{ split_cflags() — split a cflags string on whitespace
 *
 * gcc wants each token as a separate argv entry. The user's cflags
 * is a single string like "-O0 -g -mavx2" — we split it in place
 * (modifying `buf`) and stash the token pointers into `tokens`.
 * Returns the number of tokens; -1 on overflow. Tokens point into
 * `buf` and are valid for as long as `buf` is. */
static int split_cflags(char *buf, char **tokens, int max_tokens)
{
    int n = 0;
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (n >= max_tokens) return -1;
        tokens[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}
/* }}} */

/* {{{ c_compile() — gcc -shared -fPIC, plus per-box hints from box JSON
 *
 * When `box` is non-NULL the box's cflags / link_libs hints are
 * appended to the gcc command. Issue 307 items 3+4. When `box` is
 * NULL the call is the "no box context" path (e.g. the internal
 * lazy-compile when the dispatch hands the spec a raw `.c` ref) —
 * the default flags are still applied, no extras. */
static int c_compile(const char *src_path, const char *out_path,
                     const box_t *box)
{
    if (!src_path || !out_path) return -1;

    /* Bounded argv: 16 fixed slots for "-shared -fPIC -O2 -Wall
     * -o out src" plus headroom, 32 for cflags tokens, 32 for
     * link-lib `-l` pairs. Anything larger is unusual and the
     * caller can break up the work into multiple boxes. */
    enum { ARGV_CAP = 96, CFLAG_TOK_CAP = 32 };
    const char *argv_view[ARGV_CAP];
    int n = 0;
    argv_view[n++] = "gcc";
    argv_view[n++] = "-shared";
    argv_view[n++] = "-fPIC";
    argv_view[n++] = "-O2";
    argv_view[n++] = "-Wall";
    /* Default include path: the project's langs/c dir, so user
     * box source can `#include "soramech.h"` for the runtime
     * self-construction bindings (issue 319e). */
    argv_view[n++] = "-I" SORAMECH_LANGS_C_INCLUDE;

    /* Per-box cflags: split on whitespace into individual tokens. */
    char  cflags_buf[1024];
    char *cflag_tokens[CFLAG_TOK_CAP];
    int   n_cflag_tokens = 0;
    if (box && box->cflags && box->cflags[0]) {
        size_t len = strlen(box->cflags);
        if (len >= sizeof cflags_buf) {
            fprintf(stderr, "c spec: cflags too long for box '%s'\n", box->id);
            return -1;
        }
        memcpy(cflags_buf, box->cflags, len + 1);
        n_cflag_tokens = split_cflags(cflags_buf, cflag_tokens, CFLAG_TOK_CAP);
        if (n_cflag_tokens < 0) {
            fprintf(stderr, "c spec: too many cflag tokens for box '%s'\n",
                    box->id);
            return -1;
        }
        for (int i = 0; i < n_cflag_tokens; i++) {
            if (n >= ARGV_CAP) {
                fprintf(stderr, "c spec: argv overflow for box '%s'\n", box->id);
                return -1;
            }
            argv_view[n++] = cflag_tokens[i];
        }
    }

    argv_view[n++] = "-o";
    argv_view[n++] = out_path;
    argv_view[n++] = src_path;

    /* Per-box link libs: each becomes a `-l<name>` argv entry. The
     * combined "-l" + name lives in a small per-lib heap allocation
     * — n_link_libs is small so the cost is negligible. */
    char *lflag_buf[ARGV_CAP];
    int   n_lflags = 0;
    if (box && box->link_libs && box->n_link_libs > 0) {
        for (int i = 0; i < box->n_link_libs; i++) {
            const char *libname = box->link_libs[i];
            if (!libname || !*libname) continue;
            size_t buflen = strlen(libname) + 3;   /* "-l" + name + NUL */
            char *s = malloc(buflen);
            if (!s) {
                for (int j = 0; j < n_lflags; j++) free(lflag_buf[j]);
                fprintf(stderr, "c spec: oom assembling link_libs for '%s'\n",
                        box->id);
                return -1;
            }
            snprintf(s, buflen, "-l%s", libname);
            if (n >= ARGV_CAP) {
                for (int j = 0; j < n_lflags; j++) free(lflag_buf[j]);
                free(s);
                fprintf(stderr, "c spec: argv overflow for box '%s'\n", box->id);
                return -1;
            }
            lflag_buf[n_lflags++] = s;
            argv_view[n++] = s;
        }
    }

    if (n >= ARGV_CAP) {
        for (int j = 0; j < n_lflags; j++) free(lflag_buf[j]);
        return -1;
    }
    argv_view[n] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        for (int j = 0; j < n_lflags; j++) free(lflag_buf[j]);
        return -1;
    }
    if (pid == 0) {
        /* Child: replace ourselves with gcc. execvp wants
         * (char *const *), and we built (const char *) entries —
         * the cast is safe because execvp doesn't modify them. */
        execvp("gcc", (char *const *)(void *)argv_view);
        _exit(127);
    }
    int status = 0;
    int wait_rc = waitpid(pid, &status, 0);
    for (int j = 0; j < n_lflags; j++) free(lflag_buf[j]);
    if (wait_rc < 0) return -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "c spec: gcc failed for '%s' → '%s' (status=%d)\n",
                src_path, out_path, status);
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ ends_in_dot_c() */
static int ends_in_dot_c(const char *p)
{
    size_t n = strlen(p);
    return n >= 2 && p[n - 2] == '.' && p[n - 1] == 'c';
}
/* }}} */

/* {{{ hash_path() — small DJB2-like hash for deriving cache filenames */
static unsigned long hash_path(const char *p)
{
    unsigned long h = 5381;
    for (; *p; p++) h = h * 33u + (unsigned char)*p;
    return h;
}
/* }}} */

/* {{{ maybe_lazy_compile() — if file_path is a .c source, compile it
 * to a /tmp .so and return that path; otherwise return file_path
 * unchanged. The output buffer is the caller-supplied so_buf.
 *
 * Issue 313 follow-on (C eager-precompile): if the compile
 * pipeline (scripts/soramech-compile.sh) ran for this map, it
 * already produced a per-box .so in <map_dir>/bin/<basename>.so.
 * Prefer that artifact over re-running gcc at dispatch time.
 * Falls through to lazy compile when running from a source tree
 * the compile pipeline hasn't touched (i.e. the live `maps/`
 * editor flow). */
static const char *maybe_lazy_compile(const char *file_path,
                                      char *so_buf, size_t so_cap)
{
    if (!ends_in_dot_c(file_path)) return file_path;

    /* Try compiled artifact first. The pipeline puts each .c box's
     * .so at <prefix>/bin/<basename>.so where <prefix> is the
     * compiled map dir. file_path here is absolute, looking like
     * "<compiled-map-dir>/src/<basename>.c"; transform to
     * "<compiled-map-dir>/bin/<basename>.so" and use it if it
     * exists. */
    const char *base = strrchr(file_path, '/');
    const char *src_seg = strstr(file_path, "/src/");
    if (base && src_seg) {
        char precompiled[4096];
        int prefix_len = (int)(src_seg - file_path);
        const char *fname = base + 1;
        const char *ext   = strrchr(fname, '.');
        int name_len = ext ? (int)(ext - fname) : (int)strlen(fname);
        int n = snprintf(precompiled, sizeof precompiled,
                         "%.*s/bin/%.*s.so",
                         prefix_len, file_path, name_len, fname);
        if (n > 0 && (size_t)n < sizeof precompiled) {
            struct stat st;
            if (stat(precompiled, &st) == 0) {
                int copy_n = snprintf(so_buf, so_cap, "%s", precompiled);
                if (copy_n > 0 && (size_t)copy_n < so_cap) return so_buf;
            }
        }
    }

    /* Hash the source path so repeat opens of the same source file
     * pick up the same .so. Concurrent invokes will race on gcc but
     * the inputs are identical, so the output converges. */
    unsigned long h = hash_path(file_path);
    int n = snprintf(so_buf, so_cap, "/tmp/soramech-c-%lx.so", h);
    if (n < 0 || (size_t)n >= so_cap) return NULL;
    /* No box context on this internal lazy-compile path; the caller
     * passed us a raw `.c` ref and we just need a default build. */
    if (c_compile(file_path, so_buf, NULL) != 0) return NULL;
    return so_buf;
}
/* }}} */

/* {{{ get_or_load() — find a cached .so or dlopen and add */
static void *get_or_load(c_handle_t *h, const char *path)
{
    for (int i = 0; i < h->count; i++) {
        if (strcmp(h->entries[i].path, path) == 0) return h->entries[i].dl;
    }
    if (h->count >= C_CACHE_SIZE) {
        fprintf(stderr, "c spec: dlopen cache full (%d entries)\n", C_CACHE_SIZE);
        return NULL;
    }
    void *dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        fprintf(stderr, "c spec: dlopen('%s'): %s\n", path, dlerror());
        return NULL;
    }
    h->entries[h->count].path = strdup(path);
    h->entries[h->count].dl   = dl;
    h->count++;
    return dl;
}
/* }}} */

/* {{{ apply_typed_json_output() — issue 307 item 2
 *
 * The user's C function has already written its raw bytes into
 * out_buf and set *out_size. When the dispatch asked for JSON
 * output (output_native == 0) we rewrap those bytes per the box's
 * declared return type so the cross-language consumer receives
 * a parseable JSON value.
 *
 * Returns 0 on success, -1 on buffer-overflow / encoding error.
 *
 * - `string`: wrap in JSON quotes with proper escaping for
 *    quotes, backslashes, control characters, etc. The bytes
 *    might not be NUL-terminated, so we copy them into a
 *    NUL-terminated scratch before feeding json_writer_string.
 * - `int` / `double` / `bool` / `null` / `json` / `bytes`:
 *    passthrough. The user's bytes are already a valid JSON
 *    fragment ("42" is a JSON number; "true" is a JSON bool;
 *    user-declared "json" returns mean the function chose its
 *    own JSON serialization). For `bytes` proper base64 wrapping
 *    is a follow-on; raw passthrough keeps the door open for
 *    typed-wrapper generation that can do it right.
 * - `void`: empty output regardless of what the user wrote.
 * - NULL / unknown: passthrough (safest default; matches the
 *    pre-typed behavior). */
static int apply_typed_json_output(char *out_buf, int out_capacity,
                                   int *out_size, const char *returns)
{
    if (!out_size) return 0;
    if (!returns)  return 0;    /* unknown type → passthrough */

    if (strcmp(returns, "void") == 0) {
        *out_size = 0;
        return 0;
    }
    if (strcmp(returns, "string") == 0) {
        /* Stash the user's bytes (small bound — typical primitive
         * outputs fit in a few KB; if they overflow this scratch,
         * the JSON-encoded form wouldn't fit out_buf either). */
        char scratch[8192];
        int n = *out_size;
        if (n < 0 || n >= (int)sizeof scratch) return -1;
        memcpy(scratch, out_buf, (size_t)n);
        scratch[n] = '\0';

        json_writer_t w;
        json_writer_init(&w, out_buf, out_capacity);
        json_writer_string(&w, scratch);
        int written = json_writer_finish(&w);
        if (written < 0) return -1;
        *out_size = written;
        return 0;
    }
    /* int / double / bool / null / json / bytes / anything else:
     * passthrough. Already valid JSON or the spec has no better
     * shape without typed wrapper generation. */
    return 0;
}
/* }}} */

/* {{{ try_strip_json_primitive() — issue 307 JSON-input acceptance
 *
 * Attempts to parse `src` (length `src_size`, not necessarily NUL-
 * terminated) as JSON. If it's a JSON string, writes the unquoted
 * content into `scratch` and returns 1 with `*out_data` pointing at
 * the scratch buffer and `*out_size` set to the unquoted length.
 * If it's a JSON number / bool, leaves the bytes alone and returns
 * 0 (the JSON representation of these is the same shape the C
 * function expects — "42" parses with atol, "true"/"false" with
 * strcmp, "null" with strcmp). Returns 0 in all "use the raw bytes"
 * cases including parse failures and non-primitive JSON. The C
 * function only sees the substituted bytes when this returns 1.
 *
 * This is the symmetric of Lua's input-side parse-with-fallback —
 * cross-language producers that emit JSON (Lua, future Python /
 * Node etc.) get their primitive strings unwrapped here. JSON
 * arrays / objects fall through; handling those needs typed
 * wrappers (the deferred 307 work). */
static int try_strip_json_primitive(const void *src, int src_size,
                                    char *scratch, int scratch_cap,
                                    const void **out_data, int *out_size)
{
    if (src_size <= 0) return 0;

    /* json_parse needs a NUL-terminated buffer. Copy into a small
     * heap allocation rather than a fixed-size stack array because
     * input sizes vary; the allocator's per-call cost is negligible
     * compared to the cross-language wire-format work. */
    char *nul_buf = (char *)malloc((size_t)src_size + 1);
    if (!nul_buf) return 0;
    memcpy(nul_buf, src, (size_t)src_size);
    nul_buf[src_size] = '\0';

    json_arena_t *arena = json_arena_create();
    if (!arena) { free(nul_buf); return 0; }

    int err_offset = 0;
    const char *err_msg = NULL;
    json_node_t *node = json_parse(arena, nul_buf, &err_offset, &err_msg);
    free(nul_buf);

    if (!node) { json_arena_destroy(arena); return 0; }

    json_kind_t kind = json_kind(node);
    int rc = 0;
    if (kind == JSON_STRING) {
        const char *s = json_string_value(node);
        int len = (int)strlen(s);
        if (len < scratch_cap) {
            memcpy(scratch, s, (size_t)len);
            scratch[len] = '\0';
            *out_data = scratch;
            *out_size = len;
            rc = 1;
        }
    }
    /* Other primitives (NUMBER / BOOL / NULL): raw bytes already
     * match what the C function expects. Containers (ARRAY /
     * OBJECT): no clean unwrap without typed wrappers — let the
     * raw bytes pass through and surface whatever the function
     * does with them. */

    json_arena_destroy(arena);
    return rc;
}
/* }}} */

/* {{{ c_invoke() */
static int c_invoke(void *handle,
                    const box_t *box,
                    const char  *file_path,    /* path to a .so */
                    const char  *fn_name,
                    const void **input_data,
                    const int   *input_sizes,
                    const int   *input_native,
                    int          n_inputs,
                    int          output_native,
                    void        *out_buf,
                    int          out_capacity,
                    int         *out_size)
{
    c_handle_t *h = (c_handle_t *)handle;
    if (!h || !file_path || !fn_name) return -1;

    /* If a graph passed a .c source path, build a /tmp .so for it on
     * the fly and proceed with that. */
    char so_buf[4096];
    const char *to_open = maybe_lazy_compile(file_path, so_buf, sizeof so_buf);
    if (!to_open) return -1;
    void *dl = get_or_load(h, to_open);
    if (!dl) return -1;

    /* dlsym returns void*, but the cast to a function pointer is
     * formally implementation-defined; POSIX requires it to work.
     * The union trick avoids -Wpedantic noise. */
    union { void *p; c_box_fn_t fn; } u;
    (void)dlerror();
    u.p = dlsym(dl, fn_name);
    const char *err = dlerror();
    if (!u.p || err) {
        fprintf(stderr, "c spec: dlsym('%s' in '%s'): %s\n",
                fn_name, file_path, err ? err : "symbol not found");
        return -1;
    }

    /* Issue 307 JSON-input acceptance: when input_native says the
     * bytes are JSON (i.e. the producer is in a different language),
     * try to unwrap JSON primitives so the user's C function sees
     * the underlying value. Substituted bytes live in per-input
     * scratch buffers on the stack — the call returns before any
     * of them go out of scope, so a pointer table referencing them
     * is safe to hand to the user function. Inputs whose flag says
     * native, or whose JSON parse fails, or whose JSON shape is a
     * container that can't be unwrapped without typed wrappers,
     * pass through unchanged. */
    enum { C_MAX_INPUTS = 16, C_SCRATCH_SIZE = 4096 };
    if (n_inputs > C_MAX_INPUTS) {
        fprintf(stderr, "c spec: invoke '%s' has %d inputs (cap %d)\n",
                fn_name, n_inputs, C_MAX_INPUTS);
        return -1;
    }
    const void *eff_data[C_MAX_INPUTS];
    int         eff_size[C_MAX_INPUTS];
    char        scratch[C_MAX_INPUTS][C_SCRATCH_SIZE];
    for (int i = 0; i < n_inputs; i++) {
        eff_data[i] = input_data[i];
        eff_size[i] = input_sizes[i];
        if (input_native && input_native[i] == 0) {
            try_strip_json_primitive(input_data[i], input_sizes[i],
                                     scratch[i], C_SCRATCH_SIZE,
                                     &eff_data[i], &eff_size[i]);
            /* try_strip_* leaves eff_data / eff_size untouched on
             * the "use raw bytes" path — no else branch needed. */
        }
    }

    int rc = u.fn(eff_data, eff_size, n_inputs,
                  out_buf, out_capacity, out_size);
    if (rc != 0) return rc;

    /* Output-side typed JSON wrap. When the dispatch wants JSON
     * (output_native == 0) and the box declared a return type, the
     * user's raw bytes get reshaped — strings become JSON-quoted,
     * primitives pass through. Without a declared return type the
     * spec leaves the bytes alone (current behavior). */
    if (!output_native && box && box->returns) {
        if (apply_typed_json_output(out_buf, out_capacity,
                                    out_size, box->returns) != 0) {
            fprintf(stderr, "c spec: JSON-encode return for '%s' failed "
                            "(declared %s)\n",
                    box->id ? box->id : fn_name, box->returns);
            return -1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ c_native_to_json() — issue 317 bridge
 *
 * C functions in this spec write bytes directly into the output
 * buffer (ASCII for primitives, raw for `bytes`). When those bytes
 * cross a language boundary as JSON we adapt them:
 *
 *   - If the bytes already parse as JSON of any kind, pass through.
 *     `42`, `true`, `"hello"`, `{...}`, `[...]` all qualify and the
 *     producer's intent is preserved as-is.
 *   - Otherwise wrap as a JSON string with proper escaping. A C
 *     function that did `strcpy(out, "hello")` writes raw `hello`;
 *     the bridge sees that, can't parse it, and emits `"hello"`.
 *
 * The `handle` argument is ignored — there's no per-worker state to
 * read. This pairs with try_strip_json_primitive / apply_typed_json_output
 * inside invoke, which do the same shaping along the call path. */
static int c_native_to_json(void *handle,
                            const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size)
{
    (void)handle;
    if (!src || src_size < 0 || !dst || dst_capacity <= 0) return -1;

    char *scratch = malloc((size_t)src_size + 1);
    if (!scratch) return -1;
    memcpy(scratch, src, (size_t)src_size);
    scratch[src_size] = '\0';

    json_arena_t *arena = json_arena_create();
    if (!arena) { free(scratch); return -1; }
    json_node_t *n = json_parse(arena, scratch, NULL, NULL);
    int valid = (n != NULL);
    json_arena_destroy(arena);

    if (valid) {
        if (src_size > dst_capacity) {
            free(scratch);
            fprintf(stderr, "c spec: native_to_json: src %d > dst %d\n",
                    src_size, dst_capacity);
            return -1;
        }
        memcpy(dst, src, (size_t)src_size);
        if (dst_size) *dst_size = src_size;
        free(scratch);
        return 0;
    }

    json_writer_t w;
    json_writer_init(&w, (char *)dst, dst_capacity);
    json_writer_string(&w, scratch);
    int written = json_writer_finish(&w);
    free(scratch);
    if (written < 0) {
        fprintf(stderr, "c spec: native_to_json: output buffer too small\n");
        return -1;
    }
    if (dst_size) *dst_size = written;
    return 0;
}
/* }}} */

/* {{{ c_json_to_native() — issue 317 bridge
 *
 * The C side wants raw bytes — what a C function would naturally
 * read via `(const char *)inputs[i]`. The bridge mirrors
 * try_strip_json_primitive: if `src` is a JSON string, write the
 * unquoted bytes into `dst`; if it's a JSON primitive (number /
 * bool / null), pass the textual representation through; otherwise
 * pass through unchanged (arrays / objects keep their JSON form for
 * a future typed-wrapper consumer to walk). */
static int c_json_to_native(void *handle,
                            const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size)
{
    (void)handle;
    if (!src || src_size < 0 || !dst || dst_capacity <= 0) return -1;

    const void *out_data = src;
    int out_n = src_size;
    int unwrapped = try_strip_json_primitive(src, src_size,
                                             (char *)dst, dst_capacity,
                                             &out_data, &out_n);
    if (unwrapped && out_data == dst) {
        /* try_strip already wrote into dst; only out_n matters. */
        if (dst_size) *dst_size = out_n;
        return 0;
    }
    /* Either no JSON unwrap happened or the result still points at
     * the original src (numeric / bool passthrough). Copy through. */
    if (out_n > dst_capacity) {
        fprintf(stderr, "c spec: json_to_native: src %d > dst %d\n",
                out_n, dst_capacity);
        return -1;
    }
    memmove(dst, out_data, (size_t)out_n);
    if (dst_size) *dst_size = out_n;
    return 0;
}
/* }}} */

/* {{{ c_translate() — per-port custom translation shim (issue 246)
 *
 * The shim is a .c file the user wrote. Its exported function name
 * is `sm_translate` with the signature documented in the issue.
 * Lazy-compile + dlopen + dlsym, cached by path in the same
 * per-worker handle the C spec uses for box bodies. */
typedef int (*c_shim_fn_t)(const void *raw, int raw_size, int raw_native,
                           void *out_buf, int out_capacity, int *out_size);

static int c_translate(void *handle,
                       const char *shim_path,
                       const void *raw, int raw_size, int raw_native,
                       void *out_buf, int out_capacity, int *out_size)
{
    c_handle_t *h = (c_handle_t *)handle;
    if (!h || !shim_path) return -1;

    char so_buf[4096];
    const char *to_open = maybe_lazy_compile(shim_path, so_buf, sizeof so_buf);
    if (!to_open) {
        fprintf(stderr, "c spec: shim compile failed for '%s'\n", shim_path);
        return -1;
    }
    void *dl = get_or_load(h, to_open);
    if (!dl) return -1;
    c_shim_fn_t fn = (c_shim_fn_t)dlsym(dl, "sm_translate");
    if (!fn) {
        fprintf(stderr, "c spec: shim '%s' has no sm_translate symbol\n",
                shim_path);
        return -1;
    }
    return fn(raw, raw_size, raw_native, out_buf, out_capacity, out_size);
}
/* }}} */

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name           = "c",
    .file_ext       = ".c",
    .init           = c_init,
    .teardown       = c_teardown,
    .compile        = c_compile,
    .invoke         = c_invoke,
    .native_to_json = c_native_to_json,
    .json_to_native = c_json_to_native,
    .translate      = c_translate,
    /* Issue 318: C spec sentinel capabilities. Slice-1 supports
     * $ref both ways via the process-wide ref store; sentinel emit
     * from the typed-output JSON wrapper is a future enhancement
     * (the C spec's encode currently passes typed values through
     * without sentinel emission), so emit_mask only declares what
     * the spec can in principle write today. */
    .sentinel_emit_mask        = SENTINEL_MASK_REF,
    .sentinel_reconstruct_mask = SENTINEL_MASK_REF,
};
/* }}} */
