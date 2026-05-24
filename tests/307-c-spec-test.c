/* tests/307-c-spec-test.c — end-to-end test of the C spec.
 *
 * Loads langs/c/spec.so via the registry, calls its compile
 * callback to turn tests/maps/hello/src/echo.c into a .so in
 * /tmp, then invokes both functions in it. Verifies the dlopen
 * cache returns the same handle on repeat invocation.
 */

#include "011-spec-registry.h"
#include "lang-spec.h"
/* The compile callback now takes a `const box_t *` (issue 307);
 * full definition lives in the graph loader's header. */
#include "010-graph-loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

/* {{{ load_c_spec() */
static const lang_spec_t *load_c_spec(spec_registry_t **out_reg)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    if (!r) { free(err); return NULL; }
    const lang_spec_t *c = spec_registry_get(r, "c");
    if (!c || !c->compile || !c->invoke) {
        spec_registry_destroy(r);
        return NULL;
    }
    *out_reg = r;
    return c;
}
/* }}} */

/* {{{ compile_echo() — shared helper: produce /tmp/echo-XXXX.so */
static int compile_echo(const lang_spec_t *c, char *out_so_path, size_t cap)
{
    /* Pick a unique tmp filename. */
    snprintf(out_so_path, cap, "/tmp/soramech-echo-%d.so", (int)getpid());
    int rc = c->compile("tests/maps/hello/src/echo.c", out_so_path, NULL);
    return rc;
}
/* }}} */

