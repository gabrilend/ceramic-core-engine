/* tests/009-slot-store-test.c — unit tests for the slot store.
 *
 * Built and run by `make test`. Each test returns 1 on pass, 0 on
 * fail; the harness tallies and exits nonzero on any failure.
 *
 * Coverage:
 *
 *   - 1-cell peek slot: push once, peek many times, value is
 *     idempotent.
 *   - N-cell pop slot (untagged, FIFO): push N values, pop them
 *     out in order, ring is empty.
 *   - Ring-full / empty edge cases.
 *   - Tagged pop: out-of-order pushes are popped in tag order.
 *   - Atomic-counter slot: read-and-increment, modulus, concurrent
 *     unique-value guarantee.
 *   - Concurrent push/pop on an N-cell untagged ring across two
 *     threads (smoke test for the per-slot spinlock).
 */

#include "009-slot-store.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* {{{ test_peek_1cell() */
static int test_peek_1cell(void)
{
    slot_store_t *s = slot_store_create();
    ASSERT(s);
    slot_id_t id = slot_alloc(s, 32, 1, SLOT_FLAG_NONE);
    ASSERT(id != SLOT_INVALID);
    ASSERT(slot_has_value(s, id) == 0);

    const char *msg = "hello";
    ASSERT(slot_push(s, id, msg, 5, 0) == 0);
    ASSERT(slot_has_value(s, id) == 1);

    char buf[32];
    /* Peek as many times as we like — head doesn't move. */
    for (int i = 0; i < 5; i++) {
        memset(buf, 0, sizeof buf);
        int32_t got = slot_peek(s, id, buf, sizeof buf);
        ASSERT(got == 5);
        ASSERT(memcmp(buf, "hello", 5) == 0);
        ASSERT(slot_has_value(s, id) == 1);
    }
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_pop_fifo_ncell() */
static int test_pop_fifo_ncell(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 8, 4, SLOT_FLAG_NONE);
    ASSERT(id != SLOT_INVALID);

    const char *vals[] = {"a", "bb", "ccc", "dddd"};
    for (int i = 0; i < 4; i++) {
        ASSERT(slot_push(s, id, vals[i], (int)strlen(vals[i]), 0) == 0);
    }
    ASSERT(slot_fill_count(s, id) == 4);
    /* Ring full — fifth push should fail. */
    ASSERT(slot_push(s, id, "eeeee", 5, 0) == -1);

    char buf[16];
    for (int i = 0; i < 4; i++) {
        memset(buf, 0, sizeof buf);
        int32_t got = slot_pop(s, id, buf, sizeof buf);
        ASSERT(got == (int32_t)strlen(vals[i]));
        ASSERT(memcmp(buf, vals[i], (size_t)got) == 0);
    }
    /* Empty — pop fails. */
    ASSERT(slot_pop(s, id, buf, sizeof buf) == -1);
    ASSERT(slot_has_value(s, id) == 0);

    /* Push again after drain — wrap-around. */
    ASSERT(slot_push(s, id, "wrap", 4, 0) == 0);
    ASSERT(slot_pop(s, id, buf, sizeof buf) == 4);
    ASSERT(memcmp(buf, "wrap", 4) == 0);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_buf_too_small() */
static int test_buf_too_small(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 16, 1, SLOT_FLAG_NONE);
    ASSERT(slot_push(s, id, "hello world", 11, 0) == 0);

    char buf[4];
    ASSERT(slot_peek(s, id, buf, 4) == -1);
    /* But large enough should succeed. */
    char big[32];
    ASSERT(slot_peek(s, id, big, sizeof big) == 11);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_size_too_big_push() */
static int test_size_too_big_push(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 4, 2, SLOT_FLAG_NONE);
    /* cell_capacity = 4, pushing 5 bytes must fail. */
    ASSERT(slot_push(s, id, "12345", 5, 0) == -1);
    /* Exact fit ok. */
    ASSERT(slot_push(s, id, "1234", 4, 0) == 0);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_tagged_pop_lowest_first() */
static int test_tagged_pop_lowest_first(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 8, 8, SLOT_FLAG_TAGGED);
    ASSERT(id != SLOT_INVALID);

    /* Push out of tag order. */
    ASSERT(slot_push(s, id, "C", 1, 3) == 0);
    ASSERT(slot_push(s, id, "A", 1, 1) == 0);
    ASSERT(slot_push(s, id, "B", 1, 2) == 0);
    ASSERT(slot_push(s, id, "D", 1, 4) == 0);
    ASSERT(slot_fill_count(s, id) == 4);

    char buf[2];
    /* Pop should return in tag order: 1, 2, 3, 4 → A, B, C, D. */
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'A');
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'B');
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'C');
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'D');
    ASSERT(slot_pop(s, id, buf, 2) == -1);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_tagged_interleaved() */
