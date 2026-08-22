/*
 * 030-test-generator.c — proves the build path (issues 302–305).
 *
 * What this is: the test that everything the generator emitted tells
 * the truth — that emitted sizes equal what sizeof says, that a
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
#include "026-emitted.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The box types, redeclared byte-for-byte as in the box source. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { char tag; double heavy; int id; } padded;
typedef struct { int a; vec3 pos; char note[16]; unsigned long stamp; } record;

/* The boxes, external linkage in the emitted file's translation unit. */
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

/* {{{ placed() */
/*
 * **A station, placed by name**, which is where a box's shape can be
 * read now that the record is gone (issue 311b).
 *
 * The record held a shim, a parameter count, a size per parameter and
 * a return size, and this file used to read them from it. Every one
 * of those is written onto a station by the box's placement function,
 * from a `sizeof` the compiler folded — so asking a station is asking
 * the same question of the thing that actually gets used, rather than
 * of a copy kept beside it.
 */
static map_t *placed_map;

static station_t *placed_as(const char *box_name, int kind)
{
    if (!placed_map)
        placed_map = map_create_empty();
    int at = map_add_station(placed_map);
    map_place_box(placed_map, at, box_name, kind);
    return map_station(placed_map, at);
}

static station_t *placed(const char *box_name)
{
    return placed_as(box_name, STATION_PLAIN);
}

/* A comparator, which is the only placement that resolves a
 * comparison onto the station — a plain station has no use for one. */
static station_t *placed_comparator(const char *box_name)
{
    return placed_as(box_name, STATION_COMPARATOR);
}
/* }}} */

/* {{{ build_task() */
/* A hand-built task: the values are pointed at, not copied — enough
 * for calling a shim directly, which only reads in[] and writes out. */
static task_t *build_task(station_t *s, void **values, void *out)
{
    static task_t t;
    t.call = s->call;
    t.station = 0;
    t.port = 0;
    t.n_in = s->n_in_ports;
    t.in = values;
    t.out = out;
    return &t;
}
/* }}} */

/* {{{ test_emitted_contents() */
/*
 * **Every size a placed station holds equals the compiler's own
 * answer**, which is the claim this whole build path rests on and the
 * one that must never quietly stop being true.
 *
 * It used to ask the box *record* — a table the generator emitted
 * beside the placement functions — and the record is gone (issue
 * 311b). It was read once, at placement, and never again: a station
 * holds its own shim, its own slot sizes, its own return size and its
 * own comparison, so the record was a copy of numbers nobody consulted
 * twice. Asking a station instead is the same question put to the
 * thing that gets used.
 */
static void test_emitted_contents(void)
{
    /* Grows as demo boxes are added; the point is that every box in
     * the source is placeable exactly once, which the duplicate check
     * in the generator enforces and the placements below sample. */
    check(n_box_places >= 7, "the demo boxes are all placeable");

    station_t *s = placed("add");
    check(s->n_in_ports == 2, "add takes two");
    check(s->in_ports[0].elem_size == (int)sizeof(int), "add port 0 size");
    check(strcmp(s->in_ports[0].type_name, "int") == 0, "add port 0 type");
    check(s->out_size == (int)sizeof(int), "add return size");

    s = placed("mix");
    check(s->in_ports[1].elem_size == (int)sizeof(double),
          "mix double size");

    s = placed("stamp_record");
    check(s->out_size == (int)sizeof(record),
          "record return size from real C");

    s = placed("swallow");
    check(s->out_size == 0 && s->compare == NULL,
          "a sink has no return and no compare");

    check(box_place_find("vec3__compare") == NULL,
          "a compare function is not a box");
    check(box_place_find("no_such_box") == NULL, "absent name is null");

    printf("  every size a placed station holds equals sizeof of the real "
           "type\n");
}
/* }}} */

/* {{{ test_shim_equivalence() */
/*
 * **The shim comes off a placed station**, which is where it lives
 * (issue 311b). It used to come off the box record, and a station is
 * the only thing that ever held it in anger.
 */
static void test_shim_equivalence(void)
{
    /* Two of one type. */
    {
        int a = 41, b = 1, out = 0;
        void *in[2] = { &a, &b };
        station_t *s = placed("add");
        s->call(build_task(s, in, &out));
        check(out == add(41, 1), "add shim equals direct call");
    }
    /* Two different types. */
    {
        int count = 6;
        double factor = 7.5, out = 0;
        void *in[2] = { &count, &factor };
        station_t *s = placed("mix");
        s->call(build_task(s, in, &out));
        check(out == mix(6, 7.5), "mix shim equals direct call");
    }
    /* Returning a struct. */
    {
        float x = 1, y = 2, z = 3;
        vec3 out;
        void *in[3] = { &x, &y, &z };
        station_t *s = placed("make_vec3");
        s->call(build_task(s, in, &out));
        vec3 direct = make_vec3(1, 2, 3);
        check(memcmp(&out, &direct, sizeof out) == 0,
              "make_vec3 shim equals direct call");
    }
    /* Returning nothing. */
    {
        int x = 9;
        void *in[1] = { &x };
        station_t *s = placed("swallow");
        s->call(build_task(s, in, NULL));
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
    /*
     * **The comparison comes off a placed station**, which is where
     * it is resolved to (issue 311b) — the delivery path compares
     * through a pointer the station holds rather than looking
     * anything up per value, and a comparator has to be placed as one
     * for that pointer to be written.
     */
    /* Primitive: signed ordering, not byte ordering. */
    station_t *b = placed_comparator("add");
    check(b->compare != NULL, "int return has a compare");
    int neg = -5, pos = 3, same = -5;
    check(b->compare(&neg, &pos) == -1, "-5 < 3 (bytes would disagree)");
    check(b->compare(&pos, &neg) == 1, "3 > -5");
    check(b->compare(&neg, &same) == 0, "equal ints");

    /* Floating point: negative versus positive, and zero. */
    b = placed_comparator("magnitude_squared");
    check(b->compare != NULL, "float return has a compare");
    float fn = -2.0f, fp = 0.5f, fz = 0.0f, fz2 = -0.0f;
    check(b->compare(&fn, &fp) == -1, "-2.0 < 0.5 (raw bytes read it backwards)");
    check(b->compare(&fz, &fz2) == 0, "zero equals negative zero semantically");

    /* Author-written struct compare, ordering on magnitude — not on
     * the first field, so byte order and field order both disagree. */
    b = placed_comparator("make_vec3");
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
    check(box_source_text("src/boxes/029-demo-boxes.c") != NULL,
          "a source is found by the path the build knew it as");
    check(box_source_text("029-demo-boxes.c") != NULL,
          "and by the bare name somebody would type");
    check(box_source_text("no-such-file.c") == NULL,
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
    /* Sizes come from the emitted file now — nothing hand-typed. */
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
    printf("  a station placed by name ran a generated shim in anger\n");
}
/* }}} */

int main(void)
{
    test_emitted_contents();
    test_shim_equivalence();
    test_field_tables();
    test_compares();
    test_map_placed_by_name();
    the_binary_carries_its_own_source();
    return 0;
}
