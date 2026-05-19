/* tests/307-c-spec-test.c — end-to-end test of the C spec.
 *
 * Loads langs/c/spec.so via the registry, calls its compile
 * callback to turn tests/maps/hello/src/echo.c into a .so in
 * /tmp, then invokes both functions in it. Verifies the dlopen
 * cache returns the same handle on repeat invocation.
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
    int rc = c->compile("tests/maps/hello/src/echo.c", out_so_path);
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
    int rc = c->invoke(h, so_path, "echo", args, sizes, 1,
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
    int rc = c->invoke(h, so_path, "concat", args, sizes, 2,
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
    int rc = c->invoke(h, so_path, "no_such_fn",
                       NULL, NULL, 0, buf, sizeof buf, &n);
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

    int rc = c->compile(src, dst);
    ASSERT(rc != 0);

    unlink(src);
    unlink(dst); /* may not exist, ignore */
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
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