/* {{{ test_compile_succeeds() */
static int test_compile_succeeds(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    char so_path[256];
    int rc = compile_echo(c, so_path, sizeof so_path);
    ASSERT(rc == 0);

    struct stat st;
    ASSERT(stat(so_path, &st) == 0);
    ASSERT(st.st_size > 0);

    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_echo() */
static int test_invoke_echo(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);

    void *h = c->init(0);
    ASSERT(h);

    const char *msg = "hello, c!";
    const void *args[1]  = { msg };
    int         sizes[1] = { (int)strlen(msg) };
    char buf[64];
    int n = 0;
    int rc = c->invoke(h, NULL, so_path, "echo", args, sizes, NULL, 1, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == (int)strlen(msg));
    buf[n] = '\0';
    ASSERT(strcmp(buf, msg) == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_concat() */
static int test_invoke_concat(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    const char *a = "abc", *b = "DEF";
    const void *args[2]  = { a, b };
    int         sizes[2] = { 3, 3 };
    char buf[64];
    int n = 0;
    int rc = c->invoke(h, NULL, so_path, "concat", args, sizes, NULL, 2, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 6);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "abcDEF") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_missing_symbol() */
static int test_invoke_missing_symbol(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    char buf[16];
    int  n = 0;
    int rc = c->invoke(h, NULL, so_path, "no_such_fn",
                       NULL, NULL, NULL, 0, 0, buf, sizeof buf, &n);
    ASSERT(rc != 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_compile_bad_source_fails() */
static int test_compile_bad_source_fails(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    /* Write a syntactically broken C file. */
    char src[256], dst[256];
    snprintf(src, sizeof src, "/tmp/soramech-bad-%d.c", (int)getpid());
    snprintf(dst, sizeof dst, "/tmp/soramech-bad-%d.so", (int)getpid());
    FILE *fp = fopen(src, "w");
    ASSERT(fp);
    fputs("not even close to valid C\n", fp);
    fclose(fp);

    int rc = c->compile(src, dst, NULL);
    ASSERT(rc != 0);

    unlink(src);
    unlink(dst); /* may not exist, ignore */
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_json_string_input() — issue 307 JSON-input acceptance */
/* When input_native[i] = 0 and the bytes are a JSON string, the C
 * spec strips the quotes so the user's function sees the inner
 * text. echo(s) returns whatever it received; if the strip works
 * the function gets "hello" without quotes, not "\"hello\"". */
static int test_invoke_json_string_input(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    /* A JSON string `"hello"` — 7 bytes including the quotes. */
    const char *json_str = "\"hello\"";
    const void *args[1]    = { json_str };
    int         sizes[1]   = { (int)strlen(json_str) };
    int         native[1]  = { 0 };   /* JSON */
    char buf[64];
    int n = 0;
    int rc = c->invoke(h, NULL, so_path, "echo",
                       args, sizes, native, 1, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 5);                   /* "hello" minus the quotes */
    buf[n] = '\0';
    ASSERT(strcmp(buf, "hello") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_json_number_input() — passthrough on primitive */
/* JSON numbers stay as raw bytes — "42" is already the form C's
 * atoi / strtol expect. The C function receives the bytes unchanged. */
static int test_invoke_json_number_input(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    const char *json_num   = "42";
    const void *args[1]    = { json_num };
    int         sizes[1]   = { 2 };
    int         native[1]  = { 0 };   /* JSON */
    char buf[16];
    int n = 0;
    int rc = c->invoke(h, NULL, so_path, "echo",
                       args, sizes, native, 1, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 2);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "42") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_json_falls_back_on_non_json() — robustness */
/* When the bytes don't parse as JSON, the C spec passes them
 * through unchanged. Lets cross-language producers that don't yet
 * emit real JSON (other languages, future specs) keep working. */
static int test_invoke_json_falls_back_on_non_json(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    /* Raw "hello" with no quotes — not valid JSON; should pass
     * through unchanged. */
    const char *raw       = "hello";
    const void *args[1]   = { raw };
    int         sizes[1]  = { 5 };
    int         native[1] = { 0 };   /* JSON-flag but not actually JSON */
    char buf[16];
    int n = 0;
    int rc = c->invoke(h, NULL, so_path, "echo",
                       args, sizes, native, 1, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 5);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "hello") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_compile_with_cflags() — issue 307 items 3+4
 *
 * Writes a tiny C source whose return value depends on a -D macro,
 * compiles it twice: once with no box hints (default flags), once
 * with cflags="-DSORAMECH_FLAG_ON" via the box record. The two
 * builds produce different return values; the cflags string flows
 * through to gcc as intended. */
static int test_compile_with_cflags(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    char src_path[256], so_off[256], so_on[256];
    /* Each path is distinct from any path the rest of the suite or
     * a prior run could have left in glibc's per-process dlopen
     * cache: per-pid base + per-build suffix. Production code uses
     * box-id-derived paths which are also distinct per box, so the
     * cache hazard doesn't bite at runtime — this naming just
     * mirrors the production discipline inside a single test. */
    long ts = (long)time(NULL);
    snprintf(src_path, sizeof src_path,
             "/tmp/soramech-cflag-probe-%d-%ld.c",   (int)getpid(), ts);
    snprintf(so_off, sizeof so_off,
             "/tmp/soramech-cflag-probe-%d-%ld-default.so", (int)getpid(), ts);
    snprintf(so_on,  sizeof so_on,
             "/tmp/soramech-cflag-probe-%d-%ld-flagged.so", (int)getpid(), ts);

    FILE *fp = fopen(src_path, "w");
    ASSERT(fp);
    fputs(
        "int probe(const void **inputs, const int *sizes, int n,\n"
        "          void *out_buf, int out_capacity, int *out_size) {\n"
        "    (void)inputs; (void)sizes; (void)n; (void)out_capacity;\n"
        "    const char *v =\n"
        "#ifdef SORAMECH_FLAG_ON\n"
        "        \"on\";\n"
        "#else\n"
        "        \"off\";\n"
        "#endif\n"
        "    int len = 0; while (v[len]) len++;\n"
        "    for (int i = 0; i < len; i++) ((char *)out_buf)[i] = v[i];\n"
        "    *out_size = len; return 0;\n"
        "}\n",
        fp);
    fclose(fp);

    /* Default build (no cflags): probe returns "off". */
    ASSERT(c->compile(src_path, so_off, NULL) == 0);
    void *h = c->init(0);
    ASSERT(h);
    char buf[8]; int n = 0;
    int rc = c->invoke(h, NULL, so_off, "probe", NULL, NULL, NULL, 0, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "off") == 0);
    c->teardown(h);
    unlink(so_off);

    /* Build with cflags="-DSORAMECH_FLAG_ON": probe returns "on". */
    box_t hint;
    memset(&hint, 0, sizeof hint);
    hint.id     = "probe";
    hint.cflags = "-DSORAMECH_FLAG_ON";

    ASSERT(c->compile(src_path, so_on, &hint) == 0);
    h = c->init(0);
    ASSERT(h);
    n = 0;
    rc = c->invoke(h, NULL, so_on, "probe", NULL, NULL, NULL, 0, 0,
                   buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "on") == 0);
    c->teardown(h);

    unlink(so_on);
    unlink(src_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_compile_with_link_libs() — issue 307 items 3+4
 *
 * A C source that calls sqrt() (from libm) is built with
 * link_libs=["m"]. The build links against libm at compile time,
 * dlopen resolves the symbol at run time, and invoking the box's
 * function returns "4" (sqrt(16)). Without the link_libs hint the
 * loader can sometimes still resolve sqrt through ambient
 * dependencies — this test focuses on "the explicit hint works",
 * not on the negative case. */
static int test_compile_with_link_libs(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    char src_path[256], so_path[256];
    snprintf(src_path, sizeof src_path,
             "/tmp/soramech-libm-%d.c", (int)getpid());
    snprintf(so_path,  sizeof so_path,
             "/tmp/soramech-libm-%d.so", (int)getpid());

    FILE *fp = fopen(src_path, "w");
    ASSERT(fp);
    fputs(
        "#include <math.h>\n"
        "#include <stdio.h>\n"
        "int rootify(const void **inputs, const int *sizes, int n,\n"
        "            void *out_buf, int out_capacity, int *out_size) {\n"
        "    (void)inputs; (void)sizes; (void)n;\n"
        "    double v = sqrt(16.0);\n"
        "    int len = snprintf((char *)out_buf, (size_t)out_capacity,\n"
        "                       \"%g\", v);\n"
        "    *out_size = len; return 0;\n"
        "}\n",
        fp);
    fclose(fp);

    box_t hint;
    memset(&hint, 0, sizeof hint);
    hint.id          = "rootify";
    const char *libs[1] = { "m" };
    hint.link_libs   = libs;
    hint.n_link_libs = 1;

    ASSERT(c->compile(src_path, so_path, &hint) == 0);
    void *h = c->init(0);
    ASSERT(h);
    char buf[16]; int n = 0;
    int rc = c->invoke(h, NULL, so_path, "rootify", NULL, NULL, NULL, 0, 0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "4") == 0);
    c->teardown(h);

    unlink(so_path);
    unlink(src_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_string_output_jsonifies() — issue 307 item 2
 *
 * When the box declares `returns: "string"` and the dispatch asks
 * for JSON output (output_native=0), the spec wraps the user
 * function's raw bytes as a JSON string — quotes added, special
 * characters escaped. */
static int test_invoke_string_output_jsonifies(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    box_t hint;
    memset(&hint, 0, sizeof hint);
    hint.id      = "echo";
    hint.returns = "string";

    const char *msg = "hi";
    const void *args[1]  = { msg };
    int         sizes[1] = { 2 };
    char buf[64];
    int n = 0;
    int rc = c->invoke(h, &hint, so_path, "echo",
                       args, sizes, NULL, 1, /*output_native=*/0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 4);                /* "\"hi\"" — four bytes */
    buf[n] = '\0';
    ASSERT(strcmp(buf, "\"hi\"") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_string_output_native_passthrough() — issue 307 item 2
 *
 * When the dispatch asks for native (output_native=1), the spec
 * leaves the user's bytes alone regardless of the declared return
 * type. Same-language fast path keeps strings raw. */
static int test_invoke_string_output_native_passthrough(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);
    char so_path[256];
    ASSERT(compile_echo(c, so_path, sizeof so_path) == 0);
    void *h = c->init(0);

    box_t hint;
    memset(&hint, 0, sizeof hint);
    hint.id      = "echo";
    hint.returns = "string";

    const char *msg = "hi";
    const void *args[1]  = { msg };
    int         sizes[1] = { 2 };
    char buf[64];
    int n = 0;
    int rc = c->invoke(h, &hint, so_path, "echo",
                       args, sizes, NULL, 1, /*output_native=*/1,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 2);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "hi") == 0);

    c->teardown(h);
    unlink(so_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_int_output_passthrough() — issue 307 item 2
 *
 * Declared `returns: "int"`. The function writes "42" — already a
 * valid JSON number — and the JSON output mode passes those bytes
 * through unchanged. */
static int test_invoke_int_output_passthrough(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *c = load_c_spec(&r);
    ASSERT(c);

    char src_path[256], so_path[256];
    snprintf(src_path, sizeof src_path,
             "/tmp/soramech-int-%d.c", (int)getpid());
    snprintf(so_path,  sizeof so_path,
             "/tmp/soramech-int-%d.so", (int)getpid());
    FILE *fp = fopen(src_path, "w");
    ASSERT(fp);
    fputs(
        "#include <stdio.h>\n"
        "int forty_two(const void **inputs, const int *sizes, int n,\n"
        "              void *out_buf, int out_capacity, int *out_size) {\n"
        "    (void)inputs; (void)sizes; (void)n;\n"
        "    int written = snprintf((char *)out_buf, (size_t)out_capacity, \"%d\", 42);\n"
        "    *out_size = written; return 0;\n"
        "}\n",
        fp);
    fclose(fp);

    ASSERT(c->compile(src_path, so_path, NULL) == 0);
    void *h = c->init(0);

    box_t hint;
    memset(&hint, 0, sizeof hint);
    hint.id      = "forty_two";
    hint.returns = "int";

    char buf[16];
    int  n = 0;
    int rc = c->invoke(h, &hint, so_path, "forty_two",
                       NULL, NULL, NULL, 0, /*output_native=*/0,
                       buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == 2);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "42") == 0);

    c->teardown(h);
    unlink(so_path);
    unlink(src_path);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/c/spec.so", &st) != 0 ||
        stat("tests/maps/hello/src/echo.c", &st) != 0) {
        fprintf(stderr, "307-c-spec-test: spec or fixture not built; "
                        "run from project root with `make` first\n");
        return 2;
    }
    printf("307-c-spec-test:\n");
    RUN(compile_succeeds);
    RUN(invoke_echo);
    RUN(invoke_concat);
    RUN(invoke_missing_symbol);
    RUN(compile_bad_source_fails);
    RUN(invoke_json_string_input);
    RUN(invoke_json_number_input);
    RUN(invoke_json_falls_back_on_non_json);
    RUN(compile_with_cflags);
    RUN(compile_with_link_libs);
    RUN(invoke_string_output_jsonifies);
    RUN(invoke_string_output_native_passthrough);
    RUN(invoke_int_output_passthrough);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
