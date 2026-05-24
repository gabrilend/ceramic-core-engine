/* tests/020-sentinels-test.c — unit tests for src/020-sentinels.
 *
 * Covers the spec-independent sentinel machinery:
 *  - kind detection on parsed JSON
 *  - emit writers produce parseable JSON whose round-trip detects
 *    as the expected kind
 *  - $ref store: alloc / lookup / clear
 *  - capability validation: gap detection across emit / reconstruct
 *    masks
 */
#include "020-sentinels.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Tiny test harness */
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        return 0; \
    } \
} while (0)

static int tests_run = 0, tests_passed = 0;
#define RUN(name) do { \
    tests_run++; \
    printf("  %-44s ", #name); \
    fflush(stdout); \
    if (test_##name()) { \
        printf("ok\n"); \
        tests_passed++; \
    } \
} while (0)
/* }}} */

/* {{{ detect_round_trip — emit then re-detect */
static int test_detect_round_trip_ref(void)
{
    char buf[256];
    json_writer_t w;
    json_writer_init(&w, buf, sizeof buf);
    sentinel_write_ref(&w, 0xCAFEBABE, 1024);
    int n = json_writer_finish(&w);
    ASSERT(n > 0);

    json_arena_t *a = json_arena_create();
    int eo; const char *em;
    json_node_t *root = json_parse(a, buf, &eo, &em);
    ASSERT(root != NULL);
    ASSERT(sentinel_detect(root) == SENTINEL_REF);
    json_arena_destroy(a);
    return 1;
}

static int test_detect_round_trip_lang_opaque(void)
{
    char buf[256];
    json_writer_t w;
    json_writer_init(&w, buf, sizeof buf);
    sentinel_write_lang_opaque(&w, "lua", 42, "function");
    int n = json_writer_finish(&w);
    ASSERT(n > 0);

    json_arena_t *a = json_arena_create();
    int eo; const char *em;
    json_node_t *root = json_parse(a, buf, &eo, &em);
    ASSERT(root != NULL);
    ASSERT(sentinel_detect(root) == SENTINEL_LANG_OPAQUE);
    json_arena_destroy(a);
    return 1;
}

static int test_detect_round_trip_function_pointer_stub(void)
{
    char buf[256];
    json_writer_t w;
    json_writer_init(&w, buf, sizeof buf);
    sentinel_write_function_pointer_stub(&w, "int(int,int)");
    int n = json_writer_finish(&w);
    ASSERT(n > 0);

    json_arena_t *a = json_arena_create();
    int eo; const char *em;
    json_node_t *root = json_parse(a, buf, &eo, &em);
    ASSERT(root != NULL);
    ASSERT(sentinel_detect(root) == SENTINEL_FUNCTION_POINTER);
    json_arena_destroy(a);
    return 1;
}
/* }}} */

/* {{{ Non-sentinel objects don't false-positive */
static int test_detect_skips_plain_objects(void)
{
    json_arena_t *a = json_arena_create();
    int eo; const char *em;

    /* Plain object — multiple keys. */
    json_node_t *n1 = json_parse(a, "{\"a\":1,\"b\":2}", &eo, &em);
    ASSERT(sentinel_detect(n1) == SENTINEL_NONE);

    /* Single-key object whose key isn't a sentinel name. */
    json_node_t *n2 = json_parse(a, "{\"foo\":1}", &eo, &em);
    ASSERT(sentinel_detect(n2) == SENTINEL_NONE);

    /* Arrays / scalars / null. */
    json_node_t *n3 = json_parse(a, "[1,2,3]", &eo, &em);
    ASSERT(sentinel_detect(n3) == SENTINEL_NONE);
    json_node_t *n4 = json_parse(a, "42", &eo, &em);
    ASSERT(sentinel_detect(n4) == SENTINEL_NONE);
    json_node_t *n5 = json_parse(a, "null", &eo, &em);
    ASSERT(sentinel_detect(n5) == SENTINEL_NONE);

    json_arena_destroy(a);
    return 1;
}
/* }}} */

/* {{{ $ref store round-trip */
static int test_ref_store(void)
{
    sentinel_ref_store_clear();
    const char *payload = "0123456789ABCDEF";
    int len = (int)strlen(payload);
    uintptr_t p = sentinel_ref_alloc(payload, len);
    ASSERT(p != 0);
    int got_len = 0;
    const void *got = sentinel_ref_lookup(p, &got_len);
    ASSERT(got != NULL);
    ASSERT(got_len == len);
    ASSERT(memcmp(got, payload, (size_t)len) == 0);

    /* Lookup with a wrong pointer returns NULL. */
    int dummy = 0;
    ASSERT(sentinel_ref_lookup(p + 1, &dummy) == NULL);

    sentinel_ref_store_clear();
    /* After clear, the pointer is no longer valid (and may be freed
     * memory — we only assert lookup miss, not dereferencing). */
    ASSERT(sentinel_ref_lookup(p, &dummy) == NULL);
    return 1;
}
/* }}} */

