/* src/011-spec-registry.c — language spec registry, implementation.
 *
 * Scans the langs/ directory at startup, dlopens every <name>/spec.so
 * it finds, reads the exported `soramech_lang_spec` symbol, and
 * stores name + file_ext + dl handle + spec pointer. Registry stays
 * alive for the run; spec_registry_destroy walks the entries and
 * dlcloses every handle.
 *
 * Per-worker spec init (running each spec's init callback once per
 * worker thread) lives in the pool, not here — issue 301's worker_main
 * gets the hook in a follow-on. This file just produces the lookup
 * table that the hook will iterate.
 */

#include "011-spec-registry.h"

#include <dirent.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* {{{ Internal entry */
typedef struct spec_entry {
    void             *dl_handle;
    const lang_spec_t *spec;
} spec_entry_t;
/* }}} */

/* {{{ Registry */
struct spec_registry {
    int           count;
    int           capacity;
    spec_entry_t *entries;
};
/* }}} */

/* {{{ err_fmt() */
__attribute__((format(printf, 1, 2)))
static char *err_fmt(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len);
    out[len] = '\0';
    return out;
}
/* }}} */

/* {{{ grow_if_needed() */
static int grow_if_needed(spec_registry_t *r)
{
    if (r->count < r->capacity) return 0;
    int new_cap = r->capacity ? r->capacity * 2 : 4;
    spec_entry_t *grown = realloc(r->entries, (size_t)new_cap * sizeof(spec_entry_t));
    if (!grown) return -1;
    r->entries  = grown;
    r->capacity = new_cap;
    return 0;
}
/* }}} */

/* {{{ try_load_one() — open one langs/<name>/spec.so */
/* Returns 0 on success, -1 on failure with *err set. Skips entries
 * that don't contain a spec.so silently (a subdirectory without a
 * built spec just means that language isn't available). */
static int try_load_one(spec_registry_t *r, const char *langs_dir,
                        const char *name, char **err)
{
    char path[4096];
    int w = snprintf(path, sizeof path, "%s/%s/spec.so", langs_dir, name);
    if (w < 0 || w >= (int)sizeof path) {
        *err = err_fmt("spec path too long: %s/%s", langs_dir, name);
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) return 0;   /* no spec.so; silent skip */

    /* RTLD_LOCAL keeps symbols from one spec from leaking into
     * another — a spec's internal helpers shouldn't be visible to
     * any other dlopen'd module. RTLD_NOW catches missing-symbol
     * errors immediately rather than deferring them to the first
     * call. */
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        *err = err_fmt("dlopen('%s') failed: %s", path, dlerror());
        return -1;
    }

    /* dlerror's "clear the queue" idiom: call it once before dlsym
     * to flush any pending error, then again after to read it. */
    (void)dlerror();
    const lang_spec_t *spec = dlsym(h, "soramech_lang_spec");
    const char *dl_err = dlerror();
    if (!spec || dl_err) {
        *err = err_fmt("'%s' is missing 'soramech_lang_spec': %s",
                       path, dl_err ? dl_err : "symbol not found");
        dlclose(h);
        return -1;
    }
    if (!spec->name || !spec->file_ext) {
        *err = err_fmt("'%s' exports a spec with NULL name or file_ext", path);
        dlclose(h);
        return -1;
    }

    if (grow_if_needed(r) != 0) {
        *err = err_fmt("out of memory");
        dlclose(h);
        return -1;
    }
    r->entries[r->count].dl_handle = h;
    r->entries[r->count].spec      = spec;
    r->count++;
    return 0;
}
/* }}} */

/* {{{ spec_registry_load() */
spec_registry_t *spec_registry_load(const char *langs_dir, char **err)
{
    if (err) *err = NULL;
    if (!langs_dir) {
        if (err) *err = err_fmt("spec_registry_load: NULL langs_dir");
        return NULL;
    }

    DIR *d = opendir(langs_dir);
    if (!d) {
        if (err) *err = err_fmt("cannot open langs directory '%s'", langs_dir);
        return NULL;
    }

    spec_registry_t *r = calloc(1, sizeof *r);
    if (!r) {
        closedir(d);
        if (err) *err = err_fmt("out of memory");
        return NULL;
    }

    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        /* Probe the subdir as a directory; ignore plain files. */
        char sub[4096];
        int w = snprintf(sub, sizeof sub, "%s/%s", langs_dir, e->d_name);
        if (w < 0 || w >= (int)sizeof sub) continue;
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char *one_err = NULL;
        if (try_load_one(r, langs_dir, e->d_name, &one_err) != 0) {
            closedir(d);
            spec_registry_destroy(r);
            if (err) *err = one_err;
            else     free(one_err);
            return NULL;
        }
    }
    closedir(d);
    return r;
}
/* }}} */

