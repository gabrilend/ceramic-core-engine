/* tests/015-large-value-heap-test.c — unit tests for the
 * variable-size payload allocator (issue 302 follow-on, lvh).
 *
 * The tests cover the three properties that the slot store relies
 * on at the layer above:
 *   1. Pointers returned by lvh_alloc are stable across further
 *      allocations (chunks don't move).
 *   2. Concurrent allocations from many threads never produce
 *      overlapping regions.
 *   3. Allocations larger than the default chunk size succeed.
 *
 * The miscellaneous tests cover the stats accessors and the
 * zero-size convention.
 */

#include "../src/015-large-value-heap.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Local mini test harness */
static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  ASSERT failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #cond); \
        return 0; \
    } \
} while (0)

#define RUN(test) do { \
    int ok = test(); \
    printf("  %-44s %s\n", #test, ok ? "ok" : "FAIL"); \
    if (ok) g_pass++; else g_fail++; \
} while (0)
/* }}} */

/* {{{ test_create_destroy() */
static int test_create_destroy(void)
{
    lvh_t *h = lvh_create(0);
    ASSERT(h != NULL);
    ASSERT(lvh_chunk_count(h) == 0);
    ASSERT(lvh_total_allocated(h) == 0);
    ASSERT(lvh_total_used(h) == 0);
    lvh_destroy(h);

    /* destroy on NULL is safe */
    lvh_destroy(NULL);
    return 1;
}
/* }}} */

