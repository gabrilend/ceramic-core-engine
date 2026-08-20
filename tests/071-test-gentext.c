/*
 * 071-test-gentext.c — proves the generator's support machinery
 * (issue 308, step 1).
 *
 * What this is: the tests for the four things the generator needed
 * before it could be written in C at all — an arena, a string that
 * grows, an array that grows, and the string operations the parser
 * leans on. Everything above these assumes they are correct, and a
 * bug in them surfaces as garbled C hundreds of lines away, so they
 * are proven on their own before anything stands on them.
 *
 * How it does it, in general terms: each piece is exercised past the
 * point where it has to grow, because growing is where a container
 * gets its bugs, and the string helpers are checked against the exact
 * spellings the old Lua generator produced — those strings are the
 * registry's type names, so two spellings of one type would become
 * two types.
 *
 * This test links the generator's own pieces rather than the engine.
 * It is testing the tool, not the thing the tool builds.
 */
#include "065-gentext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

/* {{{ static void check() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "  FAIL: %s\n", what);
        failures++;
    }
}
/* }}} */

/* {{{ static void check_str() */
static void check_str(const char *got, const char *want, const char *what)
{
    if (!got || strcmp(got, want) != 0) {
        fprintf(stderr, "  FAIL: %s\n    got  '%s'\n    want '%s'\n",
                what, got ? got : "(null)", want);
        failures++;
    }
}
/* }}} */

/* {{{ static void test_arena() */
/*
 * The arena has to survive a request larger than its block size, and
 * it has to keep every earlier hand-out valid when it starts a new
 * block — which is the whole reason it is a list of blocks rather
 * than one realloc'd slab.
 */
static void test_arena(void)
{
    arena_t *a = arena_new();

    /* Enough small allocations to cross several blocks, each written
     * to a distinct value and checked afterwards, so a block change
     * that invalidated an earlier pointer would show. */
    enum { N = 4000 };
    int *kept[N];
    for (int i = 0; i < N; i++) {
        kept[i] = arena_alloc(a, sizeof(int) * 4);
        kept[i][0] = i;
        kept[i][3] = -i;
    }
    int intact = 1;
    for (int i = 0; i < N; i++)
        if (kept[i][0] != i || kept[i][3] != -i)
            intact = 0;
    check(intact, "arena hand-outs survive later allocations");

    /* Zeroed on the way out — the parser relies on a fresh record
     * having null pointers and zero counts. */
    unsigned char *z = arena_alloc(a, 512);
    int all_zero = 1;
    for (int i = 0; i < 512; i++)
        if (z[i] != 0)
            all_zero = 0;
    check(all_zero, "arena memory arrives zeroed");

    /* One request far larger than a block. */
    char *big = arena_alloc(a, 1024 * 1024);
    big[0] = 'a';
    big[1024 * 1024 - 1] = 'z';
    check(big[0] == 'a' && big[1024 * 1024 - 1] == 'z',
          "an allocation larger than a block is served whole");

    check_str(arena_strdup(a, "hello"), "hello", "arena_strdup copies");
    check_str(arena_strndup(a, "hello world", 5), "hello",
              "arena_strndup stops where told");

    arena_free(a);
    printf("  arena: %d hand-outs across blocks, all intact and zeroed\n", N);
}
/* }}} */

/* {{{ static void test_buf() */
static void test_buf(void)
{
    buf_t b;
    buf_init(&b);

    buf_addstr(&b, "one");
    buf_addch(&b, ' ');
    buf_add(&b, "two three", 3);
    check_str(b.data, "one two", "buffer appends strings, chars, and spans");

    buf_free(&b);
    buf_init(&b);
    buf_line(&b, "%s = %d;", "x", 42);
    check_str(b.data, "x = 42;\n", "buf_line formats and ends the line");

    /* Past the doubling point several times over, with a long format
     * that vsnprintf must be asked to measure rather than guessed at. */
    buf_free(&b);
    buf_init(&b);
    for (int i = 0; i < 5000; i++)
        buf_line(&b, "    { \"%s\", (int)sizeof(%s), %d },", "field", "vec3", i);
    check(b.len > 100000, "buffer grew past its first capacity");
    check(strstr(b.data, "4999 },") != NULL,
          "the last line written is intact after many growths");
    check(b.data[b.len] == '\0', "the buffer stays terminated");

    buf_free(&b);
    printf("  buffer: 5000 formatted lines, %s\n", "grown and intact");
}
/* }}} */

