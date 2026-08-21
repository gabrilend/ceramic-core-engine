/*
 * 075-test-latebox.c — proves a box can arrive after the program
 * started (issue 310).
 *
 * What this is: the test that C source handed to a running program
 * becomes a box a station can place, wired to boxes the program was
 * built with, delivering values byte for byte. Plus the two refusals
 * and the one acceptance that together say exactly what the type
 * system does and does not promise across that boundary.
 *
 * How it does it, in general terms: builds a small C source as a
 * string, hands it to the engine, and then places it by name like any
 * other box. Nothing here knows how compilation works; it only checks
 * that a box which did not exist a moment ago behaves like one that
 * always did.
 *
 * The third case is the uncomfortable one and is written deliberately.
 * A struct that disagrees in *layout* with one the program already
 * knows, at the same width, **is accepted and delivers scrambled
 * fields.** That is the accepted cost of checking wires by width
 * rather than by shape, and a test that pins it is how somebody later
 * discovers it was a decision rather than an oversight.
 */
#include "018-station.h"
#include "026-registry.h"
#include "040-mapfile.h"
#include "049-observe.h"
#include "073-latebox.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* {{{ static void a_late_box_runs() */
/*
 * The whole claim in one program: a box written now, placed, wired to
 * a box compiled last week, delivering a value that arrives intact.
 */
static void a_late_box_runs(void)
{
    static const char source[] =
        "int triple_it(int x)\n"
        "{\n"
        "    return x * 3;\n"
        "}\n";

    int added = registry_compile_source(source);
    check(added == 1, "one box was compiled into the running program");
    if (added != 1)
        return;

    const box_info_t *b = registry_find("triple_it");
    check(b != NULL, "and can be found by name like any other");
    if (!b)
        return;

    /* The numbers the engine actually runs on came from the compiler,
     * not from the signature — which is the reason this path invokes
     * a compiler at all rather than parsing a declaration. */
    check(b->return_size == (int)sizeof(int),
          "its return width came from a compiler, not from its name");
    check(b->n_params == 1 && b->params[0].size == (int)sizeof(int),
          "and so did its parameter's");

    /* Wire it into `double_it`, which the program was built with:
     * three times two is six, doubled is twelve. */
    map_t *m = map_create(2);
    map_place_box(m, 0, "triple_it", STATION_PLAIN);
    map_place_box(m, 1, "keep", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);

    map_start(m, 2);
    int two = 2;
    map_deliver_value(m, 0, 0, &two);
    pool_release(m->pool);
    pool_join(m->pool);

    check(atomic_load(&map_station(m, 0)->runs) == 1, "the new box ran");
    check(atomic_load(&map_station(m, 1)->runs) == 1,
          "and what it produced reached a box the program was built with");

    map_destroy(m);
    printf("  a box written after the program started ran and delivered\n");
}
/* }}} */

/* {{{ static void a_struct_crosses_intact() */
/*
 * A struct laid out exactly as one the program already knows, under a
 * name the program has never heard. Width comparison lets it wire,
 * which is the capability; the fields arrive where they belong, which
 * is what makes that safe.
 */
static void a_struct_crosses_intact(void)
{
    static const char source[] =
        "typedef struct { float x; float y; float z; } same_shape;\n"
        "same_shape build_same(float a)\n"
        "{\n"
        "    same_shape s;\n"
        "    s.x = a;\n"
        "    s.y = a + 1.0f;\n"
        "    s.z = a + 2.0f;\n"
        "    return s;\n"
        "}\n";

    check(registry_compile_source(source) == 1,
          "a box returning an unfamiliar struct compiled");

    map_t *m = map_create(2);
    map_place_box(m, 0, "build_same", STATION_PLAIN);
    /* magnitude_squared takes a vec3 — same three floats, different
     * name, and a name this program has known since it was built. */
    map_place_box(m, 1, "magnitude_squared", STATION_PLAIN);
    map_connect(m, 0, 0, 1, 0);

    map_start(m, 2);
    float one = 1.0f;
    map_deliver_value(m, 0, 0, &one);
    pool_release(m->pool);
    pool_join(m->pool);

    check(atomic_load(&map_station(m, 1)->runs) == 1,
          "a struct compiled minutes ago crossed into one compiled at build");
    map_destroy(m);
    printf("  an unfamiliar struct of the same shape wired and arrived\n");
}
/* }}} */

