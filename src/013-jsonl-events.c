/* src/013-jsonl-events.c — phase 3 run-log event writer.
 *
 * Each emit serializes one event into a 512-byte buffer via the
 * bounded JSON writer (issue 314), appends a newline, then writes
 * the line to the underlying FILE under a per-writer mutex. The
 * mutex is what the MPSC ring buffer + dedicated writer thread
 * will replace once dispatch is producing events at rate.
 *
 * Buffer size is fixed at 512 bytes. Every event the architecture
 * doc defines fits comfortably; if a future event needs more it
 * can be bumped or made dynamic.
 *
 * Designed in issue 311.
 */

#include "013-jsonl-events.h"
#include "json.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVENT_BUF 512

/* {{{ Writer struct */
struct jsonl_writer {
    FILE            *fp;
    pthread_mutex_t  mtx;
};
/* }}} */

/* {{{ jsonl_writer_open() */
jsonl_writer_t *jsonl_writer_open(const char *path)
{
    if (!path) return NULL;
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    jsonl_writer_t *w = calloc(1, sizeof *w);
    if (!w) { fclose(fp); return NULL; }
    w->fp = fp;
    if (pthread_mutex_init(&w->mtx, NULL) != 0) {
        fclose(fp);
        free(w);
        return NULL;
    }
    return w;
}
/* }}} */

/* {{{ jsonl_writer_close() */
void jsonl_writer_close(jsonl_writer_t *w)
{
    if (!w) return;
    pthread_mutex_lock(&w->mtx);
    if (w->fp) { fflush(w->fp); fclose(w->fp); w->fp = NULL; }
    pthread_mutex_unlock(&w->mtx);
    pthread_mutex_destroy(&w->mtx);
    free(w);
}
/* }}} */

/* {{{ flush_line() — finalize the writer, append \n, write under lock */
static int flush_line(jsonl_writer_t *w, json_writer_t *jw, char *buf)
{
    int n = json_writer_finish(jw);
    if (n < 0) return -1;
    if (n >= EVENT_BUF) return -1;   /* would overrun the newline byte */
    buf[n] = '\n';
    int total = n + 1;

    pthread_mutex_lock(&w->mtx);
    int written = 0;
    if (w->fp) {
        written = (int)fwrite(buf, 1, (size_t)total, w->fp);
    }
    pthread_mutex_unlock(&w->mtx);
    return (written == total) ? 0 : -1;
}
/* }}} */

/* {{{ emit helpers — common event prologue */
static void begin_event(json_writer_t *jw, const char *type, double ts)
{
    json_writer_object(jw);
    json_writer_key(jw, "event"); json_writer_string(jw, type);
    json_writer_key(jw, "ts");    json_writer_number(jw, ts);
}
/* }}} */

/* {{{ jsonl_emit_run_start() */
int jsonl_emit_run_start(jsonl_writer_t *w, double ts,
                         const char *map, int n_workers)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "run_start", ts);
    json_writer_key(&jw, "map");        json_writer_string(&jw, map ? map : "");
    json_writer_key(&jw, "n_workers");  json_writer_int(&jw, n_workers);
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_task_submit() */
int jsonl_emit_task_submit(jsonl_writer_t *w, double ts,
                           int task_id, const char *box_id, int worker_idx)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "task_submit", ts);
    json_writer_key(&jw, "task_id");    json_writer_int(&jw, task_id);
    json_writer_key(&jw, "box_id");     json_writer_string(&jw, box_id ? box_id : "");
    json_writer_key(&jw, "worker_idx"); json_writer_int(&jw, worker_idx);
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_task_start() */
int jsonl_emit_task_start(jsonl_writer_t *w, double ts,
                          int task_id, int worker_idx)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "task_start", ts);
    json_writer_key(&jw, "task_id");    json_writer_int(&jw, task_id);
    json_writer_key(&jw, "worker_idx"); json_writer_int(&jw, worker_idx);
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_task_end() */
int jsonl_emit_task_end(jsonl_writer_t *w, double ts,
                        int task_id, int worker_idx,
                        long duration_us, int output_size)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "task_end", ts);
    json_writer_key(&jw, "task_id");      json_writer_int(&jw, task_id);
    json_writer_key(&jw, "worker_idx");   json_writer_int(&jw, worker_idx);
    json_writer_key(&jw, "duration_us");  json_writer_int(&jw, duration_us);
    json_writer_key(&jw, "output_size");  json_writer_int(&jw, output_size);
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_run_end() */
int jsonl_emit_run_end(jsonl_writer_t *w, double ts,
                       long duration_us, int n_tasks)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "run_end", ts);
    json_writer_key(&jw, "duration_us"); json_writer_int(&jw, duration_us);
    json_writer_key(&jw, "n_tasks");     json_writer_int(&jw, n_tasks);
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ truncate_for_log() — copy `size` bytes into `out` as a
 * NUL-terminated string, capping at 4 KB and stopping at any
 * embedded NUL. Returns the number of bytes actually copied
 * (excluding the trailing NUL). */
