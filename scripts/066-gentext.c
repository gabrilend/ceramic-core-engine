/*
 * 066-gentext.c — the generator's support machinery, built.
 *
 * What this is: the implementation of the arena, the growable string,
 * the growable array, and the string helpers declared next door. It is
 * the least interesting code in the generator and the code everything
 * else depends on being right.
 *
 * How it does it, in general terms: the arena keeps a singly linked
 * list of blocks and bumps a pointer through the newest one, starting
 * a bigger block whenever a request will not fit. The buffer and the
 * vector double when full. Every allocation failure ends the program
 * immediately with a message, because a generator that cannot allocate
 * has no partial answer worth returning and every caller would
 * otherwise need a failure path to an exit that is already certain.
 */
#include "065-gentext.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static void out_of_memory() */
static void out_of_memory(const char *what)
{
    fprintf(stderr, "generator: out of memory allocating %s\n", what);
    exit(71);   /* the resource-exhaustion code (issue 106) */
}
/* }}} */

/* {{{ struct block / struct arena */
/*
 * Blocks start at this size and double, so a generator over a handful
 * of small files makes one allocation and a generator over a large
 * tree makes a few. The number is a guess and is meant to be one:
 * being wrong costs an extra malloc, which is not a cost.
 */
#define ARENA_FIRST_BLOCK 8192

typedef struct block {
    struct block *next;
    size_t        used;
    size_t        size;
    char          bytes[];
} block_t;

struct arena {
    block_t *head;
    size_t   next_size;
};
/* }}} */

/* {{{ arena_new() */
arena_t *arena_new(void)
{
    arena_t *a = calloc(1, sizeof *a);
    if (!a)
        out_of_memory("an arena");
    a->next_size = ARENA_FIRST_BLOCK;
    return a;
}
/* }}} */

/* {{{ arena_free() */
void arena_free(arena_t *a)
{
    if (!a)
        return;
    block_t *b = a->head;
    while (b) {
        block_t *next = b->next;
        free(b);
        b = next;
    }
    free(a);
}
/* }}} */

/* {{{ arena_alloc() */
void *arena_alloc(arena_t *a, size_t n)
{
    /* Round up so every hand-out is aligned for any type. A
     * misaligned struct pointer is undefined behaviour that usually
     * works, which is the worst kind. */
    size_t align = sizeof(void *) * 2;
    n = (n + align - 1) / align * align;
    if (n == 0)
        n = align;

    if (!a->head || a->head->size - a->head->used < n) {
        size_t size = a->next_size;
        while (size < n)
            size *= 2;
        block_t *b = calloc(1, sizeof *b + size);
        if (!b)
            out_of_memory("an arena block");
        b->size = size;
        b->next = a->head;
        a->head = b;
        a->next_size = size * 2;
    }

    void *p = a->head->bytes + a->head->used;
    a->head->used += n;
    return p;
}
/* }}} */

