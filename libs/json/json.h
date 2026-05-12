/* libs/json/json.h — small DOM JSON parser + streaming writer.
 *
 * Written for SoraMech, not vendored. Designed in issue 314. Two
 * halves: a DOM parser with arena-allocated nodes (used by the
 * phase 3 graph loader, issue 305), and a streaming writer with a
 * fixed buffer (used by the JSONL run-log writer, issue 311).
 *
 * The parser is hand-rolled recursive descent. The DOM is read-only
 * once parsed; constructing JSON goes through the writer, not the
 * DOM. Numbers are doubles; integer-shaped fields read via
 * (int)json_number_value(...). UTF-8 strict, BMP \uXXXX with
 * surrogate-pair handling. No comments, no trailing commas.
 *
 * Errors: json_parse returns NULL on failure, with *err_offset
 * and *err_msg filled. The graph loader maps the offset to a
 * filename:line:column diagnostic before surfacing it.
 *
 * Memory: every parse tree lives in one json_arena_t. Destroying
 * the arena frees every node and every string in one call. There
 * is no per-node free.
 *
 * Threading: single-threaded — the parser is meant to run on the
 * graph-load thread before the pool spins up. Concurrency comes in
 * at the writer side, where each writer call is independent and the
 * caller owns the buffer.
 */

#ifndef SORAMECH_JSON_H
#define SORAMECH_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Types */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} json_kind_t;

typedef struct json_arena json_arena_t;
typedef struct json_node  json_node_t;
/* }}} */

/* {{{ Arena lifecycle */
/* Construct an empty arena. Returns NULL on allocation failure. */
json_arena_t *json_arena_create(void);

/* Free every chunk in the arena. Safe on NULL. */
void          json_arena_destroy(json_arena_t *a);

/* Bytes currently allocated across all chunks (for diagnostics). */
size_t        json_arena_bytes(const json_arena_t *a);
/* }}} */

/* {{{ Parser */
/* Parse a NUL-terminated UTF-8 source into a tree owned by the
 * arena. Returns NULL on syntax error; *err_offset (if non-NULL)
 * receives the byte offset where parsing failed, and *err_msg
 * (if non-NULL) receives a static-string description. */
json_node_t *json_parse(json_arena_t *a,
                        const char  *src,
                        int         *err_offset,
                        const char **err_msg);

/* Read a file end-to-end and parse it. *err_line (if non-NULL) is
 * the 1-based line number where parsing failed, computed from the
 * byte offset against the file contents. */
json_node_t *json_parse_file(json_arena_t *a,
                             const char  *path,
                             int         *err_line,
                             const char **err_msg);
/* }}} */

/* {{{ Accessors */
json_kind_t   json_kind        (const json_node_t *n);
int           json_is_null     (const json_node_t *n);
int           json_bool_value  (const json_node_t *n);
double        json_number_value(const json_node_t *n);

/* NUL-terminated; lives in the arena. */
const char   *json_string_value(const json_node_t *n);

int           json_array_size  (const json_node_t *n);
json_node_t  *json_array_at    (const json_node_t *n, int i);

int           json_object_size (const json_node_t *n);
const char   *json_object_key  (const json_node_t *n, int i);
json_node_t  *json_object_value(const json_node_t *n, int i);

/* Linear scan; returns NULL if key not present. */
json_node_t  *json_object_get  (const json_node_t *n, const char *key);
/* }}} */

/* {{{ Writer (declared; implementation deferred) */
typedef struct {
    char *buf;
    int   cap;
    int   used;
    int   err;        /* 0 ok; nonzero = truncation or bad call */
    /* Internal: depth stack (commas + bracket pairing). */
    unsigned char depth;
    unsigned char first[16];
} json_writer_t;

void json_writer_init   (json_writer_t *w, char *buf, int cap);
void json_writer_object (json_writer_t *w);
void json_writer_array  (json_writer_t *w);
void json_writer_end    (json_writer_t *w);
void json_writer_key    (json_writer_t *w, const char *key);
void json_writer_string (json_writer_t *w, const char *s);
void json_writer_int    (json_writer_t *w, long long v);
void json_writer_number (json_writer_t *w, double v);
void json_writer_bool   (json_writer_t *w, int v);
void json_writer_null   (json_writer_t *w);
/* Returns used bytes (excluding NUL), or -1 if w->err. */
int  json_writer_finish (json_writer_t *w);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_JSON_H */