#define VERBOSE_MAX 4096
static int truncate_for_log(char *out, size_t cap,
                            const char *data, int size)
{
    if (cap == 0) return 0;
    int max = (size < (int)cap - 1) ? size : (int)cap - 1;
    if (max > VERBOSE_MAX) max = VERBOSE_MAX;
    int n = 0;
    for (; n < max; n++) {
        if (data[n] == '\0') break;
        out[n] = data[n];
    }
    out[n] = '\0';
    return n;
}
/* }}} */

/* {{{ jsonl_emit_task_input() */
int jsonl_emit_task_input(jsonl_writer_t *w, double ts,
                          int task_id, int port_index,
                          const char *data, int size)
{
    if (!w) return -1;
    /* Verbose events are bigger than the per-event line cap; use a
     * dedicated 8 KB buffer that fits a 4 KB data field plus
     * JSON-escape overhead. */
    char buf[8192];
    char trunc[VERBOSE_MAX + 1];
    int actual_size = truncate_for_log(trunc, sizeof trunc, data, size);

    json_writer_t jw;
    json_writer_init(&jw, buf, sizeof buf);
    begin_event(&jw, "task_input", ts);
    json_writer_key(&jw, "task_id"); json_writer_int(&jw, task_id);
    json_writer_key(&jw, "port");    json_writer_int(&jw, port_index);
    json_writer_key(&jw, "size");    json_writer_int(&jw, size);
    json_writer_key(&jw, "data");    json_writer_string(&jw, trunc);
    if (size > actual_size) {
        json_writer_key(&jw, "truncated"); json_writer_bool(&jw, 1);
    }
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_task_output() */
int jsonl_emit_task_output(jsonl_writer_t *w, double ts,
                           int task_id, const char *data, int size)
{
    if (!w) return -1;
    char buf[8192];
    char trunc[VERBOSE_MAX + 1];
    int actual_size = truncate_for_log(trunc, sizeof trunc, data, size);

    json_writer_t jw;
    json_writer_init(&jw, buf, sizeof buf);
    begin_event(&jw, "task_output", ts);
    json_writer_key(&jw, "task_id"); json_writer_int(&jw, task_id);
    json_writer_key(&jw, "size");    json_writer_int(&jw, size);
    json_writer_key(&jw, "data");    json_writer_string(&jw, trunc);
    if (size > actual_size) {
        json_writer_key(&jw, "truncated"); json_writer_bool(&jw, 1);
    }
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */

/* {{{ jsonl_emit_slot_alloc() */
int jsonl_emit_slot_alloc(jsonl_writer_t *w, double ts,
                          int slot_id, int cell_capacity, int n_cells,
                          const char *owner_box, const char *owner_port)
{
    if (!w) return -1;
    char buf[EVENT_BUF];
    json_writer_t jw;
    json_writer_init(&jw, buf, EVENT_BUF);
    begin_event(&jw, "slot_alloc", ts);
    json_writer_key(&jw, "slot_id");       json_writer_int(&jw, slot_id);
    json_writer_key(&jw, "cell_capacity"); json_writer_int(&jw, cell_capacity);
    json_writer_key(&jw, "n_cells");       json_writer_int(&jw, n_cells);
    if (owner_box) {
        json_writer_key(&jw, "box");       json_writer_string(&jw, owner_box);
    }
    if (owner_port) {
        json_writer_key(&jw, "port");      json_writer_string(&jw, owner_port);
    }
    json_writer_end(&jw);
    return flush_line(w, &jw, buf);
}
/* }}} */
