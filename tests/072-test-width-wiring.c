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
/*
 * This test reaches into the engine's own machinery — slots, pages,
 * destination sets, the constants a port holds — rather than only
 * calling what a program built with this engine calls. So it includes
 * the engine's *source* and is compiled as one unit with it (issue
 * 903). Those functions are private, and a private function cannot be
 * called from another translation unit no matter what is declared.
 *
 * A white-box test belongs inside the thing it examines. The
 * alternative was keeping the machinery public so this file could
 * reach it, which makes the test suite the reason a consumer's link
 * fails, and leaves private-by-default depending on nobody ever
 * writing another test like this one.
 *
 * The build rule for these does not also put the engine on the link
 * line: it is already here, and doing both is every symbol twice.
 */
#include "cera.c"

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
    cera_map_t *m = cera_map_create(3);

    /* as_triple(float, float, float) -> triple */
    cera_map_place_box(m, 0, "as_triple", CERA_STATION_PLAIN);
    /* magnitude_squared(vec3) -> float, which is where the value
     * arriving as a vec3 gets looked at. */
    cera_map_place_box(m, 1, "magnitude_squared", CERA_STATION_PLAIN);
    /* A sink, so the float has somewhere to land. */
    cera_map_place_box(m, 2, "swallow", CERA_STATION_PLAIN);

    /* The wire the whole issue is about: triple out, vec3 in. */
    cera_map_connect(m, 0, 0, 1, 0);

    /*
     * **Asked of the two stations**, which is where these facts live
     * now that the box record is gone (issue 311b). A station carries
     * its own return size and its ports carry their own type names,
     * written by the placement function from numbers the compiler
     * folded — so this is the same question put to the thing that
     * actually gets wired.
     */
    cera_station_t *producer = cera_map_station(m, 0);
    cera_station_t *consumer = cera_map_station(m, 1);

    check(strcmp(consumer->in_ports[0].type_name, "vec3") == 0,
          "the receiving end is spelled vec3");
    check(producer->out_size == consumer->in_ports[0].elem_size,
          "and the two ends count the same bytes, which is what the "
          "check looks at");
    check(producer->out_size == (int)sizeof(triple)
          && (int)sizeof(triple) == (int)sizeof(vec3),
          "while the two type names are different ones of the same "
          "width, which is the whole point");

    /* Feed it and see the value arrive intact. 3-4-5 gives 50. */
    cera_map_start(m, 2);
    float xs[3] = { 3.0f, 4.0f, 5.0f };
    for (int i = 0; i < 3; i++)
        cera_map_deliver_value(m, 0, i, &xs[i]);
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    long ran_producer = atomic_load(&cera_map_station(m, 0)->runs);
    long ran_consumer = atomic_load(&cera_map_station(m, 1)->runs);
    check(ran_producer == 1, "the producing station ran once");
    check(ran_consumer == 1,
          "the consuming station ran, so the wire really carried a value");

    cera_map_destroy(m);
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
 * The refusal that remains — `add` returns an int and `mix` takes a
 * double at its second port, four bytes against eight.
 *
 * **The message names both ends by position and by size, and carries
 * no type name at all.** It used to give the two spellings, which
 * reads well and is the wrong shape for what this engine checks: a
 * name is not what makes a wire legal, the width is, so a message
 * built around names sends somebody to look at the thing that is not
 * the disagreement. Two types with one layout and different names
 * wire perfectly. What actually conflicts is a place and a count on
 * each end, so that is what the message gives.
 *
 * **It asks for the reason instead of capturing it.** This scene used
 * to redirect stderr into a file for the length of the call, because
 * the only rewiring face available printed its refusal and returned a
 * code. That face is gone (issue 106): a refusal a caller can ignore
 * is a refusal that leaves a program running that somebody believes
 * they just edited. What is left hands the sentence back, and a test
 * that wants to read it simply reads it.
 */
static void different_widths_refused(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "add", CERA_STATION_PLAIN);   /* -> int         */
    cera_map_place_box(m, 1, "mix", CERA_STATION_PLAIN);   /* port 1: double */

    const char *said = cera_map_wire(m, 0, 0, 1, 1);

    check(said != NULL,
          "a four-byte value into an eight-byte port was refused");
    if (said) {
        check(strstr(said, "output 0") && strstr(said, "input 1"),
              "the refusal names both ends by position");
        check(strstr(said, "4 bytes") && strstr(said, "8 bytes"),
              "and both counts, which is what actually disagrees");
        printf("  refused: %s\n", said);
    }

    cera_map_destroy(m);
}
/* }}} */

/* {{{ static void a_source_with_no_inputs_is_checked_too() */
/*
 * **The same refusal, from a station that takes nothing** — and this
 * is the scene that would have caught a real hole.
 *
 * The width check in the wiring operation used to sit behind a
 * condition asking whether *both* stations had input port arrays. The
 * destination always does, or the port index would have been refused
 * already. The source frequently does not: a box that takes no
 * arguments and returns a value is how most programs begin, and every
 * one of them could be wired into a port of any width at all with
 * nothing said.
 *
 * It stayed invisible because the loader carried a width check of its
 * own, so every program read from a file met that one first. The hole
 * only opened when a program was built by calling the surface — which
 * is the exact shape of fault two paths produce, and the reason there
 * is one path now (issue 210g).
 *
 * `seven` takes nothing and returns an int; `mix` takes a double at
 * its second port. Four bytes against eight, with no input array on
 * the near end.
 */
static void a_source_with_no_inputs_is_checked_too(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "seven", CERA_STATION_PLAIN);  /* () -> int      */
    cera_map_place_box(m, 1, "mix", CERA_STATION_PLAIN);    /* port 1: double */

    check(cera_map_station(m, 0)->n_in_ports == 0,
          "the source really does take nothing, which is the whole point");

    const char *no = cera_map_wire(m, 0, 0, 1, 1);
    check(no != NULL,
          "a four-byte value from an input-less station into an eight-byte "
          "port was refused");
    if (no) {
        check(strstr(no, "4 bytes") && strstr(no, "8 bytes"),
              "and the refusal says which counts disagree");
        printf("  refused: %s\n", no);
    }

    /* And the wire really did not happen — a refusal that says no and
     * connects anyway is worse than one that says yes. */
    check(out_port_dests(station_out_port(cera_map_station(m, 0), 0)) == NULL,
          "and nothing was connected");

    cera_map_destroy(m);
}
/* }}} */

/* {{{ main */
int main(void)
{
    byte_identical();
    identical_shapes_wire();
    different_widths_refused();
    a_source_with_no_inputs_is_checked_too();

    if (failures) {
        fprintf(stderr, "%d width-wiring checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
