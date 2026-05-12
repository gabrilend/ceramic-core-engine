/* libs/json/json.c — DOM JSON parser + streaming writer.
 *
 * Sections, top to bottom:
 *
 *   - Arena allocator (chunked bump).
 *   - Internal node types and tagged-union layout.
 *   - Parser (hand-rolled recursive descent).
 *   - Accessors.
 *   - File loader (json_parse_file).
 *   - Writer (declared; body deferred per the implementation log on
 *     issue 314 — JSONL writer lands when 311 needs it).
 *
 * Designed in issue 314. The parser supports JSON per RFC 8259 with
 * a few tightenings: no comments, no trailing commas, no embedded
 * NULs in strings, BMP-only \uXXXX with surrogate-pair handling.
 * Numbers are doubles; integer-shaped fields are read via
 * (int)json_number_value.
 *
 * Memory model: every node and every string lives in the arena
 * passed to json_parse. json_arena_destroy frees the lot in one
 * call. There is no per-node free. During parsing, temporary
 * linked-list scaffolding for array/object construction is also
 * allocated in the arena and left "abandoned" once the final
 * fixed-size arrays are emitted — abandoned bytes are reclaimed
 * when the arena is destroyed.
 */

#include "json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Arena allocator */
typedef struct json_chunk {
    struct json_chunk *next;
    size_t size;
    size_t used;
    /* Followed by `size` bytes of storage. */
    char data[];
} json_chunk_t;

struct json_arena {
    json_chunk_t *head;
    size_t total_bytes;
};

#define JSON_CHUNK_DEFAULT 16384

/* {{{ arena_new_chunk() */
static json_chunk_t *arena_new_chunk(size_t min_size)
{
    size_t sz = (min_size > JSON_CHUNK_DEFAULT) ? min_size : JSON_CHUNK_DEFAULT;
    json_chunk_t *c = malloc(sizeof(json_chunk_t) + sz);
    if (!c) return NULL;
    c->next = NULL;
    c->size = sz;
    c->used = 0;
    return c;
}
/* }}} */

/* {{{ json_arena_create() */
json_arena_t *json_arena_create(void)
{
    json_arena_t *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->head = arena_new_chunk(JSON_CHUNK_DEFAULT);
    if (!a->head) { free(a); return NULL; }
    a->total_bytes = a->head->size;
    return a;
}
/* }}} */

/* {{{ json_arena_destroy() */
void json_arena_destroy(json_arena_t *a)
{
    if (!a) return;
    json_chunk_t *c = a->head;
    while (c) {
        json_chunk_t *next = c->next;
        free(c);
        c = next;
    }
    free(a);
}
/* }}} */

/* {{{ json_arena_bytes() */
size_t json_arena_bytes(const json_arena_t *a)
{
    return a ? a->total_bytes : 0;
}
/* }}} */

/* {{{ arena_alloc() — aligned to 8 bytes */
static void *arena_alloc(json_arena_t *a, size_t bytes)
{
    if (bytes == 0) bytes = 1;
    /* 8-byte alignment for everything; the union contains a double
     * and pointers, both of which want at least 8-byte alignment on
     * 64-bit targets. */
    size_t aligned = (bytes + 7u) & ~(size_t)7u;

    if (a->head->used + aligned > a->head->size) {
        json_chunk_t *c = arena_new_chunk(aligned);
        if (!c) return NULL;
        c->next = a->head;
        a->head = c;
        a->total_bytes += c->size;
    }
    void *p = a->head->data + a->head->used;
    a->head->used += aligned;
    return p;
}
/* }}} */

/* }}} */

/* {{{ Node layout */
typedef struct kv_link {
    struct kv_link *next;
    const char     *key;     /* arena-allocated; NULL for array items */
    json_node_t    *value;
} kv_link_t;

struct json_node {
    json_kind_t kind;
    union {
        int           b;
        double        n;
        const char   *s;
        struct {
            json_node_t **items;
            int           count;
        } arr;
        struct {
            const char  **keys;
            json_node_t **values;
            int           count;
        } obj;
    } u;
};
/* }}} */

/* {{{ Parser state */
typedef struct {
    const char  *src;
    int          pos;     /* current byte offset into src */
    json_arena_t *arena;
    const char  *err_msg; /* set on first failure */
    int          err_pos;
} parser_t;

static json_node_t *parse_value(parser_t *p);
static int          parse_string_into(parser_t *p, const char **out);
/* }}} */

