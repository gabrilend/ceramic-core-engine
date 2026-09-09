/*
 * 087-test-parts-and-programs.c — one table, one receipt, one operation
 * (issues 217a, 212a).
 *
 * What this is: the proof that placing a box and placing a map are the
 * same act, that everything is wirable to everything, and that a
 * program ends by pruning the stations it named.
 *
 * What it replaced. This file used to prove that a program *started
 * beside* another could not be wired to it. That restriction was real
 * and its reason was sound — a wire is a station index and an index
 * only means something inside one table, so two stations can be wired
 * exactly when they share a table. The trouble was what a programmer
 * met: two boxes in front of them and an engine saying these two
 * cannot be connected, for a reason living in its memory layout rather
 * than in anything about their program.
 *
 * So the table is one table and the restriction is gone. What it
 * bought — a second thing you can end without touching the first — is
 * now what pruning a receipt does, and this file proves that instead.
 *
 * How it does it, in general terms: a box and a map are placed through
 * the same call and wired together by a caller that never learns which
 * is which; two copies of one description are placed into one program
 * and wired to *each other*, which the old rule forbade; and one of
 * them is ended mid-run while the other keeps working.
 */
#include "cera.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* {{{ static void write_text() */
static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}
/* }}} */

/* {{{ static void a_box_and_a_map_are_one_operation() */
/*
 * **The caller cannot tell which kind it placed** (issue 217a), and
 * that is the whole claim.
 *
 * A box's doors are its ports: its argument N is input port N and its
 * result zero is its output port, because a box's ports are already
 * numbered and marking them would be writing down what counting
 * already says. A map's doors are its marks, because a map's ports are
 * scattered across several stations and nothing about their position
 * says which argument is which.
 *
 * Those are not two rules with a fallback between them. They are one —
 * *the doors are wherever the description put them* — and a
 * description of one station puts them on that station.
 */
static void a_box_and_a_map_are_one_operation(const char *doubler_path)
{
    cera_map_t *m = cera_map_create_empty();

    /* A box and a map, through the same call, into the same table. */
    int source = -1, doubling = -1, answer = -1;
    must_take(cera_map_add_part(m, "seven", &source), "a box as a part");
    must_take(cera_map_add_part(m, doubler_path, &doubling), "a map as a part");
    must_take(cera_map_add_part(m, "keep", &answer), "a second box");

    /* And wired by the same call, in the same way, three times. Nothing
     * here says which of the three is the map. */
    must_take(cera_map_join(m, source, 0, doubling, 0), "box into map");
    must_take(cera_map_join(m, doubling, 0, answer, 0), "map into box");

    /* The last box's output is the program's, found through its own
     * receipt rather than by knowing where it landed. */
    int at = -1, port = -1;
    check(cera_map_part_door(m, answer, 0, 0, &at, &port),
          "a box's result zero is its output port, with nothing marked");
    must_take(cera_map_designate_result(m, at, port, 0), "the way out");

    cera_map_start(m, 2);
    must_take(cera_map_bring_up(m), "the composed program");

    int landed[4] = { 0 };
    must_take(cera_map_collect(m, at, port, landed, 4, (int)sizeof landed[0]),
              "somewhere to put the answer");

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    check(cera_map_collected(m, at, port) == 1 && landed[0] == 14,
          "seven from a box, doubled by a map, kept by a box — through "
          "one placing call and one wiring call");

    cera_map_destroy(m);
    printf("  a box and a map were placed and wired by a caller that "
           "could not tell them apart\n");
}
/* }}} */

/* {{{ static void two_programs_share_one_table() */
/*
 * **Everything is wirable to everything** (issue 212a), which the old
 * rule forbade outright.
 *
 * Two copies of one description, placed into one program and wired to
 * each other. Under starting-beside these were two tables and the wire
 * between them could not be drawn at all; the caller reached the second
 * only through its doors, and a wire naming the second's index drew an
 * ordinary wire at home instead.
 */
static void two_programs_share_one_table(const char *doubler_path)
{
    cera_map_t *m = cera_map_create_empty();

    int first = -1, second = -1, feed = -1, answer = -1;
    must_take(cera_map_add_part(m, "keep", &feed), "the feed");
    must_take(cera_map_add_part(m, doubler_path, &first), "the first copy");
    must_take(cera_map_add_part(m, doubler_path, &second), "the second copy");
    must_take(cera_map_add_part(m, "keep", &answer), "the answer");

    /* The wire the old rule refused: one placed description straight
     * into another. */
    must_take(cera_map_join(m, feed, 0, first, 0), "into the first");
    must_take(cera_map_join(m, first, 0, second, 0),
              "the wire between two separately placed descriptions");
    must_take(cera_map_join(m, second, 0, answer, 0), "out of the second");

    int in_at = -1, in_port = -1, out_at = -1, out_port = -1;
    check(cera_map_part_door(m, feed, 0, 1, &in_at, &in_port),
          "the feed's argument zero is its first port");
    check(cera_map_part_door(m, answer, 0, 0, &out_at, &out_port),
          "the answer's result zero is its output port");
    must_take(cera_map_designate_argument(m, in_at, in_port, 0), "the way in");
    must_take(cera_map_designate_result(m, out_at, out_port, 0), "the way out");

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");

    int landed[8] = { 0 };
    must_take(cera_map_collect(m, out_at, out_port, landed, 8,
                               (int)sizeof landed[0]),
              "somewhere to put the answers");
    cera_pool_release(m->pool);

    int seven = 7;
    must_take(cera_map_deliver_argument(m, in_at, in_port, &seven,
                                        sizeof seven),
              "a value");

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(cera_map_collected(m, out_at, out_port) == 1 && landed[0] == 28,
          "seven doubled by each of two separately placed copies came "
          "back as twenty-eight");

    cera_map_destroy(m);
    printf("  two placed descriptions were wired to each other, which the "
           "old rule forbade\n");
}
/* }}} */

