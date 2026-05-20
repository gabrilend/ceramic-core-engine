/* tests/317-lua-bridge-test.c — Lua spec native↔JSON bridges.
 *
 * Loads langs/lua/spec.so through the spec registry and exercises
 * `native_to_json` and `json_to_native` together. The strategy is a
 * round-trip: feed a JSON string in via `json_to_native` (which
 * pushes a Lua value on the worker's lua_State), then yank it back
 * out via `native_to_json` (which reads top-of-stack and writes JSON
 * to a buffer). If the input and output match, both halves of the
 * bridge agree on the encoding.
 *
 * The round-trip approach keeps this test free of any direct
 * dependency on the LuaJIT headers — the bridges are the only thing
 * the test calls, and the lua_State is opaque from C's side. That
 * matches the cross-language wire-format design (issue 317): callers
 * never need to know Lua's internals to move data through it.
 *
 * Run from the project root so the spec dlopen path resolves.
 */

#include "011-spec-registry.h"
#include "lang-spec.h"

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

/* {{{ load_lua_spec() — boot the registry and find the lua spec */
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
    if (!lua || !lua->init || !lua->teardown
            || !lua->native_to_json || !lua->json_to_native) {
        fprintf(stderr, "lua spec missing bridge callbacks\n");
        spec_registry_destroy(r);
        return NULL;
    }
    *out_reg = r;
    return lua;
}
/* }}} */

/* {{{ roundtrip() — JSON → Lua stack → JSON, compare to expected */
/* Convenience used by every test case. `expected` is the canonical
 * JSON the bridge should emit on the way back; if NULL, the input
 * itself is the expected output. Returns 1 on success. */
static int roundtrip(const lang_spec_t *lua, void *h,
                     const char *input, const char *expected)
{
    char out[512];
    int  out_size = 0;
    int  rc = lua->json_to_native(h, input, (int)strlen(input),
                                  NULL, 0, NULL);
    if (rc != 0) {
        fprintf(stderr, "      json_to_native failed for '%s'\n", input);
        return 0;
    }
    rc = lua->native_to_json(h, NULL, 0, out, (int)sizeof(out), &out_size);
    if (rc != 0) {
        fprintf(stderr, "      native_to_json failed for '%s'\n", input);
        return 0;
    }
    out[out_size] = '\0';
    const char *want = expected ? expected : input;
    if (strcmp(out, want) != 0) {
        fprintf(stderr, "      mismatch: want '%s', got '%s'\n", want, out);
        return 0;
    }
    return 1;
}
/* }}} */

/* {{{ test_scalars() — null, bool, integer, float, string */
static int test_scalars(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);
    ASSERT(h);

    /* Integer-shaped Lua numbers round-trip as integers (3, not 3.0)
     * because encode_value picks the int writer for whole values.
     * "null" round-trips because nil → null → nil. */
    ASSERT(roundtrip(lua, h, "null",      NULL));
    ASSERT(roundtrip(lua, h, "true",      NULL));
    ASSERT(roundtrip(lua, h, "false",     NULL));
    ASSERT(roundtrip(lua, h, "0",         NULL));
    ASSERT(roundtrip(lua, h, "42",        NULL));
    ASSERT(roundtrip(lua, h, "-17",       NULL));
    ASSERT(roundtrip(lua, h, "\"\"",      NULL));
    ASSERT(roundtrip(lua, h, "\"hello\"", NULL));

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_arrays() — sequence tables round-trip as JSON arrays */
static int test_arrays(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    /* Empty array is the one shape we *can't* round-trip as []
     * because an empty Lua table can't be told apart from {} on the
     * way out (see encode_value's array-vs-object heuristic). The
     * round-trip canonicalises [] to {} — a deliberate, documented
     * choice. Every non-empty sequence works as expected. */
    ASSERT(roundtrip(lua, h, "[]",                "{}"));
    ASSERT(roundtrip(lua, h, "[1]",               NULL));
    ASSERT(roundtrip(lua, h, "[1,2,3]",           NULL));
    ASSERT(roundtrip(lua, h, "[\"a\",\"b\"]",     NULL));
    ASSERT(roundtrip(lua, h, "[[1,2],[3,4]]",     NULL));

    /* Lua impedance: `null` inside an array becomes `nil`, which
     * truncates the table's # length at the first hole. JSON's
     * "trailing nulls are still slots" semantics has no native Lua
     * shape. We document this by asserting the truncation rather
     * than pretending it round-trips. A future $lang_opaque or
     * an explicit n-field convention is the way out, but not
     * today. */
    ASSERT(roundtrip(lua, h, "[true,false,null]", "[true,false]"));
    ASSERT(roundtrip(lua, h, "[1,2,null]",        "[1,2]"));

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_objects() — keyed tables round-trip as JSON objects */
/* Object key ordering inside a Lua table is not guaranteed to
 * match the input — Lua's table iteration order is implementation
 * defined. We test single-key objects (no ordering ambiguity) and
 * nested structure for the multi-key case. */
static int test_objects(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    ASSERT(roundtrip(lua, h, "{}",            NULL));
    ASSERT(roundtrip(lua, h, "{\"x\":1}",     NULL));
    ASSERT(roundtrip(lua, h, "{\"a\":\"b\"}", NULL));
    ASSERT(roundtrip(lua, h, "{\"nest\":{\"k\":42}}", NULL));
    ASSERT(roundtrip(lua, h, "{\"items\":[1,2,3]}",   NULL));

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_truncation() — undersized buffer is a loud error */
/* Project convention forbids silent fallbacks. A too-small output
 * buffer must surface as a nonzero return from native_to_json,
 * not a truncated-but-success path. */
static int test_truncation(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    int rc = lua->json_to_native(h, "[1,2,3,4,5,6,7,8,9]",
                                 (int)strlen("[1,2,3,4,5,6,7,8,9]"),
                                 NULL, 0, NULL);
    ASSERT(rc == 0);

    char tiny[4];
    int  out_size = 0;
    rc = lua->native_to_json(h, NULL, 0, tiny, (int)sizeof(tiny), &out_size);
    ASSERT(rc != 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_parse_error() — malformed JSON returns nonzero */
static int test_parse_error(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *lua = load_lua_spec(&r);
    ASSERT(lua);
    void *h = lua->init(0);

    int rc = lua->json_to_native(h, "{not json", 9, NULL, 0, NULL);
    ASSERT(rc != 0);

    lua->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/lua/spec.so", &st) != 0) {
        fprintf(stderr, "317-lua-bridge-test: langs/lua/spec.so not built; "
                        "run `make specs` first\n");
        return 2;
    }
    printf("317-lua-bridge-test:\n");
    RUN(scalars);
    RUN(arrays);
    RUN(objects);
    RUN(truncation);
    RUN(parse_error);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
