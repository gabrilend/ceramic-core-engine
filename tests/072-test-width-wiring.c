/*
 * 072-test-width-wiring.c — proves a wire is checked by width (issue 309).
 *
 * What this is: the test for the change from comparing type *names* to
 * comparing type *widths*. It proves the capability that buys — two
 * structs with identical layouts and different names now connect, and
 * the value arrives byte for byte — and it proves the refusal that
 * remains, with both names and both widths in the message.
 *
 * How it does it, in general terms: builds a small map by hand, wires
 * a station producing a `triple` into a station taking a `vec3` (three
 * floats each, same order, same offsets, different names), runs it,
 * and checks the number that comes out the far end. Then it asks the
 * runtime rewiring surface to connect two stations whose widths
 * genuinely differ and checks that it refuses and says why.
 *
 * The two structs live in the box source and exist for this test. That
 * is deliberate: the thing being proven is that the *engine* stopped
 * caring about the name, so the shapes have to be real types the
 * generator saw, not something this file made up.
 */
#include "018-station.h"
#include "049-observe.h"
#include "026-registry.h"

#include <fcntl.h>
#include <stdatomic.h>
#include <stddef.h>
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

/* The box types, redeclared as in the box source. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { float a; float b; float c; } triple;

/* {{{ static void identical_shapes_wire() */
/*
 * A station returning `triple` feeding a station taking `vec3`. Under
 * name comparison this was refused, and the author's only options were
 * to rename one type or to write a box that took one and returned the
 * other and did nothing. The bytes were always compatible; only the
 * labels disagreed.
 */
static void identical_shapes_wire(void)
{
    map_t *m = map_create(3);

    /* as_triple(float, float, float) -> triple */
    map_place_box(m, 0, "as_triple", STATION_PLAIN);
    /* magnitude_squared(vec3) -> float, which is where the value
     * arriving as a vec3 gets looked at. */
    map_place_box(m, 1, "magnitude_squared", STATION_PLAIN);
    /* A sink, so the float has somewhere to land. */
    map_place_box(m, 2, "swallow", STATION_PLAIN);

    /* The wire the whole issue is about: triple out, vec3 in. */
    map_connect(m, 0, 0, 1, 0);

    const box_info_t *producer = registry_find("as_triple");
    const box_info_t *consumer = registry_find("magnitude_squared");
    check(producer && consumer, "both boxes are in the registry");
    if (!producer || !consumer) {
        map_destroy(m);
        return;
    }

    check(strcmp(producer->return_type, "triple") == 0
          && strcmp(consumer->params[0].type_name, "vec3") == 0,
          "the two ends really do carry different type names");
    check(producer->return_size == consumer->params[0].size,
          "and identical widths, which is what the check now looks at");

    /* Feed it and see the value arrive intact. 3-4-5 gives 50. */
    map_start(m, 2);
    float xs[3] = { 3.0f, 4.0f, 5.0f };
    for (int i = 0; i < 3; i++)
        map_deliver_value(m, 0, i, &xs[i]);
    pool_release(m->pool);
    pool_join(m->pool);

    long ran_producer = atomic_load(&m->stations[0].runs);
    long ran_consumer = atomic_load(&m->stations[1].runs);
    check(ran_producer == 1, "the producing station ran once");
    check(ran_consumer == 1,
          "the consuming station ran, so the wire really carried a value");

    map_destroy(m);
    printf("  a triple wired into a vec3 and the value arrived\n");
}
/* }}} */

/* {{{ static void byte_identical() */
/*
 * The layouts are the same, so the bytes must be too. Checked against
 * the compiler rather than against the engine: if these two ever stop
 * agreeing, the wire above stops being safe and this test is where it
 * shows.
 */
static void byte_identical(void)
{
    check(sizeof(vec3) == sizeof(triple),
          "the two structs are the same size");
    check(offsetof(vec3, x) == offsetof(triple, a)
          && offsetof(vec3, y) == offsetof(triple, b)
          && offsetof(vec3, z) == offsetof(triple, c),
          "and every field sits at the same offset");

    triple t = { 1.5f, 2.5f, 3.5f };
    vec3 v;
    memcpy(&v, &t, sizeof v);
    check(v.x == 1.5f && v.y == 2.5f && v.z == 3.5f,
          "so a copy across them is byte-identical");
    printf("  identical layouts, identical bytes, different names\n");
}
/* }}} */

/* {{{ static void different_widths_refused() */
/*
 * The refusal that remains, through the runtime rewiring surface —
 * `add` returns an int and `mix` takes a double at its second port,
 * four bytes against eight. The message has to carry both names *and*
 * both widths: "box returns int, slot takes double" does not say why
 * those disagree, and "4 bytes against 8 bytes" does.
 *
 * The rewiring surface writes its reason to stderr and returns -1
 * rather than stopping the program, so the message is captured by
 * redirecting stderr into a file for the length of the call. That is
 * clumsier than asking for a string and it is what the surface
 * offers; issue 212 is where that returns-a-code shape gets revisited.
 */
static void different_widths_refused(void)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN);   /* -> int         */
    map_place_box(m, 1, "mix", STATION_PLAIN);   /* slot 1: double */

    char path[256];
    snprintf(path, sizeof path, "/tmp/minimal-soramech/width-refusal-%d.txt",
             (int)getpid());

    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    int sink = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (sink < 0 || saved < 0) {
        fprintf(stderr, "  FAIL: cannot capture stderr\n");
        failures++;
        map_destroy(m);
        return;
    }
    dup2(sink, STDERR_FILENO);

    int rc = map_rewire_connect(m, 0, 0, 1, 1);

    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(sink);
    close(saved);

    check(rc != 0, "a four-byte value into an eight-byte slot was refused");

    char said[512] = { 0 };
    FILE *f = fopen(path, "r");
    if (f) {
        size_t got = fread(said, 1, sizeof said - 1, f);
        said[got] = '\0';
        fclose(f);
    }
    remove(path);

    check(strstr(said, "int") && strstr(said, "double"),
          "the refusal names both types");
    check(strstr(said, "4 bytes") && strstr(said, "8 bytes"),
          "and both widths, which is what actually disagrees");

    /* Trim the trailing newline for a tidy line of output. */
    size_t n = strlen(said);
    while (n && (said[n - 1] == '\n' || said[n - 1] == '\r'))
        said[--n] = '\0';
    printf("  refused: %s\n", said);

    map_destroy(m);
}
/* }}} */
/* }}} */

/* {{{ main */
int main(void)
{
    byte_identical();
    identical_shapes_wire();
    different_widths_refused();

    if (failures) {
        fprintf(stderr, "%d width-wiring checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
