/*
 * 139-test-depth-travels.c — a declared depth reaches the stations
 * below it.
 *
 * What this is: the proof that saying how deep one port should be says
 * it for the chain, and that the four station kinds each pass it on the
 * way their own routing works.
 *
 * Why it exists. Growing a ring buffer is cheap — it appends a page and
 * copies nothing — but it happens **on the delivery path, under the
 * station's own mutex**, which is the lock the readiness check also
 * wants. A burst of a hundred values into a ten-slot buffer takes that
 * lock about nine extra times, for a reason nothing in the map shows.
 * An author who knows the burst is coming can already say so on the
 * first port; before this, they had to say it again on every station
 * below, and a map that grew a station later had a hole in the middle
 * of it.
 *
 * How it does it, in general terms: each scene builds a small graph,
 * declares a depth on one port, and asks what the ports downstream came
 * to. The kinds differ in exactly one way — where a returned value goes
 * — so each gets its own scene, and the two that pass nothing on get
 * one too, because a rule with no edges is a rule nobody can rely on.
 *
 * The last two scenes are the ones that would be quietest to break: a
 * cycle, which has no end to walk to, and a wire drawn *after* the
 * depth was declared, which is the case a load-time-only pass would
 * miss entirely.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "  refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

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

/* {{{ static int depth_of() */
/*
 * How many slots a port has **room for**, which is not what
 * `cera_map_in_port_depth` answers — that says how many values are
 * waiting in it. Both are called depth in ordinary speech and they are
 * different numbers; this file is about the first.
 */
static int depth_of(cera_map_t *m, int station, int port)
{
    cera_station_t *s = cera_map_station(m, station);
    return atomic_load(&s->in_ports[port].capacity);
}
/* }}} */

/* {{{ static void a_plain_station_passes_the_whole_backlog() */
/*
 * **Fan-out duplicates a value; it does not divide it.** One value
 * leaving a plain station arrives at every wire on its output port, so
 * a station that can fall a hundred behind feeds two stations that can
 * each fall a hundred behind.
 *
 * Halving it across the two destinations would be the intuitive
 * arithmetic and the wrong one, which is why both are checked rather
 * than one.
 */
static void a_plain_station_passes_the_whole_backlog(void)
{
    cera_map_t *m = cera_map_create(4);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 2, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 3, "keep", CERA_STATION_PLAIN);

    must_take(cera_map_wire(m, 0, 0, 1, 0), "the first hop");
    must_take(cera_map_wire(m, 1, 0, 2, 0), "the second hop");
    must_take(cera_map_wire(m, 1, 0, 3, 0), "a fan-out of the second");

    check(depth_of(m, 1, 0) == CERA_IN_PORT_DEFAULT_CAPACITY,
          "before anything is declared, every port is at the default");

    must_take(cera_map_in_port_start_depth(m, 0, 0, 100),
              "a hundred slots on the first port");

    check(depth_of(m, 0, 0) == 100, "the port that was asked for is deep");
    check(depth_of(m, 1, 0) >= 100, "and so is the station below it");
    check(depth_of(m, 2, 0) >= 100,
          "and the one below that, two hops from where it was said");
    check(depth_of(m, 3, 0) >= 100,
          "and the other side of the fan-out, because fan-out copies a "
          "value rather than sharing it out");

    cera_map_destroy(m);
    printf("  a depth declared once reached two hops down and both sides "
           "of a fan-out\n");
}
/* }}} */

/* {{{ static void an_iterator_divides_it() */
/*
 * **An iterator takes its exits in turn**, so a hundred values through
 * three exits is about thirty-four each rather than a hundred each.
 *
 * Rounded up, and the remainder rides on the exits the cursor reaches
 * first — which is every exit here, because rounding up is not worth a
 * separate answer per exit for one slot.
 */