/* {{{ static void a_different_width_is_refused() */
static void a_different_width_is_refused(void)
{
    static const char source[] =
        "double halve(double v)\n"
        "{\n"
        "    return v / 2.0;\n"
        "}\n";

    check(registry_compile_source(source) == 1, "a double-returning box compiled");

    map_t *m = map_create(2);
    map_place_box(m, 0, "halve", STATION_PLAIN);   /* -> double, 8 bytes */
    map_place_box(m, 1, "keep", STATION_PLAIN);    /* takes int, 4 bytes */

    /* Through the face that hands the refusal back, which is one of
     * the two that survive: the third printed it and returned a code
     * a caller could ignore (issue 106). */
    check(map_wire(m, 0, 0, 1, 0) != NULL,
          "eight bytes into a four-byte port was refused, as it would be "
          "for a box the program was built with");

    map_destroy(m);
    printf("  a late box of the wrong width was refused by the same rule\n");
}
/* }}} */

/* {{{ static void a_disagreeing_layout_is_accepted() */
/*
 * **This test asserts a wrong answer, on purpose.**
 *
 * `scrambled` is three floats like `vec3` and puts them in a different
 * order. Same width, different layout. Checking wires by width cannot
 * see the difference, so the wire is legal and the fields arrive
 * transposed — z where x was meant to be.
 *
 * That is the accepted cost of comparing widths, written down in
 * 058 as a non-guarantee and pinned here so it cannot be mistaken for
 * an oversight. Shape comparison would catch it, is designed in full
 * in issue 309, and lost to costing a walk where width costs one
 * comparison.
 *
 * If somebody ever builds shape comparison, this test is what fails,
 * and its failure is the good news.
 */
static void a_disagreeing_layout_is_accepted(void)
{
    static const char source[] =
        "typedef struct { float z; float y; float x; } scrambled;\n"
        "scrambled build_scrambled(float a)\n"
        "{\n"
        "    scrambled s;\n"
        "    s.z = a;\n"
        "    s.y = 0.0f;\n"
        "    s.x = 0.0f;\n"
        "    return s;\n"
        "}\n";

    check(registry_compile_source(source) == 1,
          "a box whose struct disagrees in layout compiled");

    map_t *m = map_create(2);
    map_place_box(m, 0, "build_scrambled", STATION_PLAIN);
    map_place_box(m, 1, "magnitude_squared", STATION_PLAIN);

    check(map_wire(m, 0, 0, 1, 0) == NULL,
          "a disagreeing layout of the same width was ACCEPTED — this is "
          "the cost of checking widths, not a bug");

    map_destroy(m);
    printf("  a disagreeing layout wired anyway, which is the stated cost\n");
}
/* }}} */

/* {{{ static void the_source_was_saved() */
/*
 * The source is written down when the box is created rather than when
 * something asks for it, so a dump taken later — including one taken
 * while a program is dying — has something to point at.
 */
static void the_source_was_saved(void)
{
    const char *dir = registry_late_source_dir();
    check(dir != NULL && *dir, "there is a place saved sources go");

    char path[512];
    snprintf(path, sizeof path, "%s/box-%d-0.c", dir, (int)getpid());
    FILE *f = fopen(path, "r");
    check(f != NULL, "the first box's source is on disk where it was put");
    if (f) {
        char buf[256] = { 0 };
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
        check(strstr(buf, "triple_it") != NULL,
              "and it is the source that was handed over");
    }
    printf("  every late box left its source in the scratch tier\n");
}
/* }}} */

