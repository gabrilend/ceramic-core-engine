/*
 * 065-gentext.h — the generator's support machinery, declared.
 *
 * What this is: the four small things a C program needs before it can
 * do the work a scripting language does for free — a place to put
 * allocations nobody wants to free individually, a string that grows,
 * an array that grows, and the handful of string operations the
 * parser leans on. Nothing here knows anything about C source, boxes,
 * or registries; it is the floor the generator stands on.
 *
 * How it does it, in general terms: every allocation the generator
 * makes lives in one arena and dies with it, so no parse path has to
 * remember to clean up on the way to an error exit — and the generator
 * exits on the first error by design (issue 301), which makes
 * individual frees pure ceremony. The buffer and the vector both grow
 * by doubling and never shrink, which is right for a program whose
 * whole life is one pass over some files.
 *
 * Why this is its own unit (issue 308): everything above it assumes it
 * is correct, and a bug here surfaces as garbled C hundreds of lines
 * away in a file nobody reads. It gets its own test for that reason.
 */
#ifndef SORA_GENTEXT_H
#define SORA_GENTEXT_H

#include <stddef.h>

/* {{{ arena */
/*
 * A bump allocator with a list of blocks. Hands out zeroed memory,
 * never reuses it, and frees everything at once. The generator's
 * lifetime is one pass, so this is the whole memory strategy.
 */
typedef struct arena arena_t;

arena_t *arena_new(void);
void     arena_free(arena_t *a);

/* Zeroed, and aligned for any type. Dies rather than returning null:
 * a generator that cannot allocate cannot do its job, and there is no
 * partial answer worth returning. */
void    *arena_alloc(arena_t *a, size_t n);
char    *arena_strndup(arena_t *a, const char *s, size_t n);
char    *arena_strdup(arena_t *a, const char *s);
/* }}} */

/* {{{ buf_t — a string that grows */
typedef struct buf {
    char  *data;
    size_t len;
    size_t cap;
} buf_t;

void buf_init(buf_t *b);
void buf_free(buf_t *b);
void buf_add(buf_t *b, const char *s, size_t n);
void buf_addstr(buf_t *b, const char *s);
void buf_addch(buf_t *b, char c);
/* Appends a formatted string and then a newline, because every caller
 * in the emitter writes whole lines and appending "\n" by hand at
 * ninety call sites is ninety chances to forget. */
void buf_line(buf_t *b, const char *fmt, ...);
/* Same without the newline, for the few places that build a line in
 * pieces. */
void buf_addf(buf_t *b, const char *fmt, ...);
/* }}} */

/* {{{ vec_t — an array that grows */
/*
 * Elements are stored by value, contiguously, so the caller indexes
 * them like a plain array. vec_push returns a pointer to a zeroed new
 * element — which is invalidated by the next push, so a caller fills
 * it in immediately and re-reads it by index afterwards. That is a
 * sharp edge and it is deliberate: the alternative is an array of
 * pointers, and the whole reason the generator's output is laid out
 * contiguously is so the emitter can walk it in order.
 */
typedef struct vec {
    void  *data;
    int    n;
    int    cap;
    size_t elem;
} vec_t;

void  vec_init(vec_t *v, size_t elem);
void  vec_free(vec_t *v);
void *vec_push(vec_t *v);
void *vec_at(const vec_t *v, int i);
/* }}} */

/* {{{ string helpers */
/*
 * Collapse runs of whitespace to one space, settle pointer spelling
 * so "const char*" and "const  char  *" become the same text, and
 * trim both ends. The emitted registry compares types by these
 * strings, so the spelling has to be canonical or two spellings of
 * one type become two types.
 */
char *gt_normalize_type(arena_t *a, const char *s, size_t n);

/* A type name as an identifier fragment: '*' becomes "ptr",
 * whitespace becomes '_'. Used for generated symbol names, which must
 * be C identifiers and must not collide. */
char *gt_mangle(arena_t *a, const char *type_name);

/* {{{ gt_box_symbol() — issue 311a */
/*
 * The C identifier for one box, built from **the file it lives in and
 * the function's name**, so that two boxes called `read` in two files
 * are two symbols rather than a duplicate-definition error naming
 * neither of them.
 *
 * The path is the one the caller has already canonicalised. Both must
 * arrive in the same spelling every time, because a map that names a
 * box briefly and one that names it by path have to produce the *same*
 * symbol — otherwise one function acquires two definitions and the
 * problem the address was meant to solve comes back wearing a hat.
 *
 * **Punctuation is transcribed into words, and the escape character
 * escapes itself**, which is what makes the scheme injective rather
 * than merely tidier:
 *
 * | in a name | in a symbol |     | why |
 * |---|---|---|
 * | `.`  | `_dot_` |  a filename's extension |
 * | `/`  | `_sl_`  |  the path, which is what settles two files sharing a basename |
 * | `-`  | `_dsh_` |  this project's own sources are named like `029-demo-boxes.c` |
 * | `_`  | `_und_` |  the escape character, escaping itself |
 * | `:`  | `__`    |  the separator between the file and the function |
 * | else | `_xNN_` |  hex, so no filename can defeat this |
 *
 * Without the `_und_` row the scheme would move the collision rather
 * than remove it: `math.c` and `math_c` would both become `math_c`.
 * It is the same reason percent-encoding has to write `%` as `%25`.
 *
 * Every symbol carries a fixed prefix, which does two jobs for one
 * decision: it keeps generated names out of the way of anything a box
 * author writes, and it means a file whose name **begins with a
 * digit** — which every source in this project does — still produces a
 * legal identifier.
 *
 * **Uniqueness is the job; recovery is not.** Nothing decodes a symbol
 * back into a name. A name a person reads comes from the string
 * literal a placement function writes onto its station, because these
 * are static functions whose symbols may not survive a stripped binary
 * at all. The scheme is reversible because being reversible is how a
 * scheme is proved injective, not because anything reverses it.
 */
char *gt_box_symbol(arena_t *a, const char *file, const char *function);
/* }}} */

/* Which line a byte offset falls on, counting from 1. Linear, and
 * called only when something is being reported or recorded, never in
 * a loop over the whole file. */
int gt_line_of(const char *text, size_t pos);

/* Trim leading and trailing whitespace, returning a pointer into the
 * arena. */
char *gt_trim(arena_t *a, const char *s, size_t n);

/* True when the whole span is whitespace. */
int gt_all_space(const char *s, size_t n);

/* An identifier character, by C's rules: letters, digits, underscore. */
int gt_is_ident(int c);
/* }}} */

#endif