static int test_tagged_interleaved(void)
{
    /* Push some, pop some, push more, verify order remains correct
     * across the wrap. */
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 8, 4, SLOT_FLAG_TAGGED);

    ASSERT(slot_push(s, id, "y", 1, 2) == 0);   /* tag 2 */
    ASSERT(slot_push(s, id, "x", 1, 1) == 0);   /* tag 1 */
    char buf[2] = {0};
    ASSERT(slot_pop(s, id, buf, 2) == 1);
    ASSERT(buf[0] == 'x');                       /* tag 1 first */

    ASSERT(slot_push(s, id, "z", 1, 3) == 0);   /* tag 3 */
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'y');
    memset(buf, 0, 2); ASSERT(slot_pop(s, id, buf, 2) == 1); ASSERT(buf[0] == 'z');

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_atomic_counter() */
static int test_atomic_counter(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 0, SLOT_FLAG_ATOMIC_COUNTER);
    ASSERT(id != SLOT_INVALID);

    /* mod = 3: should cycle 0, 1, 2, 0, 1, 2, ... */
    for (int i = 0; i < 9; i++) {
        uint32_t v = slot_read_inc(s, id, 3);
        ASSERT(v == (uint32_t)(i % 3));
    }
    /* push / peek / pop must reject this slot type. */
    char buf[4];
    ASSERT(slot_push(s, id, "x", 1, 0) == -1);
    ASSERT(slot_peek(s, id, buf, 4) == -1);
    ASSERT(slot_pop (s, id, buf, 4) == -1);

    /* has_value is always 1 on a counter slot. */
    ASSERT(slot_has_value(s, id) == 1);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_atomic_counter_mod_zero() */
static int test_atomic_counter_mod_zero(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 0, SLOT_FLAG_ATOMIC_COUNTER);
    ASSERT(slot_read_inc(s, id, 0) == UINT32_MAX);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_bad_slot_id() */