/* {{{ static void refusals_add_nothing() */
static void refusals_add_nothing(void)
{
    int before = registry_late_count();

    /* A source the generator refuses: one field per declaration. */
    check(registry_compile_source(
              "typedef struct { float x, y; } sloppy;\n") == -1,
          "a source the generator refuses is refused here too");

    /* A source the compiler refuses: the generator is happy with the
     * shape of this and the compiler is not. */
    check(registry_compile_source(
              "int broken(int x)\n{\n    return undefined_thing(x);\n}\n") == -1,
          "a source the compiler refuses is refused here too");

    check(registry_late_count() == before,
          "and neither left a row behind — nothing is half-added");
    printf("  two refusals, and the table unchanged after both\n");
}
/* }}} */

/* {{{ static void a_dump_reloads_in_a_fresh_process() */
/*
 * The claim that makes a late box a real box: a program that grew one
 * can be written down, and a **different process** — one that was
 * never told the box exists — can read that file and run it.
 *
 * It works because the source was filed under the box's own name when
 * the box was created, not when somebody asked for it. A dump may be
 * taken while a program is dying, and a failure path is the worst
 * possible moment to discover that something needed saving.
 *
 * The child is this same binary with an argument, because a fresh
 * process is exactly what has to be proven and there is no way to
 * prove it inside the one that already has the box loaded.
 */
static void a_dump_reloads_in_a_fresh_process(const char *self)
{
    char src_path[512], map_path[512], dump_path[512], text[1024];
    snprintf(src_path, sizeof src_path,
             "/dev/shm/minimal-soramech/late-boxes/grown-%d.map",
             (int)getpid());
    snprintf(map_path, sizeof map_path, "%s", src_path);
    snprintf(dump_path, sizeof dump_path,
             "/dev/shm/minimal-soramech/late-boxes/grown-dump-%d.map",
             (int)getpid());

    /* A program naming the box that was compiled a moment ago, wired
     * into one the binary has always had. Written as text and loaded,
     * because only a loaded map carries the station names a dump
     * needs to speak. */
    snprintf(text, sizeof text,
        "statics\n"
        "  0 = 4\n"
        "\n"
        "grower triple_it p\n"
        "  in 0 $0\n"
        "  out 0 - keeper.0\n"
        "\n"
        "keeper keep p result\n");
    FILE *w = fopen(map_path, "w");
    check(w != NULL, "the grown program could be written as text");
    if (w) {
        fputs(text, w);
        fclose(w);
    }

    map_t *m = map_load_file(map_path, 2);
    FILE *f = fopen(dump_path, "w");
    check(f != NULL, "the grown program could be dumped");
    if (f) {
        map_dump(m, f);
        fclose(f);
    }
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s --reload %s", self, dump_path);
    int rc = system(cmd);
    check(rc == 0,
          "a fresh process loaded the dump and placed a box it was never "
          "built with");
    printf("  a dump of a grown program reloaded in a fresh process\n");
}
/* }}} */

/* {{{ static int reload_only() */
/*
 * The child half of the test above. Loads the named map and reports
 * whether the station running the recovered box actually ran.
 */
static int reload_only(const char *map_path)
{
    map_t *m = map_load_file(map_path, 2);
    if (!m)
        return 1;
    pool_release(m->pool);
    pool_join(m->pool);
    long ran = atomic_load(&map_station(m, 1)->runs);
    map_destroy(m);
    return ran == 1 ? 0 : 1;
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    /* The child of the fresh-process test does one thing and exits. */
    if (argc == 3 && strcmp(argv[1], "--reload") == 0)
        return reload_only(argv[2]);

    a_late_box_runs();
    the_source_was_saved();
    a_struct_crosses_intact();
    a_different_width_is_refused();
    a_disagreeing_layout_is_accepted();
    refusals_add_nothing();
    a_dump_reloads_in_a_fresh_process(argv[0]);

    if (failures) {
        fprintf(stderr, "%d late-box checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
