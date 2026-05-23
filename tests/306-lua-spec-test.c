/* tests/306-lua-spec-test.c — end-to-end test of the Lua spec.
 *
 * Loads langs/lua/spec.so through the registry, runs init / invoke /
 * teardown against tests/maps/hello/src/hello.lua. Exercises the
 * file-load + module-table + named-function invocation path with
 * both positional and optional arguments.
 *
 * Run from the project root so `langs/` and `tests/maps/` are
 * reachable.
 */

#include "011-spec-registry.h"
#include "lang-spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

/* {{{ load_lua_spec() — shared by every test */
static const lang_spec_t *load_lua_spec(spec_registry_t **out_reg)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    if (!r) {
        fprintf(stderr, "registry load failed: %s\n", err ? err : "(null)");
        free(err);
        return NULL;
    }
    const lang_spec_t *lua = spec_registry_get(r, "lua");
    if (!lua || !lua->init || !lua->invoke || !lua->teardown) {
        fprintf(stderr, "lua spec missing callbacks\n");
        spec_registry_destroy(r);
        return NULL;
    }
    *out_reg = r;
    return lua;
}
/* }}} */

/* {{{ test_init_teardown() */
static int test_init_teardown(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);
    ASSERT(h);
    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_greet_both_args() */
static int test_invoke_greet_both_args(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);
    ASSERT(h);

    const char *name = "World";
    const char *sal  = "Hi";
    const void *args[2]  = { name, sal };
    int         sizes[2] = { (int)strlen(name), (int)strlen(sal) };
    char buf[128];
    int n = 0;

    /* output_native=1 — same-language fast path, tostring coercion.
     * (Slice 4.5: output_native=0 would now write JSON.) */
    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "greet",
                         args, sizes, NULL, 2, 1,
                         buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == (int)strlen("Hi, World!"));
    buf[n] = '\0';
    ASSERT(strcmp(buf, "Hi, World!") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_greet_default_salutation() */
/* greet(name) — salutation defaults to "Hello" inside the function. */
static int test_invoke_greet_default_salutation(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    const char *name = "moon";
    const void *args[1]  = { name };
    int         sizes[1] = { 4 };
    char buf[64];
    int n = 0;

    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "greet",
                         args, sizes, NULL, 1, 1, buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "Hello, moon!") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_missing_function() */
static int test_invoke_missing_function(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    char buf[64];
    int  n = 0;
    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "no_such_fn",
                         NULL, NULL, NULL, 0, 0, buf, sizeof buf, &n);
    ASSERT(rc != 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_missing_file() */
static int test_invoke_missing_file(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    char buf[64];
    int  n = 0;
    int rc = lua->invoke(h, NULL, "tests/maps/nope/missing.lua", "greet",
                         NULL, NULL, NULL, 0, 0, buf, sizeof buf, &n);
    ASSERT(rc != 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_json_input() — slice 4 of issue 312 */
/* When input_native[i] = 0 (JSON), the Lua spec parses the bytes
 * and pushes the resulting value onto the stack. M.sum_pair receives
 * the parsed table and indexes into it; if the spec had passed raw
 * bytes instead the indexing would error. */
static int test_invoke_json_input(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    const char *json    = "{\"a\":17,\"b\":25}";
    const void *args[1] = { json };
    int         sizes[1] = { (int)strlen(json) };
    int         native[1] = { 0 };   /* JSON */
    char buf[32];
    int  n = 0;

    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "sum_pair",
                         args, sizes, native, 1, 1,
                         buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 2);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "42") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_native_input_with_flag() — slice 4 of issue 312 */
/* When input_native[i] = 1 (NATIVE), the Lua spec keeps the
 * existing raw-bytes-as-string path. greet still receives "World"
 * as a string and prepends "Hello, ". */
static int test_invoke_native_input_with_flag(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    const char *name    = "World";
    const void *args[1] = { name };
    int         sizes[1] = { 5 };
    int         native[1] = { 1 };   /* NATIVE */
    char buf[64];
    int  n = 0;

    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "greet",
                         args, sizes, native, 1, 1,
                         buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "Hello, World!") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_json_input_falls_back_to_raw() — slice 4 of issue 312 */
/* When input_native[i] = 0 but the bytes don't parse as JSON, the
 * Lua spec falls back to pushing the raw bytes as a string. This
 * preserves backward compatibility with cross-language producers
 * that don't yet emit real JSON (C and Bash currently write raw
 * bytes). greet receives "World" and prepends "Hello, ". */
static int test_invoke_json_input_falls_back_to_raw(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    /* "World" alone isn't valid JSON (unquoted string). The spec
     * should fall back to lua_pushlstring. */
    const char *name    = "World";
    const void *args[1] = { name };
    int         sizes[1] = { 5 };
    int         native[1] = { 0 };   /* JSON-but-not-actually */
    char buf[64];
    int  n = 0;

    int rc = lua->invoke(h, NULL, "tests/maps/hello/src/hello.lua", "greet",
                         args, sizes, native, 1, 1,
                         buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "Hello, World!") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_via_merged_module() — issue 313 */
/* Writes a temporary directory with a __merged__.lua that exports
 * one sub-module under the basename "calc". Calls invoke with a
 * file_path pointing at a phantom calc.lua (the file doesn't
 * exist on disk). The spec should detect __merged__.lua,
 * route through it, look up "calc" in the merged table, and
 * call M.add on the per-file module. Proves the merge path
 * is wired end-to-end. */
static int test_invoke_via_merged_module(void)
{
    /* Set up a temp directory with the merged file. */
    char tmpl[] = "/tmp/soramech-merged-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char merged_path[256];
    snprintf(merged_path, sizeof merged_path, "%s/__merged__.lua", dir);
    FILE *f = fopen(merged_path, "w");
    ASSERT(f);
    fputs("local merged = {}\n", f);
    fputs("merged[\"calc\"] = (function()\n", f);
    fputs("  local M = {}\n", f);
    fputs("  function M.add(a, b) return tostring(tonumber(a) + tonumber(b)) end\n", f);
    fputs("  return M\n", f);
    fputs("end)()\n", f);
    fputs("return merged\n", f);
    fclose(f);

    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    /* phantom_path doesn't exist on disk — the spec must reach
     * the merged module to find the function. */
    char phantom_path[256];
    snprintf(phantom_path, sizeof phantom_path, "%s/calc.lua", dir);

    const char *a = "17", *b = "25";
    const void *args[2]    = { a, b };
    int         sizes[2]   = { 2, 2 };
    int         native[2]  = { 1, 1 };
    char buf[16];
    int  n = 0;

    int rc = lua->invoke(h, NULL, phantom_path, "add",
                         args, sizes, native, 2, 1,
                         buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "42") == 0);

    lua->teardown(h);
    spec_registry_destroy(r);

    /* Clean up the temp dir. */
    unlink(merged_path);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/lua/spec.so", &st) != 0) {
        fprintf(stderr, "306-lua-spec-test: langs/lua/spec.so not built; "
                        "run `make specs` first\n");
        return 2;
    }
    if (stat("tests/maps/hello/src/hello.lua", &st) != 0) {
        fprintf(stderr, "306-lua-spec-test: tests/maps/hello/src/hello.lua "
                        "not found; run from project root\n");
        return 2;
    }
    printf("306-lua-spec-test:\n");
    RUN(init_teardown);
    RUN(invoke_greet_both_args);
    RUN(invoke_greet_default_salutation);
    RUN(invoke_missing_function);
    RUN(invoke_missing_file);
    RUN(invoke_json_input);
    RUN(invoke_native_input_with_flag);
    RUN(invoke_json_input_falls_back_to_raw);
    RUN(invoke_via_merged_module);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