/* {{{ spec_registry_destroy() */
void spec_registry_destroy(spec_registry_t *r)
{
    if (!r) return;
    for (int i = 0; i < r->count; i++) {
        if (r->entries[i].dl_handle) dlclose(r->entries[i].dl_handle);
    }
    free(r->entries);
    free(r);
}
/* }}} */

/* {{{ Accessors */
int spec_registry_size(const spec_registry_t *r)
{
    return r ? r->count : 0;
}

const lang_spec_t *spec_registry_get(const spec_registry_t *r, const char *name)
{
    if (!r || !name) return NULL;
    for (int i = 0; i < r->count; i++) {
        if (strcmp(r->entries[i].spec->name, name) == 0) return r->entries[i].spec;
    }
    return NULL;
}

const lang_spec_t *spec_registry_for_ext(const spec_registry_t *r, const char *file_ext)
{
    if (!r || !file_ext) return NULL;
    for (int i = 0; i < r->count; i++) {
        if (strcmp(r->entries[i].spec->file_ext, file_ext) == 0) return r->entries[i].spec;
    }
    return NULL;
}

const lang_spec_t *spec_registry_at(const spec_registry_t *r, int i)
{
    if (!r || i < 0 || i >= r->count) return NULL;
    return r->entries[i].spec;
}
/* }}} */

/* {{{ spec_registry_init_worker() — init every spec */
int spec_registry_init_worker(spec_registry_t *r, int worker_idx,
                              void **handles, int handles_capacity)
{
    return spec_registry_init_worker_filtered(r, worker_idx,
                                              handles, handles_capacity,
                                              NULL, 0);
}
/* }}} */

/* {{{ spec_registry_init_worker_filtered() */
int spec_registry_init_worker_filtered(spec_registry_t *r, int worker_idx,
                                       void **handles, int handles_capacity,
                                       const char **langs, int n_langs)
{
    if (!r || !handles) return -1;
    if (handles_capacity < r->count) return -1;

    for (int i = 0; i < r->count; i++) handles[i] = NULL;

    for (int i = 0; i < r->count; i++) {
        const lang_spec_t *s = r->entries[i].spec;

        /* Filter: if `langs` is non-NULL, skip specs whose name
         * isn't in the list. The map's enumerated languages from
         * graph_n_languages() / graph_language() are what gets
         * passed here. */
        if (langs && n_langs > 0) {
            int included = 0;
            for (int j = 0; j < n_langs; j++) {
                if (langs[j] && strcmp(langs[j], s->name) == 0) {
                    included = 1; break;
                }
            }
            if (!included) continue;
        }

        if (!s->init) continue;          /* spec opts out of init */
        void *h = s->init(worker_idx);
        if (!h) {
            /* Teardown anything we've already created. */
            spec_registry_teardown_worker(r, handles, i);
            return -1;
        }
        handles[i] = h;
    }
    return r->count;
}
/* }}} */

/* {{{ spec_declares_target() */
/* Issue 325 — the pair-declaration lookup the load-time wire
 * walker consults. Same-language is implicitly declared because
 * identity needs no shim; everything else must appear in the
 * spec's translate_targets list. Kept as a pure function over the
 * struct so tests can probe it with synthetic specs. */
int spec_declares_target(const lang_spec_t *spec, const char *lang)
{
    if (!spec || !lang) return 0;
    if (spec->name && strcmp(spec->name, lang) == 0) return 1;
    if (!spec->translate_targets) return 0;
    for (const char *const *t = spec->translate_targets; *t; t++) {
        if (strcmp(*t, lang) == 0) return 1;
    }
    return 0;
}
/* }}} */

/* {{{ spec_registry_teardown_worker() */
void spec_registry_teardown_worker(spec_registry_t *r,
                                   void **handles, int n_handles)
{
    if (!r || !handles) return;
    if (n_handles > r->count) n_handles = r->count;
    /* Walk in reverse — symmetric with init order. */
    for (int i = n_handles - 1; i >= 0; i--) {
        const lang_spec_t *s = r->entries[i].spec;
        if (s->teardown && handles[i]) {
            s->teardown(handles[i]);
        }
        handles[i] = NULL;
    }
}
/* }}} */
