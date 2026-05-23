/* tests/017-box-id-test.c — unit tests for src/017-box-id.
 *
 * Covers: output format, monotonic uniqueness in single-threaded
 * use, rejection on undersized buffer, and concurrent uniqueness
 * across multiple threads.
 */
#include "017-box-id.h"

#include <pthread.h>
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

/* {{{ test_format() — output matches "auto_XXXXXXXX" */
static int test_format(void)
{
    char buf[BOX_ID_GEN_BUF_SIZE];
    ASSERT(box_id_generate(buf, sizeof buf) == 0);
    ASSERT(strlen(buf) == 13);
    ASSERT(strncmp(buf, "auto_", 5) == 0);
    /* Suffix is 8 chars of [0-9a-f]. */
    for (int i = 5; i < 13; i++) {
        char c = buf[i];
        int hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        ASSERT(hex);
    }
    return 1;
}
/* }}} */

/* {{{ test_monotonic_uniqueness() — successive calls produce
 *                                   distinct strings */
static int test_monotonic_uniqueness(void)
{
    char a[BOX_ID_GEN_BUF_SIZE], b[BOX_ID_GEN_BUF_SIZE];
    ASSERT(box_id_generate(a, sizeof a) == 0);
    ASSERT(box_id_generate(b, sizeof b) == 0);
    ASSERT(strcmp(a, b) != 0);
    return 1;
}
/* }}} */

/* {{{ test_buf_too_small() — rejected with -1, no write */
static int test_buf_too_small(void)
{
    char buf[BOX_ID_GEN_BUF_SIZE - 1] = "preserve";
    ASSERT(box_id_generate(buf, sizeof buf) == -1);
    /* The function should not have written anything; the leading
     * "preserve" bytes remain. */
    ASSERT(strncmp(buf, "preserve", 8) == 0);
    return 1;
}
/* }}} */

/* {{{ test_null_buf() — rejected with -1 */
static int test_null_buf(void)
{
    ASSERT(box_id_generate(NULL, BOX_ID_GEN_BUF_SIZE) == -1);
    return 1;
}
/* }}} */

/* {{{ test_concurrent_uniqueness() — many threads, all ids unique
 *
 * Spawns 8 worker threads, each generates IDS_PER_THREAD ids and
 * stashes them in a per-thread array. Main thread collects them,
 * sorts, and verifies no duplicates. If the atomic counter had
 * been a plain int with a race window, this would surface
 * collisions on contended runs. */
enum { N_THREADS = 8, IDS_PER_THREAD = 1024 };

struct gen_ctx {
    char ids[IDS_PER_THREAD][BOX_ID_GEN_BUF_SIZE];
};

static void *gen_worker(void *arg)
{
    struct gen_ctx *ctx = arg;
    for (int i = 0; i < IDS_PER_THREAD; i++) {
        if (box_id_generate(ctx->ids[i], BOX_ID_GEN_BUF_SIZE) != 0) {
            /* Shouldn't happen — flag with empty string for the
             * checker. */
            ctx->ids[i][0] = '\0';
        }
    }
    return NULL;
}

static int cmp_strings(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static int test_concurrent_uniqueness(void)
{
    pthread_t       threads[N_THREADS];
    struct gen_ctx *ctxs = calloc(N_THREADS, sizeof *ctxs);
    ASSERT(ctxs != NULL);

    for (int t = 0; t < N_THREADS; t++) {
        pthread_create(&threads[t], NULL, gen_worker, &ctxs[t]);
    }
    for (int t = 0; t < N_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    /* Pack all ids into one flat array for sort-and-check. */
    int total = N_THREADS * IDS_PER_THREAD;
    char (*flat)[BOX_ID_GEN_BUF_SIZE] = malloc((size_t)total * BOX_ID_GEN_BUF_SIZE);
    ASSERT(flat != NULL);
    int k = 0;
    for (int t = 0; t < N_THREADS; t++) {
        for (int i = 0; i < IDS_PER_THREAD; i++) {
            ASSERT(ctxs[t].ids[i][0] != '\0');
            memcpy(flat[k++], ctxs[t].ids[i], BOX_ID_GEN_BUF_SIZE);
        }
    }
    qsort(flat, (size_t)total, BOX_ID_GEN_BUF_SIZE, cmp_strings);
    for (int i = 1; i < total; i++) {
        ASSERT(strcmp(flat[i - 1], flat[i]) != 0);
    }

    free(flat);
    free(ctxs);
    return 1;
}
/* }}} */

int main(void)
{
    printf("017-box-id-test:\n");
    RUN(format);
    RUN(monotonic_uniqueness);
    RUN(buf_too_small);
    RUN(null_buf);
    RUN(concurrent_uniqueness);
    printf("\n  %d passed, %d failed\n",
           tests_passed, tests_run - tests_passed);
    return tests_run == tests_passed ? 0 : 1;
}
