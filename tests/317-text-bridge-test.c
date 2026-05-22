/* tests/317-text-bridge-test.c — bridges for the text-oriented specs
 * (C and Bash). Both treat the wire side as bytes-in / bytes-out and
 * have no per-worker state on the native side, so the bridge logic
 * is the same shape: native_to_json passes valid JSON through and
 * JSON-string-wraps everything else; json_to_native unquotes JSON
 * strings and passes everything else through. Same suite, two
 * specs — the registry hands us a different vtable per call.
 *
 * Run from the project root so the spec dlopen paths resolve.
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

#define RUN(name, spec, label) \
    do { \
        char title[80]; snprintf(title, sizeof title, "%s · %s", label, #name); \
        fprintf(stdout, "  %-44s ", title); fflush(stdout); \
        if (test_##name(spec)) { fprintf(stdout, "ok\n"); g_pass++; } \
        else                   { fprintf(stdout, "FAIL\n"); g_fail++; } \
    } while (0)
/* }}} */

/* {{{ Suite — every check works against any spec exposing the
 * native↔JSON bridges with no worker-state requirement. */

/* A JSON-valid input passes through native_to_json unchanged. */
static int test_native_passes_through_json(const lang_spec_t *s)
{
    char dst[64];
    int n = 0;
    const char *src = "{\"a\":1}";
    ASSERT(s->native_to_json(NULL, src, (int)strlen(src),
                             dst, sizeof dst, &n) == 0);
    ASSERT(n == (int)strlen(src));
    ASSERT(memcmp(dst, src, (size_t)n) == 0);
    return 1;
}

/* Bare text wraps as a JSON string with escaping. */
static int test_native_wraps_bare_text(const lang_spec_t *s)
{
    char dst[64];
    int n = 0;
    const char *src = "hello world";
    ASSERT(s->native_to_json(NULL, src, (int)strlen(src),
                             dst, sizeof dst, &n) == 0);
    ASSERT(n == (int)strlen("\"hello world\""));
    ASSERT(memcmp(dst, "\"hello world\"", (size_t)n) == 0);
    return 1;
}

/* Bare text with a quote gets the quote escaped on the way out. */
static int test_native_escapes_quotes(const lang_spec_t *s)
{
    char dst[64];
    int n = 0;
    const char *src = "say \"hi\"";
    ASSERT(s->native_to_json(NULL, src, (int)strlen(src),
                             dst, sizeof dst, &n) == 0);
    ASSERT(n == (int)strlen("\"say \\\"hi\\\"\""));
    ASSERT(memcmp(dst, "\"say \\\"hi\\\"\"", (size_t)n) == 0);
    return 1;
}

/* JSON string round-trips to its unquoted bytes through json_to_native. */
static int test_native_to_native_unquotes_string(const lang_spec_t *s)
{
    char dst[64];
    int n = 0;
    const char *src = "\"hello\"";
    ASSERT(s->json_to_native(NULL, src, (int)strlen(src),
                             dst, sizeof dst, &n) == 0);
    ASSERT(n == 5);
    ASSERT(memcmp(dst, "hello", 5) == 0);
    return 1;
}

/* JSON numbers / bools / null pass through json_to_native unchanged. */
static int test_native_to_native_passes_primitives(const lang_spec_t *s)
{
    char dst[16];
    int n = 0;
    ASSERT(s->json_to_native(NULL, "42", 2, dst, sizeof dst, &n) == 0);
    ASSERT(n == 2 && memcmp(dst, "42", 2) == 0);

    ASSERT(s->json_to_native(NULL, "true", 4, dst, sizeof dst, &n) == 0);
    ASSERT(n == 4 && memcmp(dst, "true", 4) == 0);

    ASSERT(s->json_to_native(NULL, "null", 4, dst, sizeof dst, &n) == 0);
    ASSERT(n == 4 && memcmp(dst, "null", 4) == 0);
    return 1;
}

/* Buffer-too-small surfaces as a -1 from native_to_json. */
static int test_native_to_json_overflow_fails(const lang_spec_t *s)
{
    char tiny[4];
    int n = 0;
    const char *src = "this string will not fit in four bytes";
    ASSERT(s->native_to_json(NULL, src, (int)strlen(src),
                             tiny, sizeof tiny, &n) == -1);
    return 1;
}
/* }}} */

/* {{{ load_spec_or_skip() */
static const lang_spec_t *load_spec_or_skip(spec_registry_t **out_reg,
                                            const char *name)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    if (!r) {
        fprintf(stderr, "registry load failed: %s\n", err ? err : "(null)");
        free(err);
        *out_reg = NULL;
        return NULL;
    }
    const lang_spec_t *s = spec_registry_get(r, name);
    if (!s || !s->native_to_json || !s->json_to_native) {
        fprintf(stderr, "%s spec missing bridge callbacks\n", name);
        spec_registry_destroy(r);
        *out_reg = NULL;
        return NULL;
    }
    *out_reg = r;
    return s;
}
/* }}} */

/* {{{ run_suite() */
static void run_suite(const lang_spec_t *s, const char *label)
{
    RUN(native_passes_through_json,        s, label);
    RUN(native_wraps_bare_text,            s, label);
    RUN(native_escapes_quotes,             s, label);
    RUN(native_to_native_unquotes_string,  s, label);
    RUN(native_to_native_passes_primitives,s, label);
    RUN(native_to_json_overflow_fails,     s, label);
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/c/spec.so",    &st) != 0 ||
        stat("langs/bash/spec.so", &st) != 0) {
        fprintf(stderr, "317-text-bridge-test: spec(s) not built; "
                        "run from project root with `make` first\n");
        return 2;
    }
    printf("317-text-bridge-test:\n");

    spec_registry_t *r_c = NULL;
    const lang_spec_t *c = load_spec_or_skip(&r_c, "c");
    if (c) run_suite(c, "c");

    spec_registry_t *r_b = NULL;
    const lang_spec_t *b = load_spec_or_skip(&r_b, "bash");
    if (b) run_suite(b, "bash");

    if (r_c) spec_registry_destroy(r_c);
    if (r_b) spec_registry_destroy(r_b);

    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