static int test_bad_slot_id(void)
{
    slot_store_t *s = slot_store_create();
    char buf[4];
    ASSERT(slot_push(s, 0,  "x", 1, 0) == -1);
    ASSERT(slot_pop (s, -1, buf, 4)    == -1);
    ASSERT(slot_peek(s, 99, buf, 4)    == -1);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ Concurrent counter — N threads, M reads each, all unique */
struct counter_args {
    slot_store_t *store;
    slot_id_t     id;
    int           n;
    uint32_t     *out;
};

static void *counter_worker(void *arg)
{
    struct counter_args *a = arg;
    for (int i = 0; i < a->n; i++) {
        a->out[i] = slot_read_inc(a->store, a->id, UINT32_MAX);
    }
    return NULL;
}

static int cmp_u32(const void *l, const void *r)
{
    uint32_t lv = *(const uint32_t *)l, rv = *(const uint32_t *)r;
    return (lv < rv) ? -1 : (lv > rv) ? 1 : 0;
}

static int test_concurrent_counter(void)
{
    enum { N_THREADS = 8, N_READS = 1000 };
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 0, SLOT_FLAG_ATOMIC_COUNTER);

    pthread_t   threads[N_THREADS];
    uint32_t    out[N_THREADS * N_READS];
    struct counter_args args[N_THREADS];

    for (int t = 0; t < N_THREADS; t++) {
        args[t].store = s;
        args[t].id    = id;
        args[t].n     = N_READS;
        args[t].out   = out + t * N_READS;
        pthread_create(&threads[t], NULL, counter_worker, &args[t]);
    }
    for (int t = 0; t < N_THREADS; t++) pthread_join(threads[t], NULL);

    /* Every value must be unique and cover 0..N_THREADS*N_READS-1. */
    qsort(out, N_THREADS * N_READS, sizeof out[0], cmp_u32);
    for (uint32_t i = 0; i < N_THREADS * N_READS; i++) {
        ASSERT(out[i] == i);
    }
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ Concurrent push/pop on an N-cell ring */
struct prod_args {
    slot_store_t *store;
    slot_id_t     id;
    int           n;
    int           sum;   /* consumer writes the running total here */
};

static void *producer(void *arg)
{
    struct prod_args *a = arg;
    for (int i = 0; i < a->n; i++) {
        /* Spin if ring full; producer/consumer race intentionally. */
        while (slot_push(a->store, a->id, &i, (int)sizeof i, 0) != 0) {
            /* busy-wait */
        }
    }
    return NULL;
}

static void *consumer(void *arg)
{
    struct prod_args *a = arg;
    int seen = 0;
    while (seen < a->n) {
        int v;
        if (slot_pop(a->store, a->id, &v, (int)sizeof v) > 0) {
            a->sum += v;
            seen++;
        }
    }
    return NULL;
}

static int test_concurrent_push_pop(void)
{
    enum { N = 5000 };
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, (int)sizeof(int), 16, SLOT_FLAG_NONE);

    struct prod_args pa = { s, id, N, 0 };
    struct prod_args ca = { s, id, N, 0 };
    pthread_t tp, tc;
    pthread_create(&tp, NULL, producer, &pa);
    pthread_create(&tc, NULL, consumer, &ca);
    pthread_join(tp, NULL);
    pthread_join(tc, NULL);

    /* Producer pushed 0..N-1; consumer's running sum must match. */
    int expected = N * (N - 1) / 2;
    ASSERT(ca.sum == expected);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_store_size_growth() */
static int test_store_size_growth(void)
{
    /* Allocate enough slots to cross the initial capacity boundary
     * (16) and verify slot_id_t stays monotonic. */
    slot_store_t *s = slot_store_create();
    slot_id_t prev = -1;
    for (int i = 0; i < 50; i++) {
        slot_id_t id = slot_alloc(s, 4, 1, SLOT_FLAG_NONE);
        ASSERT(id != SLOT_INVALID);
        ASSERT(id == prev + 1);
        prev = id;
    }
    ASSERT(slot_store_size(s) == 50);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_large_value_push_pop() */
/* SLOT_FLAG_LARGE_VALUE accepts any size on push, regardless of
 * cell_capacity (which is ignored). Bytes live in the slot store's
 * lazy-created lvh heap; the cell holds a stable pointer. Peek
 * and pop both copy from the heap back into the caller buffer. */
static int test_large_value_push_pop(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 4, SLOT_FLAG_LARGE_VALUE);
    ASSERT(id != SLOT_INVALID);

    /* 100 KB push — well beyond any sane fixed cell size. */
    int payload_size = 100 * 1024;
    unsigned char *src = malloc((size_t)payload_size);
    for (int i = 0; i < payload_size; i++) src[i] = (unsigned char)(i & 0xFF);
    ASSERT(slot_push(s, id, src, payload_size, 0) == 0);

    /* Peek twice — value stays in the cell. */
    unsigned char *dst = malloc((size_t)payload_size);
    int32_t got = slot_peek(s, id, dst, payload_size);
    ASSERT(got == payload_size);
    for (int i = 0; i < payload_size; i++) ASSERT(dst[i] == src[i]);

    memset(dst, 0, (size_t)payload_size);
    got = slot_peek(s, id, dst, payload_size);
    ASSERT(got == payload_size);
    ASSERT(dst[0] == 0 && dst[payload_size - 1] == (unsigned char)((payload_size - 1) & 0xFF));

    /* Pop drains. */
    memset(dst, 0, (size_t)payload_size);
    got = slot_pop(s, id, dst, payload_size);
    ASSERT(got == payload_size);
    for (int i = 0; i < payload_size; i++) ASSERT(dst[i] == src[i]);

    /* Empty now. */
    ASSERT(slot_pop(s, id, dst, payload_size) == -1);

    free(src); free(dst);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_large_value_buf_too_small() */
/* Pop / peek into an undersized buffer returns -1 and leaves the
 * cell in place (same semantics as fixed-size slots). */
static int test_large_value_buf_too_small(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 1, SLOT_FLAG_LARGE_VALUE);
    ASSERT(id != SLOT_INVALID);

    char src[200];
    memset(src, 'A', sizeof src);
    ASSERT(slot_push(s, id, src, sizeof src, 0) == 0);

    char small_buf[50];
    ASSERT(slot_peek(s, id, small_buf, sizeof small_buf) == -1);
    /* Cell still present — full peek succeeds. */
    char ok_buf[200];
    ASSERT(slot_peek(s, id, ok_buf, sizeof ok_buf) == (int32_t)sizeof src);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_large_value_multi_push_fifo() */
/* N-cell pop semantics still work for LARGE_VALUE slots. Push
 * three values of wildly different sizes; pop returns them in
 * order with their original bytes. */
static int test_large_value_multi_push_fifo(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 8, SLOT_FLAG_LARGE_VALUE);
    ASSERT(id != SLOT_INVALID);

    char a[]   = "alpha";
    char b[]   = "bravo-bravo-bravo";
    int big_sz = 70 * 1024;
    char *big  = malloc((size_t)big_sz);
    memset(big, 'X', (size_t)big_sz);

    ASSERT(slot_push(s, id, a,   (int)strlen(a),   0) == 0);
    ASSERT(slot_push(s, id, b,   (int)strlen(b),   0) == 0);
    ASSERT(slot_push(s, id, big, big_sz,           0) == 0);
    ASSERT(slot_fill_count(s, id) == 3);

    char buf[100 * 1024];
    int32_t got;

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == (int32_t)strlen(a));
    ASSERT(memcmp(buf, a, (size_t)got) == 0);

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == (int32_t)strlen(b));
    ASSERT(memcmp(buf, b, (size_t)got) == 0);

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == big_sz);
    for (int i = 0; i < big_sz; i++) ASSERT(buf[i] == 'X');

    free(big);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_large_value_tagged_ordering() */
