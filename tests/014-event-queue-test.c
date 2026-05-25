/* tests/014-event-queue-test.c — concurrent producer + dedicated
 * writer thread tests.
 *
 * Single-producer sanity, multi-thread burst, ordering check
 * (within a producer, events stay in submission order), shutdown
 * drain (events queued at shutdown still hit the file).
 */

#include "014-event-queue.h"
#include "json.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* {{{ tmp_path() / slurp_file() */
static void tmp_path(char *out, size_t cap)
{
    snprintf(out, cap, "/tmp/soramech-evq-%d-XXXXXX", (int)getpid());
    int fd = mkstemp(out);
    if (fd >= 0) close(fd);
}

static char *slurp_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

static int count_lines(const char *body, size_t len)
{
    int n = 0;
    for (size_t i = 0; i < len; i++) if (body[i] == '\n') n++;
    return n;
}
/* }}} */

/* {{{ test_single_producer() */
static int test_single_producer(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);
    ASSERT(q);

    event_queue_run_start  (q, 1.0, "calc", 4);
    event_queue_task_submit(q, 1.1, 0, "add", -1);
    event_queue_task_start (q, 1.2, 0, 2);
    event_queue_task_end   (q, 1.3, 0, 2, 127, 2);
    event_queue_run_end    (q, 1.4, 1850, 1);

    event_queue_destroy(q);   /* drain + join */

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    ASSERT(count_lines(body, len) == 5);

    /* Spot-check by parsing the first and last line. */
    json_arena_t *a = json_arena_create();
    char *first_nl = strchr(body, '\n');
    *first_nl = '\0';
    json_node_t *n = json_parse(a, body, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "run_start") == 0);
    *first_nl = '\n';

    /* Find the last line (skip trailing newline). */
    char *p = body + len - 1;
    if (*p == '\n') p--;
    while (p > body && *p != '\n') p--;
    if (*p == '\n') p++;
    n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "run_end") == 0);

    json_arena_destroy(a);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_concurrent_burst() — many producers, one writer */
struct producer_args { event_queue_t *q; int n; int worker_idx; };

static void *producer_thread(void *arg)
{
    struct producer_args *a = arg;
    for (int i = 0; i < a->n; i++) {
        event_queue_task_start(a->q, (double)i, i, a->worker_idx);
    }
    return NULL;
}

static int test_concurrent_burst(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);
    ASSERT(q);

    enum { N_PROD = 8, PER = 250 };
    pthread_t threads[N_PROD];
    struct producer_args args[N_PROD];
    for (int t = 0; t < N_PROD; t++) {
        args[t].q = q; args[t].n = PER; args[t].worker_idx = t;
        pthread_create(&threads[t], NULL, producer_thread, &args[t]);
    }
    for (int t = 0; t < N_PROD; t++) pthread_join(threads[t], NULL);

    event_queue_destroy(q);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    int lines = count_lines(body, len);
    ASSERT(lines == N_PROD * PER);

    /* Every line must be parsable JSON. */
    json_arena_t *a = json_arena_create();
    char *p = body;
    int ok = 0;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = '\0';
        json_node_t *n = json_parse(a, p, NULL, NULL);
        ASSERT(n);
        ASSERT(strcmp(json_string_value(json_object_get(n, "event")),
                      "task_start") == 0);
        ok++;
        p = nl + 1;
    }
    ASSERT(ok == N_PROD * PER);

    json_arena_destroy(a);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_shutdown_drains_late_events() */
/* Emit events right before destroy; they must end up in the file. */
static int test_shutdown_drains_late_events(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);

    /* Emit a small burst without giving the writer time to drain
     * before destroy. */
    for (int i = 0; i < 50; i++) {
        event_queue_task_start(q, (double)i, i, 0);
    }
    event_queue_destroy(q);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    ASSERT(count_lines(body, len) == 50);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_high_contention_burst() — MPSC ring stress
 *
 * Drives much more parallelism than test_concurrent_burst:
 * 32 producer threads × 4000 events each = 128 000 lines, each
 * thread firing back-to-back with no sleeps. This exists to
 * exercise the Vyukov-style ring's CAS retry path and the
 * has_data / has_space CV interplay under heavy contention.
 *
 * Validation: line count must equal n_threads × per_thread, and
 * every line must parse as valid JSON. Within-thread ordering is
 * preserved by the ring (the task_id field counts up monotonically
 * per producer worker_idx). */
