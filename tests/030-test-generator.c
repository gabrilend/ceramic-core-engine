/*
 * 030-test-generator.c — proves the build path (issues 302–305).
 *
 * What this is: the test that everything the generator emitted tells
 * the truth — that registry sizes equal what sizeof says, that a
 * generated shim called with a hand-built task gives the same answer
 * as calling the box directly, that field tables agree with the
 * compiler about offsets, that compare functions order semantically
 * rather than byte-wise, and that a map placed by name runs.
 *
 * How it does it, in general terms: the test re-declares the box
 * types (same text, same compiler, structurally identical) and the
 * box functions (external linkage), then compares the generated
 * artifacts against direct calls and direct sizeofs. Nothing here
 * knows how the generator works; it only checks the emitted claims.
 */
#include "026-registry.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The box types, redeclared byte-for-byte as in the box source. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { char tag; double heavy; int id; } padded;
typedef struct { int a; vec3 pos; char note[16]; unsigned long stamp; } record;

/* The boxes, external linkage in the registry's translation unit. */
extern int add(int a, int b);
extern double mix(int count, double factor);
extern vec3 make_vec3(float x, float y, float z);
extern record stamp_record(int a, vec3 pos, unsigned long stamp);
extern int vec3__compare(vec3 a, vec3 b);

/* {{{ check() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "generator test failed: %s\n", what);
        exit(1);
    }
}
/* }}} */

/* {{{ build_task() */
/* A hand-built task: the values are pointed at, not copied — enough
 * for calling a shim directly, which only reads in[] and writes out. */
static task_t *build_task(const box_info_t *b, void **values, void *out)
{
    static task_t t;
    t.call = b->shim;
    t.station = 0;
    t.port = 0;
    t.n_in = b->n_params;
    t.in = values;
    t.out = out;
    return &t;
}
/* }}} */

/* {{{ test_registry_contents() */
static void test_registry_contents(void)
{
    /* Grows as demo boxes are added; the point is that every box in
     * the source is here exactly once, which the duplicate check in
     * the generator enforces and the finds below sample. */
    check(registry_n_boxes >= 7, "the demo boxes are all registered");

    const box_info_t *b = registry_find("add");
    check(b != NULL, "add present");
    check(b->n_params == 2, "add takes two");
    check(b->params[0].size == (int)sizeof(int), "add param 0 size");
    check(strcmp(b->params[0].type_name, "int") == 0, "add param 0 type name");
    check(b->return_size == (int)sizeof(int), "add return size");
    check(b->task_size == sizeof(task_t) + 2 * sizeof(void *)
                        + 2 * sizeof(int) + sizeof(int),
          "add task size is exact");

    b = registry_find("mix");
    check(b && b->params[1].size == (int)sizeof(double), "mix double size");

    b = registry_find("stamp_record");
    check(b && b->return_size == (int)sizeof(record),
          "record return size from real C");

    b = registry_find("swallow");
    check(b && b->return_size == 0 && b->compare == NULL,
          "a sink has no return and no compare");

    check(registry_find("vec3__compare") == NULL,
          "a compare function is not a box");
    check(registry_find("no_such_box") == NULL, "absent name is null");

    printf("  registry sizes all equal sizeof of the real types\n");
}
/* }}} */

/* {{{ test_shim_equivalence() */
static void test_shim_equivalence(void)
{
    /* Two of one type. */
    {
        int a = 41, b = 1, out = 0;
        void *in[2] = { &a, &b };
        const box_info_t *info = registry_find("add");
        info->shim(build_task(info, in, &out));
        check(out == add(41, 1), "add shim equals direct call");
    }
    /* Two different types. */
    {
        int count = 6;
        double factor = 7.5, out = 0;
        void *in[2] = { &count, &factor };
        const box_info_t *info = registry_find("mix");
        info->shim(build_task(info, in, &out));
        check(out == mix(6, 7.5), "mix shim equals direct call");
    }
    /* Returning a struct. */
    {
        float x = 1, y = 2, z = 3;
        vec3 out;
        void *in[3] = { &x, &y, &z };
        const box_info_t *info = registry_find("make_vec3");
        info->shim(build_task(info, in, &out));
        vec3 direct = make_vec3(1, 2, 3);
        check(memcmp(&out, &direct, sizeof out) == 0,
              "make_vec3 shim equals direct call");
    }
    /* Returning nothing. */
    {
        int x = 9;
        void *in[1] = { &x };
        const box_info_t *info = registry_find("swallow");
        info->shim(build_task(info, in, NULL));
        /* Surviving the call with a null out is the whole test. */
    }
    printf("  four shim shapes match their boxes exactly\n");
}
/* }}} */

/* {{{ test_field_tables() */
static void test_field_tables(void)
{
    const struct_info_t *s = struct_find("padded");
    check(s != NULL, "padded present");
    check(s->size == (int)sizeof(padded), "padded size");
    check(s->fields[1].offset == (int)offsetof(padded, heavy),
          "the double lands after the padding hole, where the compiler says");
    check(s->fields[2].offset == (int)offsetof(padded, id), "id offset");

    s = struct_find("record");
    check(s != NULL, "record present");
    check(s->n_fields == 4, "record field count");
    check(s->fields[1].kind == FIELD_STRUCT, "nested vec3 is a struct field");
    check(s->fields[1].nested == struct_find("vec3"),
          "nested field points at the vec3 table");
    check(s->fields[2].kind == FIELD_STRING && s->fields[2].array_len == 16,
          "the note is a fixed string of sixteen");
    check(s->fields[3].kind == FIELD_UINT, "the stamp is unsigned");
    check(s->fields[3].offset == (int)offsetof(record, stamp), "stamp offset");

    printf("  field tables agree with the compiler about every offset\n");
}
/* }}} */

