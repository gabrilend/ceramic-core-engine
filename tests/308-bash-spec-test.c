/* tests/308-bash-spec-test.c — end-to-end test of the Bash spec.
 *
 * Loads langs/bash/spec.so via the registry and runs each of the
 * three fixture functions in tests/maps/hello/src/echo.sh. The
 * current implementation forks bash per invoke (fork+exec model);
 * the persistent-socket model lands as a follow-on within 308.
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

/* {{{ load_bash_spec() */
static const lang_spec_t *load_bash_spec(spec_registry_t **out_reg)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    if (!r) { free(err); return NULL; }
    const lang_spec_t *b = spec_registry_get(r, "bash");
    if (!b || !b->init || !b->invoke) {
        spec_registry_destroy(r);
        return NULL;
    }
    *out_reg = r;
    return b;
}
/* }}} */

/* {{{ test_init_teardown() */
static int test_init_teardown(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *b = load_bash_spec(&r);
    ASSERT(b);
    void *h = b->init(0);
    ASSERT(h);
    b->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_echo() */
static int test_invoke_echo(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *b = load_bash_spec(&r);
    ASSERT(b);
    void *h = b->init(0);

    const char *msg = "hello, bash";
    const void *args[1]  = { msg };
    int         sizes[1] = { (int)strlen(msg) };
    char buf[64];
    int  n = 0;

    int rc = b->invoke(h, NULL, "tests/maps/hello/src/echo.sh", "echo_arg",
                       args, sizes, NULL, 1, 0, buf, sizeof buf, &n);
    ASSERT(rc == 0);
    ASSERT(n == (int)strlen(msg));
    buf[n] = '\0';
    ASSERT(strcmp(buf, msg) == 0);

    b->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_concat() */
static int test_invoke_concat(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *b = load_bash_spec(&r);
    void *h = b->init(0);

    const char *a = "left-", *c = "right";
    const void *args[2]  = { a, c };
    int         sizes[2] = { (int)strlen(a), (int)strlen(c) };
    char buf[64];
    int  n = 0;
    int rc = b->invoke(h, NULL, "tests/maps/hello/src/echo.sh", "concat",
                       args, sizes, NULL, 2, 0, buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "left-right") == 0);

    b->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_shout() */
static int test_invoke_shout(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *b = load_bash_spec(&r);
    void *h = b->init(0);

    const char *m = "hey";
    const void *args[1]  = { m };
    int         sizes[1] = { 3 };
    char buf[16];
    int  n = 0;
    int rc = b->invoke(h, NULL, "tests/maps/hello/src/echo.sh", "shout",
                       args, sizes, NULL, 1, 0, buf, sizeof buf, &n);
    ASSERT(rc == 0);
    buf[n] = '\0';
    ASSERT(strcmp(buf, "hey!") == 0);

    b->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_invoke_missing_function() */
static int test_invoke_missing_function(void)
{
    spec_registry_t *r = NULL;
    const lang_spec_t *b = load_bash_spec(&r);
    void *h = b->init(0);

    char buf[16];
    int  n = 0;
    int rc = b->invoke(h, NULL, "tests/maps/hello/src/echo.sh", "no_such_fn",
                       NULL, NULL, NULL, 0, 0, buf, sizeof buf, &n);
    ASSERT(rc != 0);

    b->teardown(h);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/bash/spec.so", &st) != 0 ||
        stat("tests/maps/hello/src/echo.sh", &st) != 0) {
        fprintf(stderr, "308-bash-spec-test: spec or fixture not built; "
                        "run from project root with `make` first\n");
        return 2;
    }
    printf("308-bash-spec-test:\n");
    RUN(init_teardown);
    RUN(invoke_echo);
    RUN(invoke_concat);
    RUN(invoke_shout);
    RUN(invoke_missing_function);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