static void an_iterator_divides_it(void)
{
    cera_map_t *m = cera_map_create(5);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_ITERATOR);
    cera_map_place_box(m, 2, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 3, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 4, "keep", CERA_STATION_PLAIN);

    must_take(cera_map_wire(m, 0, 0, 1, 0), "into the iterator");
    must_take(cera_map_wire(m, 1, 0, 2, 0), "its first exit");
    must_take(cera_map_wire(m, 1, 1, 3, 0), "its second");
    must_take(cera_map_wire(m, 1, 2, 4, 0), "its third");

    must_take(cera_map_in_port_start_depth(m, 0, 0, 99),
              "ninety-nine slots on the first port");

    check(depth_of(m, 1, 0) >= 99, "the iterator itself takes all of it");
    for (int at = 2; at <= 4; at++) {
        check(depth_of(m, at, 0) >= 33,
              "each exit is sized for its share");
        check(depth_of(m, at, 0) < 99,
              "and not for the whole backlog, which is the point of a "
              "round robin");
    }

    cera_map_destroy(m);
    printf("  an iterator gave each of three exits a third of the "
           "backlog\n");
}
/* }}} */

/* {{{ static void a_comparator_stops_it() */
/*
 * **A comparator chooses one of three exits by the data**, so any of
 * them could take everything and none of them can be sized honestly.
 *
 * The two answers available are to size all three for the whole backlog
 * — three times the memory for a guess — or to size none and let
 * whichever turns out busy grow a page at a time. The cheaper mistake
 * is the one that only costs time, so the walk stops here.
 *
 * The comparator's own port is still sized, because that is upstream of
 * the decision and nothing about it is in doubt.
 */
static void a_comparator_stops_it(void)
{
    cera_map_t *m = cera_map_create(4);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_COMPARATOR);
    cera_map_place_box(m, 2, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 3, "keep", CERA_STATION_PLAIN);

    must_take(cera_map_configure_port(m, 1, 1, CERA_IN_PORT_STATIC, "5"),
              "the threshold");
    must_take(cera_map_wire(m, 0, 0, 1, 0), "into the comparator");
    must_take(cera_map_wire(m, 1, 0, 2, 0), "its less-than exit");
    must_take(cera_map_wire(m, 1, 2, 3, 0), "its greater-than exit");

    must_take(cera_map_in_port_start_depth(m, 0, 0, 100),
              "a hundred slots on the first port");

    check(depth_of(m, 1, 0) >= 100,
          "the comparator's own port is sized — the doubt is about which "
          "exit, not about what arrives");
    check(depth_of(m, 2, 0) == CERA_IN_PORT_DEFAULT_CAPACITY,
          "and its less-than branch is left at the default");
    check(depth_of(m, 3, 0) == CERA_IN_PORT_DEFAULT_CAPACITY,
          "and so is its greater-than branch");

    cera_map_destroy(m);
    printf("  a comparator took the depth and passed none of it on\n");
}
/* }}} */

/* {{{ static void an_uneven_join_takes_the_smaller() */
/*
 * **A station runs when every input port holds a value**, so one fed a
 * hundred from one side and ten from the other runs ten times and
 * produces ten.
 *
 * Passing on the larger would size everything below an uneven join for
 * a backlog that cannot arrive. This is the scene that tells the
 * minimum from the maximum, and it is the one an author is least likely
 * to notice going wrong, because over-sizing costs only memory and
 * nothing ever says so.
 */
static void an_uneven_join_takes_the_smaller(void)
{
    cera_map_t *m = cera_map_create(4);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 2, "add", CERA_STATION_PLAIN);   /* two ports */
    cera_map_place_box(m, 3, "keep", CERA_STATION_PLAIN);

    must_take(cera_map_wire(m, 0, 0, 2, 0), "the deep side");
    must_take(cera_map_wire(m, 1, 0, 2, 1), "the shallow side");
    must_take(cera_map_wire(m, 2, 0, 3, 0), "out of the join");

    must_take(cera_map_in_port_start_depth(m, 0, 0, 100), "the deep side");
    must_take(cera_map_in_port_start_depth(m, 1, 0, 20), "the shallow side");

    check(depth_of(m, 2, 0) >= 100, "the join's deep port took a hundred");
    check(depth_of(m, 2, 1) >= 20, "and its shallow port took twenty");
    check(depth_of(m, 3, 0) < 100,
          "and what the join feeds is sized for what the join can "
          "produce, which is the smaller of the two");

    cera_map_destroy(m);
    printf("  an uneven join passed on the smaller of its two sides\n");
}
/* }}} */