/* {{{ static void test_vec() */
typedef struct { int a; char name[8]; double d; } item_t;

static void test_vec(void)
{
    vec_t v;
    vec_init(&v, sizeof(item_t));

    enum { N = 3000 };
    for (int i = 0; i < N; i++) {
        item_t *it = vec_push(&v);
        it->a = i;
        it->d = i * 1.5;
        snprintf(it->name, sizeof it->name, "n%d", i % 100);
    }
    check(v.n == N, "vector counted every push");

    int intact = 1;
    for (int i = 0; i < N; i++) {
        item_t *it = vec_at(&v, i);
        if (it->a != i || it->d != i * 1.5)
            intact = 0;
    }
    check(intact, "every element survived the growths");

    /* A fresh element arrives zeroed, which the parser depends on for
     * optional fields like a struct field's array length. */
    item_t *fresh = vec_push(&v);
    check(fresh->a == 0 && fresh->d == 0.0 && fresh->name[0] == '\0',
          "a pushed element arrives zeroed");

    vec_free(&v);
    printf("  vector: %d elements across doublings, all intact\n", N);
}
/* }}} */

/* {{{ static void test_strings() */
/*
 * These spellings are the registry's type names. The old Lua
 * generator produced exactly these, and a wire is checked by
 * comparing them, so a difference here is two spellings of one type
 * becoming two incompatible types.
 */
static void test_strings(void)
{
    arena_t *a = arena_new();

    check_str(gt_normalize_type(a, "int", 3), "int", "a bare type is unchanged");
    check_str(gt_normalize_type(a, "  int  ", 7), "int", "ends are trimmed");
    check_str(gt_normalize_type(a, "const char*", 11), "const char *",
              "a star with no space gains one before it");
    check_str(gt_normalize_type(a, "const  char  *", 14), "const char *",
              "runs of space collapse and the star settles");
    check_str(gt_normalize_type(a, "const char  *  ", 15), "const char *",
              "space after a star is dropped");
    check_str(gt_normalize_type(a, "unsigned  long   long", 21),
              "unsigned long long", "interior runs collapse to one space");
    check_str(gt_normalize_type(a, "char **", 7), "char * *",
              "two stars each get their own space, as the old generator did");

    check_str(gt_mangle(a, "const char *"), "const_char_ptr",
              "mangling makes an identifier of a pointer type");
    check_str(gt_mangle(a, "vec3"), "vec3", "a plain name mangles to itself");
    check_str(gt_mangle(a, "unsigned long"), "unsigned_long",
              "spaces become underscores");

    check_str(gt_trim(a, "  hello  ", 9), "hello", "trim takes both ends");
    check_str(gt_trim(a, "     ", 5), "", "all-space trims to nothing");

    check(gt_all_space("  \t\n ", 5), "all_space sees only whitespace");
    check(!gt_all_space("  x  ", 5), "all_space sees a character");

    const char *text = "one\ntwo\nthree";
    check(gt_line_of(text, 0) == 1, "offset zero is line one");
    check(gt_line_of(text, 4) == 2, "past one newline is line two");
    check(gt_line_of(text, 8) == 3, "past two newlines is line three");

    check(gt_is_ident('a') && gt_is_ident('Z') && gt_is_ident('0')
          && gt_is_ident('_'), "identifier characters are recognized");
    check(!gt_is_ident('*') && !gt_is_ident(' ') && !gt_is_ident('-'),
          "non-identifier characters are not");

    arena_free(a);
    printf("  strings: type spellings match what the registry compares\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
    test_arena();
    test_buf();
    test_vec();
    test_strings();

    if (failures) {
        fprintf(stderr, "%d support-machinery checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