struct stress_args { event_queue_t *q; int n; int worker_idx; };

static void *stress_producer(void *arg)
{
    struct stress_args *a = arg;
    for (int i = 0; i < a->n; i++) {
        /* task_id == i so the consumer can verify per-thread
         * monotonicity if it cares. */
        event_queue_task_start(a->q, (double)i, i, a->worker_idx);
    }
    return NULL;
}

static int test_high_contention_burst(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);
    ASSERT(q);

    enum { N_PROD = 32, PER = 4000 };
    pthread_t threads[N_PROD];
    struct stress_args args[N_PROD];
    for (int t = 0; t < N_PROD; t++) {
        args[t].q = q; args[t].n = PER; args[t].worker_idx = t;
        pthread_create(&threads[t], NULL, stress_producer, &args[t]);
    }
    for (int t = 0; t < N_PROD; t++) pthread_join(threads[t], NULL);

    event_queue_destroy(q);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    int lines = count_lines(body, len);
    ASSERT(lines == N_PROD * PER);

    /* Verify per-thread monotonicity: scan every line, group by
     * worker_idx, check each group's task_id sequence is
     * monotonically non-decreasing. (Cross-thread interleaving is
     * allowed and expected; within a single producer the ring
     * must preserve emit order.) */
    int last_task_id[N_PROD];
    int seen_count  [N_PROD];
    for (int i = 0; i < N_PROD; i++) { last_task_id[i] = -1; seen_count[i] = 0; }
    json_arena_t *a = json_arena_create();
    char *p = body;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = '\0';
        json_node_t *n = json_parse(a, p, NULL, NULL);
        ASSERT(n);
        int widx = (int)json_number_value(json_object_get(n, "worker_idx"));
        int tid  = (int)json_number_value(json_object_get(n, "task_id"));
        ASSERT(widx >= 0 && widx < N_PROD);
        ASSERT(tid >= last_task_id[widx]);
        last_task_id[widx] = tid;
        seen_count[widx]++;
        p = nl + 1;
    }
    for (int i = 0; i < N_PROD; i++) ASSERT(seen_count[i] == PER);

    json_arena_destroy(a);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_ring_full_blocks_producer() — exercise has_space CV
 *
 * Fills the ring beyond capacity from a single producer with no
 * consumer drain pressure (the writer thread is slow because it's
 * doing fwrite per line). The producer must block on has_space
 * rather than dropping events, and every emitted event must
 * eventually land in the file.
 *
 * 10 000 events exceeds the 4096-slot ring capacity by ~2.4x, so
 * the producer will hit the full-ring path multiple times. */
static int test_ring_full_blocks_producer(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);
    ASSERT(q);

    const int N = 10000;
    for (int i = 0; i < N; i++) {
        event_queue_task_start(q, (double)i, i, 0);
    }
    event_queue_destroy(q);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    ASSERT(count_lines(body, len) == N);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_pending_count() */
static int test_pending_count(void)
{
    char path[256]; tmp_path(path, sizeof path);
    event_queue_t *q = event_queue_create(path);

    /* pending is best-effort; the writer might drain between emit
     * and our read. Just verify it's a small bounded value. */
    for (int i = 0; i < 10; i++) event_queue_task_start(q, 0.0, i, 0);
    int pending = event_queue_pending(q);
    ASSERT(pending >= 0);
    ASSERT(pending <= 10);

    event_queue_destroy(q);
    ASSERT(event_queue_pending(NULL) == 0);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    printf("014-event-queue-test:\n");
    RUN(single_producer);
    RUN(concurrent_burst);
    RUN(shutdown_drains_late_events);
    RUN(pending_count);
    RUN(high_contention_burst);
    RUN(ring_full_blocks_producer);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