/* {{{ static void a_program_ends_by_being_pruned() */
/*
 * **Ending a program is pruning its stations** (issue 212a), which is
 * what the receipt was kept for.
 *
 * What starting-beside offered was a second thing you could end without
 * touching the first. This is that, without the restriction that came
 * with it: the part that ends is wired to the part that stays, and
 * ending it cuts those wires like any others.
 */
static void a_program_ends_by_being_pruned(const char *doubler_path)
{
    cera_map_t *m = cera_map_create_empty();

    int feed = -1, going = -1, staying = -1;
    must_take(cera_map_add_part(m, "keep", &feed), "the feed");
    must_take(cera_map_add_part(m, doubler_path, &going), "the part that goes");
    must_take(cera_map_add_part(m, "keep", &staying), "the part that stays");

    must_take(cera_map_join(m, feed, 0, going, 0), "into the doomed part");
    must_take(cera_map_join(m, feed, 0, staying, 0), "into the one that stays");

    /*
     * The doomed part's own result goes somewhere, because **a sub-map
     * brings its door marks with it** and an unwired one becomes a way
     * out of the program that placed it. That is right — an unwired
     * marked port is a way out from outside, whoever put it there — and
     * it is the rough edge a caller meets: place a map, ignore its
     * result, and you have a door you did not ask for. Wiring it to a
     * sink says what was meant.
     */
    int sink = -1;
    must_take(cera_map_add_part(m, "swallow", &sink), "a sink");
    must_take(cera_map_join(m, going, 0, sink, 0), "the doomed part's output");

    int in_at = -1, in_port = -1, stay_at = -1, stay_port = -1;
    check(cera_map_part_door(m, feed, 0, 1, &in_at, &in_port), "the way in");
    check(cera_map_part_door(m, staying, 0, 0, &stay_at, &stay_port),
          "the survivor's way out");
    must_take(cera_map_designate_argument(m, in_at, in_port, 0), "the way in");
    must_take(cera_map_designate_result(m, stay_at, stay_port, 0),
              "the way out");

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");

    int landed[16] = { 0 };
    must_take(cera_map_collect(m, stay_at, stay_port, landed, 16,
                               (int)sizeof landed[0]),
              "somewhere to put the survivor's answers");
    cera_pool_release(m->pool);

    for (int i = 1; i <= 4; i++)
        must_take(cera_map_deliver_argument(m, in_at, in_port, &i, sizeof i),
                  "a value before the pruning");
    while (cera_map_collected(m, stay_at, stay_port) < 4)
        usleep(200);

    /* ---- and now one of them ends ---- */
    must_take(cera_map_end_part(m, going), "ending the doomed part");
    check(cera_map_end_part(m, going) != NULL,
          "ending it twice is refused rather than quietly doing nothing");

    for (int i = 5; i <= 8; i++)
        must_take(cera_map_deliver_argument(m, in_at, in_port, &i, sizeof i),
                  "a value after the pruning");

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(cera_map_collected(m, stay_at, stay_port) == 8,
          "the part that stayed received everything, before and after the "
          "other one ended");

    int total = 0;
    for (int i = 0; i < 8; i++)
        total += landed[i];
    check(total == 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8,
          "and the values are the ones that were sent");

    cera_map_destroy(m);
    printf("  one part ended while the part it was wired beside kept "
           "working\n");
}
/* }}} */

int main(void)
{
    const char *dir = "/dev/shm/minimal-soramech";
    char path[320];
    snprintf(path, sizeof path, "%s/doubler-part.map", dir);

    /* A description of one station: a way in, a doubling, a way out.
     * Its doors are marked, because a map's ports are scattered and
     * nothing about their position says which argument is which. */
    write_text(path,
        "station twice double_it p\n"
        "  in 0 $0\n"
        "  out 0 $0\n");

    a_box_and_a_map_are_one_operation(path);
    two_programs_share_one_table(path);
    a_program_ends_by_being_pruned(path);

    if (failures) {
        fprintf(stderr, "%d composition checks failed\n", failures);
        return 1;
    }
    return 0;
}