/* {{{ Local helpers — error reporting & character classes */
static void fail(parser_t *p, const char *msg)
{
    if (!p->err_msg) {
        p->err_msg = msg;
        p->err_pos = p->pos;
    }
}

static int is_ws(int c)   { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int is_digit(int c){ return c >= '0' && c <= '9'; }
static int is_hex(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static int peek(parser_t *p)
{
    return (unsigned char)p->src[p->pos];
}

static void skip_ws(parser_t *p)
{
    while (is_ws((unsigned char)p->src[p->pos])) p->pos++;
}
/* }}} */

/* {{{ Local helpers — node makers */
static json_node_t *mk_node(parser_t *p, json_kind_t k)
{
    json_node_t *n = arena_alloc(p->arena, sizeof *n);
    if (!n) { fail(p, "out of memory"); return NULL; }
    memset(n, 0, sizeof *n);
    n->kind = k;
    return n;
}
/* }}} */

/* {{{ parse_string_into() — read "...", decode escapes into arena */
/* Reads from the opening '"' at p->pos. On success advances p past
 * the closing '"' and sets *out to an arena-owned NUL-terminated
 * UTF-8 string. */
static int parse_string_into(parser_t *p, const char **out)
{
    if (peek(p) != '"') { fail(p, "expected string"); return 0; }
    p->pos++;

    /* First pass: scan to closing quote, count decoded bytes. */
    int start = p->pos;
    int decoded_len = 0;
    while (1) {
        int c = peek(p);
        if (c == 0) { fail(p, "unterminated string"); return 0; }
        if (c == '"') break;
        if (c == '\\') {
            p->pos++;
            int e = peek(p);
            switch (e) {
                case '"': case '\\': case '/':
                case 'b': case 'f': case 'n': case 'r': case 't':
                    decoded_len += 1; p->pos++; break;
                case 'u': {
                    /* 4 hex digits → 1..4 UTF-8 bytes; surrogate pairs
                     * combine to 4 bytes. We compute conservatively
                     * during the count pass. */
                    p->pos++;
                    for (int i = 0; i < 4; i++) {
                        if (!is_hex(peek(p))) { fail(p, "bad \\u escape"); return 0; }
                        p->pos++;
                    }
                    /* Possible surrogate pair? If high surrogate, consume
                     * the second \uXXXX too. */
                    /* We can't know the codepoint without decoding, so
                     * we add the worst case (4) and refine in pass 2. */
                    decoded_len += 4;
                    break;
                }
                default: fail(p, "invalid escape sequence"); return 0;
            }
        } else if ((unsigned)c < 0x20) {
            fail(p, "unescaped control character in string");
            return 0;
        } else {
            decoded_len++;
            p->pos++;
        }
    }

    /* Second pass: actually decode. */
    int end = p->pos;
    char *buf = arena_alloc(p->arena, (size_t)decoded_len + 1);
    if (!buf) { fail(p, "out of memory"); return 0; }
    int o = 0;
    p->pos = start;
    while (p->pos < end) {
        int c = peek(p);
        if (c == '\\') {
            p->pos++;
            int e = peek(p);
            p->pos++;
            switch (e) {
                case '"':  buf[o++] = '"';  break;
                case '\\': buf[o++] = '\\'; break;
                case '/':  buf[o++] = '/';  break;
                case 'b':  buf[o++] = '\b'; break;
                case 'f':  buf[o++] = '\f'; break;
                case 'n':  buf[o++] = '\n'; break;
                case 'r':  buf[o++] = '\r'; break;
                case 't':  buf[o++] = '\t'; break;
                case 'u': {
                    int cp = 0;
                    for (int i = 0; i < 4; i++) {
                        cp = (cp << 4) | hex_val(peek(p));
                        p->pos++;
                    }
                    /* High surrogate? */
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (p->src[p->pos] != '\\' || p->src[p->pos + 1] != 'u') {
                            fail(p, "lone high surrogate");
                            return 0;
                        }
                        p->pos += 2;
                        int cp2 = 0;
                        for (int i = 0; i < 4; i++) {
                            if (!is_hex(peek(p))) { fail(p, "bad \\u escape"); return 0; }
                            cp2 = (cp2 << 4) | hex_val(peek(p));
                            p->pos++;
                        }
                        if (cp2 < 0xDC00 || cp2 > 0xDFFF) {
                            fail(p, "expected low surrogate");
                            return 0;
                        }
                        cp = 0x10000 + (((cp - 0xD800) << 10) | (cp2 - 0xDC00));
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail(p, "lone low surrogate");
                        return 0;
                    }
                    if (cp == 0) {
                        fail(p, "embedded NUL not allowed");
                        return 0;
                    }
                    /* Encode UTF-8. */
                    if (cp < 0x80) {
                        buf[o++] = (char)cp;
                    } else if (cp < 0x800) {
                        buf[o++] = (char)(0xC0 | (cp >> 6));
                        buf[o++] = (char)(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        buf[o++] = (char)(0xE0 | (cp >> 12));
                        buf[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[o++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[o++] = (char)(0xF0 | (cp >> 18));
                        buf[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                        buf[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: /* impossible — pass 1 caught it */ break;
            }
        } else {
            buf[o++] = (char)c;
            p->pos++;
        }
    }
    buf[o] = '\0';
    p->pos++;  /* past the closing '"' */
    *out = buf;
    return 1;
}
/* }}} */

/* {{{ parse_number() */
static json_node_t *parse_number(parser_t *p)
{
    int start = p->pos;
    if (peek(p) == '-') p->pos++;
    if (!is_digit(peek(p))) { fail(p, "invalid number"); return NULL; }
    if (peek(p) == '0') {
        p->pos++;
    } else {
        while (is_digit(peek(p))) p->pos++;
    }
    if (peek(p) == '.') {
        p->pos++;
        if (!is_digit(peek(p))) { fail(p, "expected digit after '.'"); return NULL; }
        while (is_digit(peek(p))) p->pos++;
    }
    if (peek(p) == 'e' || peek(p) == 'E') {
        p->pos++;
        if (peek(p) == '+' || peek(p) == '-') p->pos++;
        if (!is_digit(peek(p))) { fail(p, "expected digit in exponent"); return NULL; }
        while (is_digit(peek(p))) p->pos++;
    }

    int len = p->pos - start;
    /* Copy into a scratch NUL-terminated buffer for strtod. 64 bytes
     * covers any double-representable literal. Longer literals get a
     * heap fallback. */
    char small[64];
    char *buf = small;
    char *heap = NULL;
    if (len >= (int)sizeof small) {
        heap = malloc((size_t)len + 1);
        if (!heap) { fail(p, "out of memory"); return NULL; }
        buf = heap;
    }
    memcpy(buf, p->src + start, (size_t)len);
    buf[len] = '\0';

    char *endp;
    double v = strtod(buf, &endp);
    if (endp != buf + len) {
        if (heap) free(heap);
        fail(p, "invalid number");
        return NULL;
    }
    if (heap) free(heap);

    json_node_t *n = mk_node(p, JSON_NUMBER);
    if (!n) return NULL;
    n->u.n = v;
    return n;
}
/* }}} */

/* {{{ parse_literal() — true / false / null */
static json_node_t *parse_literal(parser_t *p)
{
    const char *s = p->src + p->pos;
    if (strncmp(s, "true", 4) == 0) {
        p->pos += 4;
        json_node_t *n = mk_node(p, JSON_BOOL);
        if (n) n->u.b = 1;
        return n;
    }
    if (strncmp(s, "false", 5) == 0) {
        p->pos += 5;
        json_node_t *n = mk_node(p, JSON_BOOL);
        if (n) n->u.b = 0;
        return n;
    }
    if (strncmp(s, "null", 4) == 0) {
        p->pos += 4;
        return mk_node(p, JSON_NULL);
    }
    fail(p, "unexpected token");
    return NULL;
}
/* }}} */

/* {{{ parse_array() */
static json_node_t *parse_array(parser_t *p)
{
    if (peek(p) != '[') { fail(p, "expected '['"); return NULL; }
    p->pos++;
    skip_ws(p);

    kv_link_t *head = NULL, *tail = NULL;
    int count = 0;

    if (peek(p) == ']') {
        p->pos++;
        json_node_t *n = mk_node(p, JSON_ARRAY);
        if (n) { n->u.arr.items = NULL; n->u.arr.count = 0; }
        return n;
    }

    while (1) {
        json_node_t *v = parse_value(p);
        if (!v) return NULL;
        kv_link_t *link = arena_alloc(p->arena, sizeof *link);
        if (!link) { fail(p, "out of memory"); return NULL; }
        link->next = NULL;
        link->key  = NULL;
        link->value = v;
        if (!head) head = link;
        else       tail->next = link;
        tail = link;
        count++;

        skip_ws(p);
        if (peek(p) == ',') {
            p->pos++;
            skip_ws(p);
            if (peek(p) == ']') { fail(p, "trailing comma"); return NULL; }
        } else if (peek(p) == ']') {
            p->pos++;
            break;
        } else {
            fail(p, "expected ',' or ']'");
            return NULL;
        }
    }

    json_node_t *n = mk_node(p, JSON_ARRAY);
    if (!n) return NULL;
    n->u.arr.count = count;
    n->u.arr.items = arena_alloc(p->arena, (size_t)count * sizeof(json_node_t *));
    if (!n->u.arr.items) { fail(p, "out of memory"); return NULL; }
    int i = 0;
    for (kv_link_t *l = head; l; l = l->next) n->u.arr.items[i++] = l->value;
    return n;
}
/* }}} */

/* {{{ parse_object() */
static json_node_t *parse_object(parser_t *p)
{
    if (peek(p) != '{') { fail(p, "expected '{'"); return NULL; }
    p->pos++;
    skip_ws(p);

    kv_link_t *head = NULL, *tail = NULL;
    int count = 0;

    if (peek(p) == '}') {
        p->pos++;
        json_node_t *n = mk_node(p, JSON_OBJECT);
        if (n) { n->u.obj.keys = NULL; n->u.obj.values = NULL; n->u.obj.count = 0; }
        return n;
    }

    while (1) {
        skip_ws(p);
        const char *key;
        if (!parse_string_into(p, &key)) return NULL;
        skip_ws(p);
        if (peek(p) != ':') { fail(p, "expected ':' after key"); return NULL; }
        p->pos++;
        skip_ws(p);
        json_node_t *v = parse_value(p);
        if (!v) return NULL;

        kv_link_t *link = arena_alloc(p->arena, sizeof *link);
        if (!link) { fail(p, "out of memory"); return NULL; }
        link->next  = NULL;
        link->key   = key;
        link->value = v;
        if (!head) head = link;
        else       tail->next = link;
        tail = link;
        count++;

        skip_ws(p);
        if (peek(p) == ',') {
            p->pos++;
            skip_ws(p);
            if (peek(p) == '}') { fail(p, "trailing comma"); return NULL; }
        } else if (peek(p) == '}') {
            p->pos++;
            break;
        } else {
            fail(p, "expected ',' or '}'");
            return NULL;
        }
    }

    json_node_t *n = mk_node(p, JSON_OBJECT);
    if (!n) return NULL;
    n->u.obj.count  = count;
    n->u.obj.keys   = arena_alloc(p->arena, (size_t)count * sizeof(const char *));
    n->u.obj.values = arena_alloc(p->arena, (size_t)count * sizeof(json_node_t *));
    if (!n->u.obj.keys || !n->u.obj.values) {
        fail(p, "out of memory");
        return NULL;
    }
    int i = 0;
    for (kv_link_t *l = head; l; l = l->next) {
        n->u.obj.keys[i]   = l->key;
        n->u.obj.values[i] = l->value;
        i++;
    }
    return n;
}
/* }}} */

/* {{{ parse_value() — dispatch on first non-space char */
static json_node_t *parse_value(parser_t *p)
{
    skip_ws(p);
    int c = peek(p);
    if (c == '{')                                 return parse_object(p);
    if (c == '[')                                 return parse_array(p);
    if (c == '"') {
        json_node_t *n = mk_node(p, JSON_STRING);
        if (!n) return NULL;
        if (!parse_string_into(p, &n->u.s)) return NULL;
        return n;
    }
    if (c == '-' || is_digit(c))                  return parse_number(p);
    if (c == 't' || c == 'f' || c == 'n')         return parse_literal(p);
    fail(p, "unexpected character");
    return NULL;
}
/* }}} */

/* {{{ json_parse() */
json_node_t *json_parse(json_arena_t *a, const char *src,
                        int *err_offset, const char **err_msg)
{
    parser_t p = { src, 0, a, NULL, 0 };
    if (!src || !a) {
        if (err_msg)    *err_msg    = "null arena or source";
        if (err_offset) *err_offset = 0;
        return NULL;
    }
    json_node_t *root = parse_value(&p);
    if (!root) {
        if (err_msg)    *err_msg    = p.err_msg ? p.err_msg : "parse error";
        if (err_offset) *err_offset = p.err_pos;
        return NULL;
    }
    skip_ws(&p);
    if (p.src[p.pos] != '\0') {
        if (err_msg)    *err_msg    = "trailing garbage after JSON value";
        if (err_offset) *err_offset = p.pos;
        return NULL;
    }
    return root;
}
/* }}} */

/* {{{ json_parse_file() */
json_node_t *json_parse_file(json_arena_t *a, const char *path,
                             int *err_line, const char **err_msg)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (err_msg)  *err_msg  = "cannot open file";
        if (err_line) *err_line = 0;
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) {
        fclose(fp);
        if (err_msg) *err_msg = "cannot size file";
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        if (err_msg) *err_msg = "out of memory";
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[got] = '\0';

    int off = 0;
    const char *m = NULL;
    json_node_t *n = json_parse(a, buf, &off, &m);
    if (!n) {
        if (err_msg) *err_msg = m;
        if (err_line) {
            int line = 1;
            for (int i = 0; i < off && i < (int)got; i++) {
                if (buf[i] == '\n') line++;
            }
            *err_line = line;
        }
    }
    free(buf);
    return n;
}
/* }}} */

/* {{{ Accessors */
json_kind_t json_kind(const json_node_t *n)         { return n ? n->kind : JSON_NULL; }
int         json_is_null(const json_node_t *n)      { return n && n->kind == JSON_NULL; }
int         json_bool_value(const json_node_t *n)   { return (n && n->kind == JSON_BOOL)   ? n->u.b : 0; }
double      json_number_value(const json_node_t *n) { return (n && n->kind == JSON_NUMBER) ? n->u.n : 0.0; }
const char *json_string_value(const json_node_t *n) { return (n && n->kind == JSON_STRING) ? n->u.s : NULL; }
int         json_array_size(const json_node_t *n)   { return (n && n->kind == JSON_ARRAY)  ? n->u.arr.count : 0; }
int         json_object_size(const json_node_t *n)  { return (n && n->kind == JSON_OBJECT) ? n->u.obj.count : 0; }

json_node_t *json_array_at(const json_node_t *n, int i)
{
    if (!n || n->kind != JSON_ARRAY) return NULL;
    if (i < 0 || i >= n->u.arr.count) return NULL;
    return n->u.arr.items[i];
}

const char *json_object_key(const json_node_t *n, int i)
{
    if (!n || n->kind != JSON_OBJECT) return NULL;
    if (i < 0 || i >= n->u.obj.count) return NULL;
    return n->u.obj.keys[i];
}

json_node_t *json_object_value(const json_node_t *n, int i)
{
    if (!n || n->kind != JSON_OBJECT) return NULL;
    if (i < 0 || i >= n->u.obj.count) return NULL;
    return n->u.obj.values[i];
}

json_node_t *json_object_get(const json_node_t *n, const char *key)
{
    if (!n || n->kind != JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < n->u.obj.count; i++) {
        if (strcmp(n->u.obj.keys[i], key) == 0) return n->u.obj.values[i];
    }
    return NULL;
}
/* }}} */

/* {{{ Writer — declared, body deferred */
/* The writer half is exercised once the JSONL run-log writer
 * (issue 311) lands. The signatures are declared in json.h so other
 * components can link against this object even before the writer
 * body is implemented. For now each entrypoint sets w->err and
 * returns; json_writer_finish returns -1 if the writer has been
 * touched, 0 if untouched. */

/* {{{ json_writer_init() */
void json_writer_init(json_writer_t *w, char *buf, int cap)
{
    if (!w) return;
    w->buf   = buf;
    w->cap   = cap;
    w->used  = 0;
    w->err   = 0;
    w->depth = 0;
    memset(w->first, 0, sizeof w->first);
}
/* }}} */

/* {{{ json_writer_object() */
void json_writer_object(json_writer_t *w) { (void)w; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_array() */
void json_writer_array (json_writer_t *w) { (void)w; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_end() */
void json_writer_end   (json_writer_t *w) { (void)w; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_key() */
void json_writer_key   (json_writer_t *w, const char *k) { (void)k; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_string() */
void json_writer_string(json_writer_t *w, const char *s) { (void)s; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_int() */
void json_writer_int   (json_writer_t *w, long long v)   { (void)v; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_number() */
void json_writer_number(json_writer_t *w, double v)      { (void)v; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_bool() */
void json_writer_bool  (json_writer_t *w, int v)         { (void)v; if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_null() */
void json_writer_null  (json_writer_t *w)                { if (w) w->err = -1; }
/* }}} */
/* {{{ json_writer_finish() */
int json_writer_finish(json_writer_t *w)
{
    if (!w) return -1;
    return w->err ? -1 : w->used;
}
/* }}} */
/* }}} */
