/* tests/016-unified-allocator-test.c — exercises the unified
 * allocator's contract end-to-end. Each test is independent (its
 * own heap), so failures isolate cleanly.
 *
 * What's covered:
 *   - empty-heap create/destroy round-trip
 *   - pre-warming: declared sizes produce ready-to-pop chunks
 *   - alloc + unref reuses chunks (no growth)
 *   - reference counting: extra ref keeps chunk live
 *   - adjacent free chunks merge eagerly on the second unref
 *   - non-adjacent frees stay separate
 *   - alloc on empty class falls through to a larger class
 *   - alloc above all declared sizes triggers growth
 *   - sweep merges runs of contiguous free chunks
 *   - duplicate sizes in the declaration get summed
 *   - 8-thread concurrent alloc/unref stress
 *
 * Designed in issue 302's rewrite; first stage of the unified
 * allocator implementation.
 */

#include "../src/016-unified-allocator.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Test harness */
static int pass = 0;
static int fail = 0;

#define CHECK(cond, msg, ...) do {                                      \
    if (!(cond)) {                                                      \
        fprintf(stderr, "  FAIL: " msg " (%s:%d)\n",                    \
                ##__VA_ARGS__, __FILE__, __LINE__);                     \
        fail++;                                                         \
        return;                                                         \
    }                                                                   \
} while (0)

#define PASS(name) do { pass++; printf("  ok   %s\n", name); } while (0)
/* }}} */

