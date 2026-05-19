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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* {{{ c_compile() — gcc -shared -fPIC */
static int c_compile(const char *src_path, const char *out_path)
{
    if (!src_path || !out_path) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* Child: replace ourselves with gcc. */
        execlp("gcc", "gcc",
               "-shared", "-fPIC", "-O2", "-Wall",
               "-o", out_path, src_path, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
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
 * unchanged. The output buffer is the caller-supplied so_buf. */
static const char *maybe_lazy_compile(const char *file_path,
                                      char *so_buf, size_t so_cap)
{
    if (!ends_in_dot_c(file_path)) return file_path;
    /* Hash the source path so repeat opens of the same source file
     * pick up the same .so. Concurrent invokes will race on gcc but
     * the inputs are identical, so the output converges. */
    unsigned long h = hash_path(file_path);
    int n = snprintf(so_buf, so_cap, "/tmp/soramech-c-%lx.so", h);
    if (n < 0 || (size_t)n >= so_cap) return NULL;
    if (c_compile(file_path, so_buf) != 0) return NULL;
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

/* {{{ c_invoke() */
static int c_invoke(void *handle,
                    const char  *file_path,    /* path to a .so */
                    const char  *fn_name,
                    const void **input_data,
                    const int   *input_sizes,
                    int          n_inputs,
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
    return u.fn(input_data, input_sizes, n_inputs,
                out_buf, out_capacity, out_size);
}
/* }}} */

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "c",
    .file_ext = ".c",
    .init     = c_init,
    .teardown = c_teardown,
    .compile  = c_compile,
    .invoke   = c_invoke,
};
/* }}} */