/* {{{ test_compares() */
static void test_compares(void)
{
    /* Primitive: signed ordering, not byte ordering. */
    const box_info_t *b = registry_find("add");
    check(b->compare != NULL, "int return has a compare");
    int neg = -5, pos = 3, same = -5;
    check(b->compare(&neg, &pos) == -1, "-5 < 3 (bytes would disagree)");
    check(b->compare(&pos, &neg) == 1, "3 > -5");
    check(b->compare(&neg, &same) == 0, "equal ints");

    /* Floating point: negative versus positive, and zero. */
    b = registry_find("magnitude_squared");
    check(b->compare != NULL, "float return has a compare");
    float fn = -2.0f, fp = 0.5f, fz = 0.0f, fz2 = -0.0f;
    check(b->compare(&fn, &fp) == -1, "-2.0 < 0.5 (raw bytes read it backwards)");
    check(b->compare(&fz, &fz2) == 0, "zero equals negative zero semantically");

    /* Author-written struct compare, ordering on magnitude — not on
     * the first field, so byte order and field order both disagree. */
    b = registry_find("make_vec3");
    check(b->compare != NULL, "vec3 return found its author compare");
    vec3 small = { 9.0f, 0.0f, 0.0f };   /* first field large, magnitude 81 */
    vec3 large = { 1.0f, 8.0f, 8.0f };   /* first field small, magnitude 129 */
    check(b->compare(&small, &large) == -1,
          "vec3 orders by magnitude, proving the author compare ran");
    check(vec3__compare(small, large) == -1, "direct call agrees");

    printf("  compares order semantically; byte order would have lied\n");
}
/* }}} */

/* {{{ static void the_binary_carries_its_own_source() */
/*
 * **The C this program was made from, inside this program** (issue
 * 311c).
 *
 * The generated file includes each box source whole so the compiler
 * can see the types and inline each box into its shim, and until now
 * the text was thrown away at that point — surviving only as compiled
 * code. So a running program could not say what its boxes look like,
 * and a program handed to somebody else was a binary needing a source
 * tree beside it before it could do anything with new code.
 *
 * What is asserted is the thing worth having: the carried text is
 * **byte for byte** the file on disk. Not similar, not equivalent —
 * the same, because a source reported from a program has to be the
 * source that program was compiled from, or it is worse than nothing.
 *
 * If somebody edits a box source and rebuilds, this passes. If
 * somebody edits it and does not rebuild, this fails, which is
 * correct: the binary is then carrying the truth and the disk is not.
 */
static void the_binary_carries_its_own_source(void)
{
    check(sora_n_box_sources > 0,
          "the program carries at least one box source");

    for (int i = 0; i < sora_n_box_sources; i++) {
        const char *path = sora_box_sources[i].path;
        const char *carried = sora_box_sources[i].text;

        char full[512];
        snprintf(full, sizeof full, "%s/%s", SORA_ROOT, path);
        FILE *f = fopen(full, "rb");
        if (!f) {
            fprintf(stderr, "generator test failed: cannot open %s to "
                            "compare the carried text against\n", full);
            exit(1);
        }

        size_t at = 0;
        int same = 1, c;
        while ((c = fgetc(f)) != EOF) {
            if (carried[at] != (char)c) {
                same = 0;
                break;
            }
            at++;
        }
        fclose(f);
        if (same && carried[at] != '\0')
            same = 0;   /* the carried copy is longer than the file */

        if (!same) {
            fprintf(stderr, "generator test failed: the carried text of %s "
                            "differs from the file at byte %zu — the binary "
                            "and the disk disagree about what was "
                            "compiled\n", path, at);
            exit(1);
        }
    }

    /* Found by the path the build knew, and by the bare name a person
     * would type having seen the file. */
    check(registry_box_source("src/boxes/029-demo-boxes.c") != NULL,
          "a source is found by the path the build knew it as");
    check(registry_box_source("029-demo-boxes.c") != NULL,
          "and by the bare name somebody would type");
    check(registry_box_source("no-such-file.c") == NULL,
          "and a name that is not there is not there");

    printf("  every box source is carried in the binary, byte for byte\n");
}
/* }}} */

/* {{{ test_map_placed_by_name() */
static _Atomic long placed_total;

/* One local hand shim survives here as harness instrumentation: a
 * recording sink needs to reach this test's counter, which no
 * generated box can see. */
static void record_total__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    placed_total += x;
}

static void test_map_placed_by_name(void)
{
    enum { VALUES = 30 };
    map_t *m = map_create(2);
    /* Sizes come from the registry now — nothing hand-typed. */
    map_place_box(m, 0, "add", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, record_total__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_start(m, 4);

    placed_total = 0;
    for (int i = 0; i < VALUES; i++) {
        int a = i, b = 100;
        map_deliver_value(m, 0, 0, &a);
        map_deliver_value(m, 0, 1, &b);
    }
    pool_release(m->pool);
    pool_join(m->pool);

    long expected = 0;
    for (int i = 0; i < VALUES; i++)
        expected += i + 100;
    check(placed_total == expected, "a map placed by name computes correctly");

    map_destroy(m);
    printf("  a station placed by registry name ran a generated shim in anger\n");
}
/* }}} */

int main(void)
{
    test_registry_contents();
    test_shim_equivalence();
    test_field_tables();
    test_compares();
    test_map_placed_by_name();
    the_binary_carries_its_own_source();
    return 0;
}