/* {{{ $ref refcount: dec to zero frees the chunk */
static int test_ref_refcount_dec(void)
{
    sentinel_ref_store_clear();
    uintptr_t p = sentinel_ref_alloc("hello", 5);
    ASSERT(p != 0);
    int len = 0;
    ASSERT(sentinel_ref_lookup(p, &len) != NULL);
    ASSERT(len == 5);

    /* Initial refcount is 1. A single dec brings it to 0 and the
     * chunk is freed; lookup misses. */
    sentinel_ref_dec(p);
    ASSERT(sentinel_ref_lookup(p, &len) == NULL);
    return 1;
}
/* }}} */

/* {{{ $ref refcount: inc balances dec */
static int test_ref_refcount_inc_dec(void)
{
    sentinel_ref_store_clear();
    uintptr_t p = sentinel_ref_alloc("balanced", 8);
    ASSERT(p != 0);

    /* refcount 1 → 3 via two incs. Two decs leaves it at 1; chunk
     * still present. Third dec brings it to 0; chunk freed. */
    sentinel_ref_inc(p);
    sentinel_ref_inc(p);
    sentinel_ref_dec(p);
    sentinel_ref_dec(p);
    int len = 0;
    ASSERT(sentinel_ref_lookup(p, &len) != NULL);
    ASSERT(len == 8);

    sentinel_ref_dec(p);
    ASSERT(sentinel_ref_lookup(p, &len) == NULL);
    return 1;
}
/* }}} */

/* {{{ $ref slot reuse — a freed slot is reused by the next alloc */
static int test_ref_slot_reuse(void)
{
    sentinel_ref_store_clear();
    uintptr_t p1 = sentinel_ref_alloc("aaa", 3);
    uintptr_t p2 = sentinel_ref_alloc("bbb", 3);
    ASSERT(p1 != 0 && p2 != 0 && p1 != p2);

    /* Free p1. Allocate p3 — it should land in the same table
     * slot p1 occupied (not extend the table further). We can't
     * assert the slot index directly through the public API, but
     * we can assert that after freeing one and allocating one,
     * lookups behave: p1 misses, p2 hits, p3 hits. */
    sentinel_ref_dec(p1);
    int len = 0;
    ASSERT(sentinel_ref_lookup(p1, &len) == NULL);
    ASSERT(sentinel_ref_lookup(p2, &len) != NULL);

    uintptr_t p3 = sentinel_ref_alloc("ccc", 3);
    ASSERT(p3 != 0);
    ASSERT(sentinel_ref_lookup(p3, &len) != NULL);
    ASSERT(len == 3);
    return 1;
}
/* }}} */

/* {{{ Capability validation */
static int test_validate(void)
{
    /* No emission → no gap. */
    ASSERT(sentinel_validate(0, 0) == 0);

    /* Identical capabilities → no gap. */
    ASSERT(sentinel_validate(
        SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE,
        SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE) == 0);

    /* Consumer covers producer's emit set → no gap. */
    ASSERT(sentinel_validate(
        SENTINEL_MASK_REF,
        SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE) == 0);

    /* Producer emits something consumer can't reconstruct → gap. */
    unsigned int gap = sentinel_validate(
        SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE,
        SENTINEL_MASK_REF);
    ASSERT(gap == SENTINEL_MASK_LANG_OPAQUE);
    return 1;
}
/* }}} */

/* {{{ Kind names */
static int test_kind_names(void)
{
    ASSERT(strcmp(sentinel_kind_name(SENTINEL_NONE),             "(none)") == 0);
    ASSERT(strcmp(sentinel_kind_name(SENTINEL_REF),              "$ref") == 0);
    ASSERT(strcmp(sentinel_kind_name(SENTINEL_LANG_OPAQUE),      "$lang_opaque") == 0);
    ASSERT(strcmp(sentinel_kind_name(SENTINEL_FUNCTION_POINTER), "$function_pointer") == 0);
    return 1;
}
/* }}} */

int main(void)
{
    printf("020-sentinels-test:\n");
    RUN(detect_round_trip_ref);
    RUN(detect_round_trip_lang_opaque);
    RUN(detect_round_trip_function_pointer_stub);
    RUN(detect_skips_plain_objects);
    RUN(ref_store);
    RUN(ref_refcount_dec);
    RUN(ref_refcount_inc_dec);
    RUN(ref_slot_reuse);
    RUN(validate);
    RUN(kind_names);
    printf("\n  %d passed, %d failed\n",
           tests_passed, tests_run - tests_passed);
    return tests_run == tests_passed ? 0 : 1;
}