/* {{{ arena_strndup() */
char *arena_strndup(arena_t *a, const char *s, size_t n)
{
    char *p = arena_alloc(a, n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
/* }}} */

/* {{{ arena_strdup() */
char *arena_strdup(arena_t *a, const char *s)
{
    return arena_strndup(a, s, strlen(s));
}
/* }}} */

/* {{{ buf_init() / buf_free() */
void buf_init(buf_t *b)
{
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

void buf_free(buf_t *b)
{
    free(b->data);
    buf_init(b);
}
/* }}} */

/* {{{ static void buf_reserve() */
static void buf_reserve(buf_t *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 1024;
    while (cap < b->len + extra + 1)
        cap *= 2;
    char *data = realloc(b->data, cap);
    if (!data)
        out_of_memory("an output buffer");
    b->data = data;
    b->cap  = cap;
}
/* }}} */

/* {{{ buf_add() / buf_addstr() / buf_addch() */
void buf_add(buf_t *b, const char *s, size_t n)
{
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void buf_addstr(buf_t *b, const char *s)
{
    buf_add(b, s, strlen(s));
}

void buf_addch(buf_t *b, char c)
{
    buf_add(b, &c, 1);
}
/* }}} */

/* {{{ static void buf_vaddf() */
/*
 * Formats into the buffer, asking vsnprintf how much room it needs
 * rather than guessing. Two passes over the format string is the
 * price of never truncating a generated line, and a truncated line of
 * C is a compile error somebody else has to diagnose.
 */
static void buf_vaddf(buf_t *b, const char *fmt, va_list ap)
{
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        fprintf(stderr, "generator: cannot format output\n");
        exit(70);
    }
    buf_reserve(b, (size_t)n);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    b->len += (size_t)n;
}
/* }}} */

/* {{{ buf_addf() / buf_line() */
void buf_addf(buf_t *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    buf_vaddf(b, fmt, ap);
    va_end(ap);
}

void buf_line(buf_t *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    buf_vaddf(b, fmt, ap);
    va_end(ap);
    buf_addch(b, '\n');
}
/* }}} */

/* {{{ vec_init() / vec_free() */
void vec_init(vec_t *v, size_t elem)
{
    v->data = NULL;
    v->n    = 0;
    v->cap  = 0;
    v->elem = elem;
}

void vec_free(vec_t *v)
{
    free(v->data);
    v->data = NULL;
    v->n = v->cap = 0;
}
/* }}} */

/* {{{ vec_push() */
void *vec_push(vec_t *v)
{
    if (v->n == v->cap) {
        int cap = v->cap ? v->cap * 2 : 8;
        void *data = realloc(v->data, (size_t)cap * v->elem);
        if (!data)
            out_of_memory("a growing list");
        v->data = data;
        v->cap  = cap;
    }
    void *slot = (char *)v->data + (size_t)v->n * v->elem;
    memset(slot, 0, v->elem);
    v->n++;
    return slot;
}
/* }}} */

/* {{{ vec_at() */
void *vec_at(const vec_t *v, int i)
{
    return (char *)v->data + (size_t)i * v->elem;
}
/* }}} */

/* {{{ gt_is_ident() */
int gt_is_ident(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}
/* }}} */

/* {{{ static int is_space() */
static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\v' || c == '\f';
}
/* }}} */

/* {{{ gt_all_space() */
int gt_all_space(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!is_space((unsigned char)s[i]))
            return 0;
    return 1;
}
/* }}} */

/* {{{ gt_trim() */
char *gt_trim(arena_t *a, const char *s, size_t n)
{
    size_t start = 0;
    while (start < n && is_space((unsigned char)s[start]))
        start++;
    size_t end = n;
    while (end > start && is_space((unsigned char)s[end - 1]))
        end--;
    return arena_strndup(a, s + start, end - start);
}
/* }}} */

/* {{{ gt_normalize_type() */
/*
 * Two passes, because the second depends on the first being done.
 * First collapse every run of whitespace to a single space and trim
 * the ends; then rewrite every star so it is preceded by exactly one
 * space and followed by none. "const char*", "const  char *", and
 * "const char  *  " all land on "const char *".
 */
char *gt_normalize_type(arena_t *a, const char *s, size_t n)
{
    char *flat = arena_alloc(a, n + 2);
    size_t out = 0;
    size_t i = 0;

    while (i < n && is_space((unsigned char)s[i]))
        i++;
    while (i < n) {
        if (is_space((unsigned char)s[i])) {
            while (i < n && is_space((unsigned char)s[i]))
                i++;
            if (i < n)
                flat[out++] = ' ';
        } else {
            flat[out++] = s[i++];
        }
    }
    flat[out] = '\0';

    /* Now the stars. Worst case every character becomes two. */
    char *fixed = arena_alloc(a, out * 2 + 2);
    size_t o = 0;
    for (size_t j = 0; j < out; j++) {
        if (flat[j] == '*') {
            /* Drop a space we just wrote, then write our own, so a
             * run like "* *" comes out as " * *". */
            while (o > 0 && fixed[o - 1] == ' ')
                o--;
            if (o > 0)
                fixed[o++] = ' ';
            fixed[o++] = '*';
            /* Skip whitespace after the star. */
            while (j + 1 < out && flat[j + 1] == ' ')
                j++;
        } else {
            fixed[o++] = flat[j];
        }
    }
    fixed[o] = '\0';
    return fixed;
}
/* }}} */

/* {{{ gt_mangle() */
char *gt_mangle(arena_t *a, const char *type_name)
{
    size_t n = strlen(type_name);
    /* "ptr" is three characters where '*' was one. */
    char *out = arena_alloc(a, n * 3 + 2);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (type_name[i] == '*') {
            out[o++] = 'p';
            out[o++] = 't';
            out[o++] = 'r';
        } else if (is_space((unsigned char)type_name[i])) {
            out[o++] = '_';
        } else {
            out[o++] = type_name[i];
        }
    }
    out[o] = '\0';
    return out;
}
/* }}} */

/* {{{ gt_line_of() */
int gt_line_of(const char *text, size_t pos)
{
    int line = 1;
    for (size_t i = 0; i < pos; i++)
        if (text[i] == '\n')
            line++;
    return line;
}
/* }}} */