/* {{{ static void a_cycle_ends() */
/*
 * **A station whose output feeds its own input** — an accumulator,
 * which is how anything remembers anything here, because a box may not.
 *
 * The walk arrives back where it started. What stops it is not a
 * visited set or a depth limit but the same test it does everywhere:
 * a port already deep enough is left alone, and the second visit finds
 * the port at the size the first visit gave it.
 *
 * If that were wrong this scene would not fail — it would never
 * return.
 */
static void a_cycle_ends(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "add", CERA_STATION_PLAIN);

    must_take(cera_map_wire(m, 0, 0, 1, 0), "the argument");
    must_take(cera_map_wire(m, 1, 0, 1, 1), "the loop back into itself");

    must_take(cera_map_in_port_start_depth(m, 0, 0, 64),
              "sixty-four slots at the head of a loop");

    check(depth_of(m, 1, 0) >= 64, "the loop's argument port is deep");
    check(depth_of(m, 1, 1) >= 1,
          "and so is the port its own output feeds, without the walk "
          "going round forever");

    cera_map_destroy(m);
    printf("  a station wired to itself was sized and the walk ended\n");
}
/* }}} */

/* {{{ static void a_later_wire_carries_it() */
/*
 * **The depth was declared before this station existed**, which is the
 * case a load-time-only pass would miss.
 *
 * A program that grows while it runs draws wires long after the file
 * that started it was read. Carrying the backlog on the wire rather
 * than on the reading is what makes a station added at minute ten as
 * deep as one added at minute zero.
 *
 * It is also what makes a *file* size itself, at no extra cost: loading
 * draws every wire it names, so the same rule covers both without a
 * second mechanism.
 */
static void a_later_wire_carries_it(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_in_port_start_depth(m, 0, 0, 128),
              "a deep port, with nothing wired below it yet");

    /* And now, afterwards, somewhere for its values to go. */
    cera_map_place_box(m, 1, "keep", CERA_STATION_PLAIN);
    check(depth_of(m, 1, 0) == CERA_IN_PORT_DEFAULT_CAPACITY,
          "a station placed after the declaration starts at the default");

    must_take(cera_map_wire(m, 0, 0, 1, 0), "the wire drawn afterwards");
    check(depth_of(m, 1, 0) >= 128,
          "and the wire brought the depth with it");

    cera_map_destroy(m);
    printf("  a wire drawn after the depth was declared carried it "
           "anyway\n");
}
/* }}} */

/* {{{ static void the_default_stays_the_default() */
/*
 * **Two fresh stations wired together change nothing**, which is what
 * makes this affordable: the propagation runs on every wire ever drawn,
 * and in the ordinary case it compares two numbers and stops.
 *
 * A rule that quietly deepened every buffer in every program would be a
 * memory cost nobody asked for and nobody could see.
 */
static void the_default_stays_the_default(void)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "keep", CERA_STATION_PLAIN);
    cera_map_place_box(m, 1, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_wire(m, 0, 0, 1, 0), "an ordinary wire");

    check(depth_of(m, 1, 0) == CERA_IN_PORT_DEFAULT_CAPACITY,
          "an ordinary wire between two ordinary stations leaves both at "
          "the default");

    cera_map_destroy(m);
    printf("  wiring two ordinary stations deepened nothing\n");
}
/* }}} */

int main(void)
{
    a_plain_station_passes_the_whole_backlog();
    an_iterator_divides_it();
    a_comparator_stops_it();
    an_uneven_join_takes_the_smaller();
    a_cycle_ends();
    a_later_wire_carries_it();
    the_default_stays_the_default();

    if (failures) {
        fprintf(stderr, "%d depth checks failed\n", failures);
        return 1;
    }
    return 0;
}