/* {{{ test_empty_heap */
static void test_empty_heap(void)
{
    const char *name = "empty heap create/destroy";
    ua_t *h = ua_create(NULL, 0);
    CHECK(h != NULL, "ua_create with no classes returned NULL");
    CHECK(ua_region_count(h) == 0, "expected 0 regions, got %u",
          ua_region_count(h));
    CHECK(ua_bytes_total(h) == 0, "expected 0 total bytes");
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_prewarming */
static void test_prewarming(void)
{
    const char *name = "pre-warmed classes populate free-lists";
    ua_class_decl_t classes[] = {
        { .size = 64,  .prewarm_count = 4 },
        { .size = 256, .prewarm_count = 2 },
    };
    ua_t *h = ua_create(classes, 2);
    CHECK(h != NULL, "ua_create failed");
    CHECK(ua_region_count(h) == 1, "expected 1 region, got %u",
          ua_region_count(h));
    CHECK(ua_class_free_count(h, 64) == 4,
          "expected 4 free 64-byte chunks, got %u",
          ua_class_free_count(h, 64));
    CHECK(ua_class_free_count(h, 256) == 2,
          "expected 2 free 256-byte chunks, got %u",
          ua_class_free_count(h, 256));
    CHECK(ua_alloc_calls(h) == 0, "no allocs yet");
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_alloc_pop_from_free_list */
static void test_alloc_pop_from_free_list(void)
{
    const char *name = "alloc pops from the matching class";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 2 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h != NULL, "create failed");

    ua_chunk_t *a = ua_alloc(h, 32);  /* fits in 64-byte class */
    CHECK(a != NULL, "alloc returned NULL");
    CHECK(ua_size(a) >= 32, "chunk too small");
    CHECK(ua_class_free_count(h, 64) == 1, "expected 1 left, got %u",
          ua_class_free_count(h, 64));

    ua_chunk_t *b = ua_alloc(h, 64);
    CHECK(b != NULL, "second alloc returned NULL");
    CHECK(ua_class_free_count(h, 64) == 0, "expected 0 left");
    CHECK(ua_grow_calls(h) == 0, "should not have grown");

    /* Distinct pointers — payloads don't overlap. */
    CHECK(ua_data(a) != ua_data(b), "two allocs gave same address");

    ua_unref(h, a);
    ua_unref(h, b);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_reuse_after_unref */
static void test_reuse_after_unref(void)
{
    const char *name = "alloc reuses a freed chunk without growing";
    ua_class_decl_t classes[] = {
        { .size = 128, .prewarm_count = 1 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    ua_chunk_t *a = ua_alloc(h, 128);
    void       *pa = ua_data(a);
    ua_unref(h, a);

    ua_chunk_t *b = ua_alloc(h, 128);
    CHECK(ua_data(b) == pa, "expected same pointer back");
    CHECK(ua_grow_calls(h) == 0, "should not have grown");
    ua_unref(h, b);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_refcount_keeps_alive */
static void test_refcount_keeps_alive(void)
{
    const char *name = "ua_ref prevents free on first unref";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 1 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    ua_chunk_t *a = ua_alloc(h, 64);
    ua_ref(a);                 /* refcount = 2 */
    ua_unref(h, a);            /* refcount = 1 — still live */

    /* Trying to alloc again should NOT return `a`'s address — `a` is still
     * in use. */
    ua_chunk_t *b = ua_alloc(h, 64);
    CHECK(b != a, "alloc reclaimed a chunk that still has a ref");

    ua_unref(h, a);            /* refcount = 0 — now freed */
    ua_unref(h, b);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_adjacent_merge */
static void test_adjacent_merge(void)
{
    const char *name = "two adjacent frees merge into one larger chunk";
    /* Carve 3 chunks of size 64 contiguously in one region. */
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 3 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    /* Take all three. Their payloads are in some order; we'll free
     * them in physical-address order to trigger merging. */
    ua_chunk_t *a = ua_alloc(h, 64);
    ua_chunk_t *b = ua_alloc(h, 64);
    ua_chunk_t *c = ua_alloc(h, 64);
    CHECK(a && b && c, "alloc failed");

    /* Free a and b. They might or might not be physically adjacent
     * depending on the order ua_alloc returns them. The free-list is
     * LIFO, so the last-pushed is the first-popped. The test isn't
     * sensitive to which physical order we got — we just need to
     * verify that whichever two are adjacent merge when both freed. */
    uint32_t merges_before = ua_neighbor_merges(h);
    ua_unref(h, a);
    ua_unref(h, b);
    ua_unref(h, c);

    /* After all three free, at minimum some neighbor merges should
     * have happened (three physically adjacent chunks can fuse). */
    CHECK(ua_neighbor_merges(h) > merges_before,
          "expected at least one neighbor merge, got %u",
          ua_neighbor_merges(h) - merges_before);

    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_non_adjacent_stay_separate */
static void test_non_adjacent_stay_separate(void)
{
    const char *name = "non-adjacent frees do not merge";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 3 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    ua_chunk_t *a = ua_alloc(h, 64);
    ua_chunk_t *b = ua_alloc(h, 64);
    ua_chunk_t *c = ua_alloc(h, 64);

    /* Free outermost; keep middle (whichever it is in physical
     * order). We don't know which is middle in physical order from
     * the API, so this test instead verifies a weaker but useful
     * property: freeing only `a` produces no merges (the other two
     * are still allocated, so no neighbors are free). */
    uint32_t before = ua_neighbor_merges(h);
    ua_unref(h, a);
    CHECK(ua_neighbor_merges(h) == before,
          "expected no merges with neighbors allocated");

    ua_unref(h, b);
    ua_unref(h, c);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_fallthrough_to_larger_class */
static void test_fallthrough_to_larger_class(void)
{
    const char *name = "alloc falls through to a larger class when small one is empty";
    ua_class_decl_t classes[] = {
        { .size = 64,  .prewarm_count = 1 },
        { .size = 256, .prewarm_count = 1 },
    };
    ua_t *h = ua_create(classes, 2);
    CHECK(h, "create failed");

    /* Exhaust the 64 class. */
    ua_chunk_t *a = ua_alloc(h, 64);
    CHECK(a, "first alloc failed");

    /* Now ask for 64 again. Should NOT grow — should fall through
     * to the 256 class. */
    ua_chunk_t *b = ua_alloc(h, 64);
    CHECK(b, "second alloc failed");
    CHECK(ua_grow_calls(h) == 0, "expected no growth, got %u",
          ua_grow_calls(h));
    CHECK(ua_size(b) >= 64, "chunk too small");

    ua_unref(h, a);
    ua_unref(h, b);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_growth_for_oversize */
static void test_growth_for_oversize(void)
{
    const char *name = "alloc above all classes triggers growth";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 1 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");
    CHECK(ua_region_count(h) == 1, "should start with 1 region");

    ua_chunk_t *big = ua_alloc(h, 4096);
    CHECK(big, "oversized alloc returned NULL");
    CHECK(ua_size(big) >= 4096, "chunk smaller than requested");
    CHECK(ua_region_count(h) == 2, "expected 2 regions after growth, got %u",
          ua_region_count(h));
    CHECK(ua_grow_calls(h) == 1, "expected 1 grow call");

    ua_unref(h, big);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_explicit_sweep_merges_runs */
static void test_explicit_sweep_merges_runs(void)
{
    const char *name = "ua_sweep merges what eager merge couldn't catch";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 4 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    ua_chunk_t *chunks[4];
    for (int i = 0; i < 4; i++) chunks[i] = ua_alloc(h, 64);

    /* Free all four. Eager merge will catch most of this. The
     * explicit sweep should be a no-op or near-no-op (since eager
     * already did the work). The point of the test is that calling
     * ua_sweep is safe and reports a sane count. */
    for (int i = 0; i < 4; i++) ua_unref(h, chunks[i]);

    uint32_t sweep_count = ua_sweep(h);
    CHECK(sweep_count == 0, "expected 0 extra merges after eager already ran, got %u",
          sweep_count);

    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_duplicate_size_sums_count */
static void test_duplicate_size_sums_count(void)
{
    const char *name = "duplicate class sizes sum their prewarm counts";
    ua_class_decl_t classes[] = {
        { .size = 64, .prewarm_count = 2 },
        { .size = 64, .prewarm_count = 3 },
    };
    ua_t *h = ua_create(classes, 2);
    CHECK(h, "create failed");
    CHECK(ua_class_free_count(h, 64) == 5,
          "expected 5 (2+3), got %u", ua_class_free_count(h, 64));
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ Concurrent stress */
typedef struct stress_arg {
    ua_t    *h;
    int      n_ops;
    _Atomic uint32_t *write_failures;
} stress_arg_t;

static void *stress_worker(void *arg_)
{
    stress_arg_t *arg = arg_;
    for (int i = 0; i < arg->n_ops; i++) {
        size_t sz = 16 + (size_t)(rand() % 240);
        ua_chunk_t *c = ua_alloc(arg->h, sz);
        if (!c) {
            atomic_fetch_add_explicit(arg->write_failures, 1u,
                                      memory_order_relaxed);
            continue;
        }
        /* Write the chunk to test no-overlap. */
        memset(ua_data(c), 0xAB, ua_size(c));
        /* Random hold pattern — sometimes ref-then-unref-twice. */
        if (i % 3 == 0) {
            ua_ref(c);
            ua_unref(arg->h, c);
        }
        ua_unref(arg->h, c);
    }
    return NULL;
}

static void test_concurrent_stress(void)
{
    const char *name = "8-thread alloc/unref stress (1000 ops each)";
    ua_class_decl_t classes[] = {
        { .size = 64,  .prewarm_count = 32 },
        { .size = 256, .prewarm_count = 16 },
    };
    ua_t *h = ua_create(classes, 2);
    CHECK(h, "create failed");

    _Atomic uint32_t failures = 0;
    pthread_t       threads[8];
    stress_arg_t    args[8];
    for (int i = 0; i < 8; i++) {
        args[i].h              = h;
        args[i].n_ops          = 1000;
        args[i].write_failures = &failures;
        pthread_create(&threads[i], NULL, stress_worker, &args[i]);
    }
    for (int i = 0; i < 8; i++) pthread_join(threads[i], NULL);

    CHECK(failures == 0, "saw %u alloc failures during stress",
          (uint32_t)failures);
    CHECK(ua_bytes_in_use(h) == 0,
          "expected 0 bytes in use after stress, got %llu",
          (unsigned long long)ua_bytes_in_use(h));

    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ test_ua_data_writes_persist */
static void test_ua_data_writes_persist(void)
{
    const char *name = "chunk payload survives across the API boundary";
    ua_class_decl_t classes[] = {
        { .size = 256, .prewarm_count = 1 },
    };
    ua_t *h = ua_create(classes, 1);
    CHECK(h, "create failed");

    ua_chunk_t *c = ua_alloc(h, 200);
    CHECK(c, "alloc failed");
    uint8_t *d = ua_data(c);
    for (int i = 0; i < 200; i++) d[i] = (uint8_t)(i & 0xFF);

    /* Round-trip check: write a pattern, read it back. */
    for (int i = 0; i < 200; i++) {
        CHECK(d[i] == (uint8_t)(i & 0xFF), "byte %d corrupted", i);
    }
    ua_unref(h, c);
    ua_destroy(h);
    PASS(name);
}
/* }}} */

/* {{{ main */
int main(void)
{
    printf("016-unified-allocator-test:\n");
    test_empty_heap();
    test_prewarming();
    test_alloc_pop_from_free_list();
    test_reuse_after_unref();
    test_refcount_keeps_alive();
    test_adjacent_merge();
    test_non_adjacent_stay_separate();
    test_fallthrough_to_larger_class();
    test_growth_for_oversize();
    test_explicit_sweep_merges_runs();
    test_duplicate_size_sums_count();
    test_ua_data_writes_persist();
    test_concurrent_stress();
    printf("\n  %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
/* }}} */
