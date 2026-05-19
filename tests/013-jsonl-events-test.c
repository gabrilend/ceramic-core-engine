/* tests/013-jsonl-events-test.c — write a sequence of run-log
 * events, read the file back line by line, parse each line with
 * the JSON parser, and verify the event shapes.
 */

#include "013-jsonl-events.h"
#include "json.h"

#include <pthread.h>
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

/* {{{ tmp_path() — make a unique temp filename */
static void tmp_path(char *out, size_t cap)
{
    snprintf(out, cap, "/tmp/soramech-jsonl-%d-XXXXXX", (int)getpid());
    int fd = mkstemp(out);
    if (fd >= 0) close(fd);
}
/* }}} */

/* {{{ slurp_file() — load file contents into a new malloc'd buffer */
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
/* }}} */

/* {{{ test_empty_run() */
/* Open then close; file is empty / zero lines. */
static int test_empty_run(void)
{
    char path[256];
    tmp_path(path, sizeof path);
    jsonl_writer_t *w = jsonl_writer_open(path);
    ASSERT(w);
    jsonl_writer_close(w);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    ASSERT(len == 0);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_full_sequence() */
/* run_start → task_submit → task_start → task_end → run_end.
 * Parse each line, verify event names and key fields. */
static int test_full_sequence(void)
{
    char path[256];
    tmp_path(path, sizeof path);
    jsonl_writer_t *w = jsonl_writer_open(path);
    ASSERT(w);

    ASSERT(jsonl_emit_run_start  (w, 1.0, "maps/hello", 4) == 0);
    ASSERT(jsonl_emit_task_submit(w, 1.1, 42, "greet", -1) == 0);
    ASSERT(jsonl_emit_task_start (w, 1.2, 42, 2) == 0);
    ASSERT(jsonl_emit_task_end   (w, 1.3, 42, 2, 127, 12) == 0);
    ASSERT(jsonl_emit_run_end    (w, 1.4, 1850, 1) == 0);

    jsonl_writer_close(w);

    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    /* Count newlines: should be 5. */
    int newlines = 0;
    for (size_t i = 0; i < len; i++) if (body[i] == '\n') newlines++;
    ASSERT(newlines == 5);

    /* Parse the first line — run_start. */
    char *p = body;
    char *nl = strchr(p, '\n');
    *nl = '\0';
    json_arena_t *a = json_arena_create();
    json_node_t *n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "run_start") == 0);
    ASSERT(strcmp(json_string_value(json_object_get(n, "map")),   "maps/hello") == 0);
    ASSERT(json_number_value(json_object_get(n, "n_workers")) == 4.0);
    *nl = '\n';
    p = nl + 1;

    /* Skip task_submit. */
    nl = strchr(p, '\n'); *nl = '\0';
    n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "task_submit") == 0);
    ASSERT(json_number_value(json_object_get(n, "task_id")) == 42.0);
    ASSERT(strcmp(json_string_value(json_object_get(n, "box_id")), "greet") == 0);
    *nl = '\n'; p = nl + 1;

    /* task_start */
    nl = strchr(p, '\n'); *nl = '\0';
    n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "task_start") == 0);
    ASSERT(json_number_value(json_object_get(n, "worker_idx")) == 2.0);
    *nl = '\n'; p = nl + 1;

    /* task_end */
    nl = strchr(p, '\n'); *nl = '\0';
    n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "task_end") == 0);
    ASSERT(json_number_value(json_object_get(n, "duration_us")) == 127.0);
    ASSERT(json_number_value(json_object_get(n, "output_size")) == 12.0);
    *nl = '\n'; p = nl + 1;

    /* run_end */
    nl = strchr(p, '\n'); *nl = '\0';
    n = json_parse(a, p, NULL, NULL);
    ASSERT(n);
    ASSERT(strcmp(json_string_value(json_object_get(n, "event")), "run_end") == 0);
    ASSERT(json_number_value(json_object_get(n, "n_tasks")) == 1.0);

    json_arena_destroy(a);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ test_concurrent_emit() — multiple threads, single writer */
struct producer_args { jsonl_writer_t *w; int n; int worker_idx; };

static void *producer_thread(void *arg)
{
    struct producer_args *a = arg;
    for (int i = 0; i < a->n; i++) {
        jsonl_emit_task_start(a->w, (double)i, i, a->worker_idx);
    }
    return NULL;
}

static int test_concurrent_emit(void)
{
    char path[256];
    tmp_path(path, sizeof path);
    jsonl_writer_t *w = jsonl_writer_open(path);
    ASSERT(w);

    enum { N_THREADS = 8, PER = 200 };
    pthread_t threads[N_THREADS];
    struct producer_args args[N_THREADS];
    for (int t = 0; t < N_THREADS; t++) {
        args[t].w = w;
        args[t].n = PER;
        args[t].worker_idx = t;
        pthread_create(&threads[t], NULL, producer_thread, &args[t]);
    }
    for (int t = 0; t < N_THREADS; t++) pthread_join(threads[t], NULL);
    jsonl_writer_close(w);

    /* All N_THREADS * PER lines should be present, each a valid JSON
     * object on its own line. */
    size_t len = 0;
    char *body = slurp_file(path, &len);
    ASSERT(body);
    int newlines = 0;
    for (size_t i = 0; i < len; i++) if (body[i] == '\n') newlines++;
    ASSERT(newlines == N_THREADS * PER);

    /* Spot-check: every line parses. */
    json_arena_t *a = json_arena_create();
    char *p = body;
    int parsed = 0;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = '\0';
        json_node_t *n = json_parse(a, p, NULL, NULL);
        ASSERT(n);
        parsed++;
        p = nl + 1;
    }
    ASSERT(parsed == N_THREADS * PER);
    json_arena_destroy(a);
    free(body);
    unlink(path);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    printf("013-jsonl-events-test:\n");
    RUN(empty_run);
    RUN(full_sequence);
    RUN(concurrent_emit);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