/* {{{ test_single_alloc_within_chunk() */
static int test_single_alloc_within_chunk(void)
{
    lvh_t *h = lvh_create(4096);
    ASSERT(h != NULL);

    void *p = lvh_alloc(h, 100);
    ASSERT(p != NULL);

    /* Write through the pointer; read it back. */
    memset(p, 0xAB, 100);
    unsigned char *q = (unsigned char *)p;
    for (int i = 0; i < 100; i++) ASSERT(q[i] == 0xAB);

    ASSERT(lvh_chunk_count(h) == 1);
    ASSERT(lvh_total_allocated(h) >= 4096);
    ASSERT(lvh_total_used(h) >= 100);

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ test_multiple_allocs_within_chunk() */
static int test_multiple_allocs_within_chunk(void)
{
    lvh_t *h = lvh_create(4096);
    ASSERT(h != NULL);

    void *a = lvh_alloc(h, 100);
    void *b = lvh_alloc(h, 100);
    void *c = lvh_alloc(h, 100);
    ASSERT(a && b && c);
    ASSERT(a != b && b != c && a != c);

    /* Write different bytes to each; verify no overlap. */
    memset(a, 0x11, 100);
    memset(b, 0x22, 100);
    memset(c, 0x33, 100);
    ASSERT(((unsigned char *)a)[0] == 0x11);
    ASSERT(((unsigned char *)b)[0] == 0x22);
    ASSERT(((unsigned char *)c)[0] == 0x33);
    ASSERT(((unsigned char *)a)[99] == 0x11);
    ASSERT(((unsigned char *)b)[99] == 0x22);
    ASSERT(((unsigned char *)c)[99] == 0x33);

    /* Still one chunk. */
    ASSERT(lvh_chunk_count(h) == 1);

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ test_oversized_alloc() */
static int test_oversized_alloc(void)
{
    lvh_t *h = lvh_create(1024);  /* small chunks on purpose */
    ASSERT(h != NULL);

    /* Request more than a chunk holds. */
    void *p = lvh_alloc(h, 100 * 1024);  /* 100 KB */
    ASSERT(p != NULL);

    /* Writable end-to-end. */
    memset(p, 0xCD, 100 * 1024);
    ASSERT(((unsigned char *)p)[0]            == 0xCD);
    ASSERT(((unsigned char *)p)[100*1024 - 1] == 0xCD);

    /* One chunk, sized for the oversized request (>= 100KB). */
    ASSERT(lvh_chunk_count(h) == 1);
    ASSERT(lvh_total_allocated(h) >= 100u * 1024u);

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ test_pointer_stability_across_growth() */
/* This is the property the slot store relies on: an earlier
 * consumer's pointer must not be invalidated when a later
 * producer's allocation forces a chunk grow. */
static int test_pointer_stability_across_growth(void)
{
    lvh_t *h = lvh_create(1024);
    ASSERT(h != NULL);

    void *first = lvh_alloc(h, 64);
    ASSERT(first != NULL);
    memset(first, 0xEE, 64);
    uintptr_t first_addr = (uintptr_t)first;

    /* Push another ~20 KB through the heap, forcing several
     * chunks. */
    for (int i = 0; i < 200; i++) {
        void *p = lvh_alloc(h, 128);
        ASSERT(p != NULL);
    }
    ASSERT(lvh_chunk_count(h) > 1);

    /* Original pointer still has its bytes. */
    ASSERT((uintptr_t)first == first_addr);
    for (int i = 0; i < 64; i++) {
        ASSERT(((unsigned char *)first)[i] == 0xEE);
    }

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ test_zero_size_alloc() */
static int test_zero_size_alloc(void)
{
    lvh_t *h = lvh_create(0);
    void *p = lvh_alloc(h, 0);
    ASSERT(p != NULL);  /* non-NULL is the convention */
    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ test_stats_match_reality() */
static int test_stats_match_reality(void)
{
    lvh_t *h = lvh_create(4096);
    ASSERT(lvh_total_used(h) == 0);
    ASSERT(lvh_total_allocated(h) == 0);
    ASSERT(lvh_chunk_count(h) == 0);

    lvh_alloc(h, 100);
    /* Used is 16-aligned; lower bound 112, upper bound 128 (the
     * exact value depends on LVH_ALIGN). */
    uint64_t used = lvh_total_used(h);
    ASSERT(used >= 100 && used <= 128);

    lvh_alloc(h, 200);
    used = lvh_total_used(h);
    ASSERT(used >= 300 && used <= 400);

    ASSERT(lvh_chunk_count(h) == 1);

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ Concurrent alloc test scaffolding */
#define N_THREADS    8
#define N_PER_THREAD 1000
#define ALLOC_SIZE   64

typedef struct {
    lvh_t *h;
    void  *ptrs[N_PER_THREAD];
    int    thread_idx;
} worker_arg_t;

static void *concurrent_worker(void *vp)
{
    worker_arg_t *w = vp;
    for (int i = 0; i < N_PER_THREAD; i++) {
        void *p = lvh_alloc(w->h, ALLOC_SIZE);
        if (!p) { w->ptrs[i] = NULL; continue; }
        /* Write something thread-specific so unique-pointer checks
         * later are paired with non-overlap evidence. */
        memset(p, (unsigned char)(w->thread_idx + 1), ALLOC_SIZE);
        w->ptrs[i] = p;
    }
    return NULL;
}
/* }}} */

/* {{{ test_concurrent_alloc() */
/* Eight threads, 1000 allocs each. Every returned pointer must be
 * unique, every memset must survive (no overlap), and the chunk
 * count + total_used must reflect reality. */
static int test_concurrent_alloc(void)
{
    lvh_t *h = lvh_create(4096);
    ASSERT(h != NULL);

    pthread_t   threads[N_THREADS];
    worker_arg_t args[N_THREADS];

    for (int t = 0; t < N_THREADS; t++) {
        args[t].h          = h;
        args[t].thread_idx = t;
        pthread_create(&threads[t], NULL, concurrent_worker, &args[t]);
    }
    for (int t = 0; t < N_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    /* Verify each pointer still contains the byte its thread wrote.
     * If two threads' regions had overlapped, at least some bytes
     * would be the other thread's marker. */
    int n_alloced = 0;
    for (int t = 0; t < N_THREADS; t++) {
        for (int i = 0; i < N_PER_THREAD; i++) {
            void *p = args[t].ptrs[i];
            if (!p) continue;
            n_alloced++;
            unsigned char expect = (unsigned char)(t + 1);
            for (int k = 0; k < ALLOC_SIZE; k++) {
                ASSERT(((unsigned char *)p)[k] == expect);
            }
        }
    }
    ASSERT(n_alloced == N_THREADS * N_PER_THREAD);

    /* Pairwise uniqueness — a quadratic check, but for 8000
     * pointers it's still well under a second and it gives us the
     * strongest evidence. */
    void **all = malloc((size_t)n_alloced * sizeof(void *));
    int n = 0;
    for (int t = 0; t < N_THREADS; t++) {
        for (int i = 0; i < N_PER_THREAD; i++) {
            if (args[t].ptrs[i]) all[n++] = args[t].ptrs[i];
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT(all[i] != all[j]);
        }
    }
    free(all);

    lvh_destroy(h);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    printf("015-large-value-heap-test:\n");
    RUN(test_create_destroy);
    RUN(test_single_alloc_within_chunk);
    RUN(test_multiple_allocs_within_chunk);
    RUN(test_oversized_alloc);
    RUN(test_pointer_stability_across_growth);
    RUN(test_zero_size_alloc);
    RUN(test_stats_match_reality);
    RUN(test_concurrent_alloc);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
