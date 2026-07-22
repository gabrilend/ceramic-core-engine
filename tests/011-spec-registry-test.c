/* tests/011-spec-registry-test.c — unit tests for the spec
 * registry.
 *
 * Exercises directory scan, dlopen, symbol lookup, and the name /
 * extension accessors. Uses the langs/{lua,c,bash}/spec.so stubs
 * that ship with the build, so this test only runs from the
 * project root (where `langs/` is reachable).
 */

#include "011-spec-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* {{{ Test harness */
static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "      %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 0; \
    } } while (0)

#define RUN(name) \
    do { \
        fprintf(stdout, "  %-44s ", #name); fflush(stdout); \
        if (test_##name()) { fprintf(stdout, "ok\n"); g_pass++; } \
        else                { fprintf(stdout, "FAIL\n"); g_fail++; } \
    } while (0)
/* }}} */

/* {{{ test_load_three_specs() */
static int test_load_three_specs(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    if (!r) {
        fprintf(stderr, "      spec_registry_load failed: %s\n", err ? err : "(null)");
        free(err);
        return 0;
    }
    ASSERT(spec_registry_size(r) == 3);

    /* Order isn't guaranteed (depends on readdir), but all three
     * names must be reachable by lookup. */
    const lang_spec_t *lua  = spec_registry_get(r, "lua");
    const lang_spec_t *cspec = spec_registry_get(r, "c");
    const lang_spec_t *bash = spec_registry_get(r, "bash");
    ASSERT(lua  && strcmp(lua ->file_ext, ".lua") == 0);
    ASSERT(cspec&& strcmp(cspec->file_ext, ".c")  == 0);
    ASSERT(bash && strcmp(bash->file_ext, ".sh")  == 0);

    /* By extension */
    ASSERT(spec_registry_for_ext(r, ".lua") == lua);
    ASSERT(spec_registry_for_ext(r, ".c")   == cspec);
    ASSERT(spec_registry_for_ext(r, ".sh")  == bash);
    ASSERT(spec_registry_for_ext(r, ".rs")  == NULL);

    /* By unknown name */
    ASSERT(spec_registry_get(r, "rust") == NULL);

    /* Enumeration */
    int seen_lua = 0, seen_c = 0, seen_bash = 0;
    for (int i = 0; i < spec_registry_size(r); i++) {
        const lang_spec_t *s = spec_registry_at(r, i);
        ASSERT(s);
        if (strcmp(s->name, "lua")  == 0) seen_lua  = 1;
        if (strcmp(s->name, "c")    == 0) seen_c    = 1;
        if (strcmp(s->name, "bash") == 0) seen_bash = 1;
    }
    ASSERT(seen_lua && seen_c && seen_bash);

    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_missing_dir() */
static int test_missing_dir(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("does/not/exist", &err);
    ASSERT(r == NULL);
    ASSERT(err != NULL);
    free(err);
    return 1;
}
/* }}} */

/* {{{ test_null_dir() */
static int test_null_dir(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load(NULL, &err);
    ASSERT(r == NULL);
    ASSERT(err != NULL);
    free(err);
    return 1;
}
/* }}} */

/* {{{ test_null_safety() */
static int test_null_safety(void)
{
    /* All accessors must tolerate NULL registry. */
    ASSERT(spec_registry_size(NULL) == 0);
    ASSERT(spec_registry_get(NULL, "lua") == NULL);
    ASSERT(spec_registry_for_ext(NULL, ".lua") == NULL);
    ASSERT(spec_registry_at(NULL, 0) == NULL);
    spec_registry_destroy(NULL); /* must not crash */
    return 1;
}
/* }}} */

/* {{{ test_worker_init_teardown() */
/* Calls spec_registry_init_worker against the real shipped specs.
 * The Lua spec has a real init that returns a lua_State; C and
 * Bash currently have NULL init pointers (stubs), so they
 * contribute NULL handles. Verifies the lua handle is non-NULL,
 * the others are NULL, and teardown walks cleanly. */
static int test_worker_init_teardown(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);

    int n = spec_registry_size(r);
    ASSERT(n == 3);

    void *handles[8] = {0};
    int got = spec_registry_init_worker(r, 0, handles, (int)(sizeof handles / sizeof *handles));
    ASSERT(got == 3);

    /* Find the lua spec's index and assert its handle is non-NULL. */
    int lua_idx = -1, c_idx = -1, bash_idx = -1;
    for (int i = 0; i < n; i++) {
        const lang_spec_t *s = spec_registry_at(r, i);
        if (strcmp(s->name, "lua")  == 0) lua_idx = i;
        if (strcmp(s->name, "c")    == 0) c_idx = i;
        if (strcmp(s->name, "bash") == 0) bash_idx = i;
    }
    ASSERT(lua_idx >= 0);
    ASSERT(handles[lua_idx]  != NULL);
    ASSERT(handles[c_idx]    != NULL);
    ASSERT(handles[bash_idx] != NULL);

    spec_registry_teardown_worker(r, handles, got);
    /* After teardown, all slots are NULL. */
    for (int i = 0; i < got; i++) ASSERT(handles[i] == NULL);

    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_translate_pair_declarations() — issue 325 */
/* The pair-declaration lookup is a pure function, so synthetic
 * specs probe the edge cases; the real dlopened specs then prove
 * every shipped pair is declared both ways (the wire walker makes
 * an undeclared pair a load error, so a regression here would
 * break every cross-language map at load). */
static int test_translate_pair_declarations(void)
{
    /* Synthetic: declares only "c". */
    static const char *const targets[] = { "c", NULL };
    lang_spec_t synth = { 0 };
    synth.name = "synthlang";
    synth.translate_targets = targets;

    ASSERT(spec_declares_target(&synth, "c")         == 1);
    ASSERT(spec_declares_target(&synth, "synthlang") == 1); /* self: implicit */
    ASSERT(spec_declares_target(&synth, "bash")      == 0); /* undeclared    */
    synth.translate_targets = NULL;
    ASSERT(spec_declares_target(&synth, "c")         == 0); /* NULL declares nothing */
    ASSERT(spec_declares_target(&synth, "synthlang") == 1); /* self still implicit   */
    ASSERT(spec_declares_target(NULL,   "c")         == 0);
    ASSERT(spec_declares_target(&synth, NULL)        == 0);

    /* Real specs: every shipped pair declared in both directions. */
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);
    const char *names[] = { "lua", "c", "bash" };
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            const lang_spec_t *s = spec_registry_get(r, names[a]);
            ASSERT(s);
            ASSERT(spec_declares_target(s, names[b]) == 1);
        }
    }
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    /* The langs/ directory must be reachable from cwd. */
    struct stat st;
    if (stat("langs", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "011-spec-registry-test: langs/ not found; "
                        "run from project root\n");
        return 2;
    }
    printf("011-spec-registry-test:\n");
    RUN(load_three_specs);
    RUN(missing_dir);
    RUN(null_dir);
    RUN(null_safety);
    RUN(worker_init_teardown);
    RUN(translate_pair_declarations);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