/* SLOT_FLAG_LARGE_VALUE combined with SLOT_FLAG_TAGGED: pops are
 * served lowest-tag-first, with bytes intact from the heap. */
static int test_large_value_tagged_ordering(void)
{
    slot_store_t *s = slot_store_create();
    slot_id_t id = slot_alloc(s, 0, 4,
                              SLOT_FLAG_LARGE_VALUE | SLOT_FLAG_TAGGED);
    ASSERT(id != SLOT_INVALID);

    /* Push in mixed-tag order. */
    ASSERT(slot_push(s, id, "third",  5, 30) == 0);
    ASSERT(slot_push(s, id, "first",  5, 10) == 0);
    ASSERT(slot_push(s, id, "second", 6, 20) == 0);

    char buf[64];
    int32_t got;

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == 5 && memcmp(buf, "first", 5) == 0);

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == 6 && memcmp(buf, "second", 6) == 0);

    got = slot_pop(s, id, buf, sizeof buf);
    ASSERT(got == 5 && memcmp(buf, "third", 5) == 0);

    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ test_large_value_reclamation() */
/* The reclamation regression test for issue 302. Before the
 * unified-allocator integration, every push into a LARGE_VALUE slot
 * carved fresh bytes off a monotonically-growing arena that nothing
 * ever returned to. A long-running graph that pushed and popped
 * through the same slot would watch the arena climb forever.
 *
 * Now, pop drops the chunk's last reference and the chunk rejoins
 * the allocator's free-lists. This test confirms that by running
 * many push-pop iterations on a depth-1 ring and asserting the
 * allocator's "bytes in use" stays bounded — specifically, that it
 * doesn't grow with the iteration count.
 *
 * The bound used here is "no more than three chunks worth of bytes
 * at any moment" — generous enough that the eager neighbor-merge's
 * intermediate states are tolerated, tight enough to catch the
 * monotonic-growth bug it's named after. */
static int test_large_value_reclamation(void)
{
    slot_store_t *s = slot_store_create();
    ASSERT(s);
    slot_id_t id = slot_alloc(s, 0, 1, SLOT_FLAG_LARGE_VALUE);
    ASSERT(id != SLOT_INVALID);

    const int payload_size = 8 * 1024;  /* 8 KB */
    char *src = malloc((size_t)payload_size);
    char *dst = malloc((size_t)payload_size);
    memset(src, 'Z', (size_t)payload_size);

    const int iterations = 500;
    uint64_t bound = 0;  /* set after the first push so the bound
                          * tracks the actual chunk size (which the
                          * allocator may round up). */

    for (int i = 0; i < iterations; i++) {
        ASSERT(slot_push(s, id, src, payload_size, 0) == 0);
        int32_t got = slot_pop(s, id, dst, payload_size);
        ASSERT(got == payload_size);
        ASSERT(dst[0] == 'Z' && dst[payload_size - 1] == 'Z');

        uint64_t in_use = slot_store_large_value_bytes_in_use(s);
        if (i == 0) bound = in_use * 4 + 64;  /* slack for header */
        ASSERT(in_use <= bound);
    }

    free(src); free(dst);
    slot_store_destroy(s);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    printf("009-slot-store-test:\n");
    RUN(peek_1cell);
    RUN(pop_fifo_ncell);
    RUN(buf_too_small);
    RUN(size_too_big_push);
    RUN(tagged_pop_lowest_first);
    RUN(tagged_interleaved);
    RUN(atomic_counter);
    RUN(atomic_counter_mod_zero);
    RUN(bad_slot_id);
    RUN(concurrent_counter);
    RUN(concurrent_push_pop);
    RUN(store_size_growth);
    RUN(large_value_push_pop);
    RUN(large_value_buf_too_small);
    RUN(large_value_multi_push_fifo);
    RUN(large_value_tagged_ordering);
    RUN(large_value_reclamation);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
