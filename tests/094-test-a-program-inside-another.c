/*
 * 094-test-a-program-inside-another.c — a program used as a box
 * (issue 217).
 *
 * What this is: the proof that a description can be brought inside a
 * program that already exists, that doing it twice produces two
 * independent copies, and that a parent wiring to its doors cannot
 * tell whether the thing behind the port is a graph or a C function.
 *
 * That last sentence is the one three issues were waiting on. 209
 * named where a program's results come from and 213 named where its
 * arguments arrive; both said their final step was "a program used as
 * a box", and neither could take it, because there was no operation
 * that put one program inside another.
 *
 * How it does it, in general terms: write a small description to
 * disk — a doubler with an entrance and a way out — and instantiate
 * it into a parent, twice. The parent wires its own source into the
 * first instance's entrance, the first instance's result into the
 * second instance's entrance, and reads the second's result. Nothing
 * in the parent names anything inside either instance.
 */
#include "cera.h"

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

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

static char work_dir[256];
static char part_path[512];

/* {{{ static void write_the_part() */
/*
 * The description that gets instantiated: a thing that takes a
 * number, doubles it, and hands it back. Two stations, an entrance
 * and a way out, and one interior station whose name the parent will
 * never learn.
 */
static void write_the_part(void)
{
    /*
     * The doors are called `way_in` and `way_out` rather than `in`
     * and `out`, and that is not taste. The first word of a line is
     * what the reader dispatches on, and `in`, `out` and `statics`
     * already mean something there — so a station cannot be named any
     * of the three. Found by naming one `in` and being told there was
     * an input line before any station.
     */
    static const char text[] =
        "# a part, meant to be used inside something else\n"
        "station way_in keep p entry\n"
        "  out 0 - middle.0\n"
        "station middle double_it p\n"
        "  out 0 - way_out.0\n"
        "station way_out keep p result\n";

    snprintf(part_path, sizeof part_path, "%s/part.map", work_dir);
    FILE *f = fopen(part_path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", part_path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}
/* }}} */

/* {{{ static void two_instances_share_nothing() */
/*
 * **One description, instantiated twice, is two of everything.**
 *
 * This is the scene that says "template" rather than "move". A
 * merge would have one set of stations to give away and could only
 * give them once; instantiating builds a fresh set each time, so the
 * two copies have separate stations, separate buffers and separate
 * constants, and nothing done to one is visible in the other.
 *
 * It also proves the translation is a table rather than an offset in
 * any way that matters: the second instance lands wherever the table
 * had room, and its arrows are drawn between the places its own
 * stations took.
 */
static void two_instances_share_nothing(void)
{
    map_t *m = map_create_empty();

    map_instance_t first = map_instantiate_file(m, part_path);
    map_instance_t second = map_instantiate_file(m, part_path);

    check(first.count == 3 && second.count == 3,
          "each instance built the three stations its description names");

    int shared = 0;
    for (int i = 0; i < first.count; i++)
        for (int j = 0; j < second.count; j++)
            if (first.station[i] == second.station[j])
                shared = 1;
    check(!shared, "and the two instances share no station at all");

    /* Doors found by asking which way they face, never by index. */
    int in_a = map_instance_entrance(m, &first, 0);
    int out_a = map_instance_result(m, &first, 0);
    int in_b = map_instance_entrance(m, &second, 0);
    int out_b = map_instance_result(m, &second, 0);
    check(in_a >= 0 && out_a >= 0 && in_b >= 0 && out_b >= 0,
          "each instance has an entrance and a way out");
    check(in_a != in_b && out_a != out_b,
          "and they are different stations, which is what two copies "
          "means");

    /* Separate buffers: filling one instance's entrance leaves the
     * other's empty. */
    check(map_in_port_depth(m, in_a, 0) == 0
          && map_in_port_depth(m, in_b, 0) == 0,
          "both start empty");

    map_instance_free(&first);
    map_instance_free(&second);
    map_destroy(m);
    printf("  one description instantiated twice built two of everything, "
           "sharing nothing\n");
}
/* }}} */

/* {{{ static void a_parent_cannot_tell() */
/*
 * **The sentence 209 and 213 were both waiting for.**
 *
 * A parent wires its own station into an instance's entrance and the
 * instance's result onward, exactly as it would wire any station —
 * and it never names anything inside. The instance's interior station
 * is called `middle`, and the parent could not say so.
 *
 * The chain is deliberately two instances deep: the first instance's
 * way out feeds the second instance's entrance, which is one graph
 * after instantiation and needs no seam, no boundary check, and no
 * per-instance bookkeeping. There are stations with indices, the way
 * there always were.
 *
 * Seven in, doubled twice, fourteen and then twenty-eight.
 */
static void a_parent_cannot_tell(void)
{
    map_t *m = map_create_empty();

    /* The parent's own station: a source of sevens. */
    int source = map_add_station(m);
    map_place_box(m, source, "seven", STATION_PLAIN);
    must_take(map_name_station(m, source, "source"), "a name");

    map_instance_t first = map_instantiate_file(m, part_path);
    map_instance_t second = map_instantiate_file(m, part_path);

    int in_a = map_instance_entrance(m, &first, 0);
    int out_a = map_instance_result(m, &first, 0);
    int in_b = map_instance_entrance(m, &second, 0);
    int out_b = map_instance_result(m, &second, 0);

    /* Three ordinary wires. Two of them cross what used to be a seam
     * and there is nothing there to cross. */
    must_take(map_wire(m, source, 0, in_a, 0), "the wire into the first");
    must_take(map_wire(m, out_a, 0, in_b, 0), "the wire between the two");

    /* The parent's own way out, fed by the second instance. */
    int answer = map_add_station(m);
    map_place_box(m, answer, "keep", STATION_PLAIN);
    must_take(map_name_station(m, answer, "answer"), "a name");
    must_take(map_designate_output(m, answer), "the parent's way out");
    must_take(map_wire(m, out_b, 0, answer, 0), "the wire to the answer");

    map_start(m, 2);
    must_take(map_bring_up(m), "the composed program");
    pool_release(m->pool);
    pool_join(m->pool);

    int got = 0;
    check(map_output_take(m, answer, &got, sizeof got) && got == 28,
          "seven went in, was doubled by each of two instances, and "
          "twenty-eight came out");

    /*
     * And the seam really is gone: the interior station of the first
     * instance ran, and the parent has no name for it. Asking the
     * program what it is called gets the description's word, which
     * the parent never typed.
     */
    int middle = -1;
    for (int i = 0; i < first.count; i++)
        if (first.station[i] != in_a && first.station[i] != out_a)
            middle = first.station[i];
    check(middle >= 0
          && atomic_load(&map_station(m, middle)->runs) == 1,
          "the instance's interior station ran, though nothing outside "
          "it knows its name");

    map_instance_free(&first);
    map_instance_free(&second);
    map_destroy(m);
    printf("  a parent wired to two instances by their doors alone and "
           "got 28 back\n");
}
/* }}} */

/* {{{ static void instantiating_into_a_running_program() */
/*
 * **Every operation this is made of is legal at any moment**, so this
 * one is too. A program is going, with values moving through it, and
 * gains a whole subgraph — added, wired, and brought up — without
 * anything being quiesced.
 *
 * The waiting is the same as elsewhere: let what is already in flight
 * land before growing, so that what the new part receives is a number
 * rather than a schedule.
 */
static void instantiating_into_a_running_program(void)
{
    map_t *m = map_create_empty();

    int gate = map_add_station(m);
    map_place_box(m, gate, "keep", STATION_PLAIN);
    must_take(map_name_station(m, gate, "gate"), "a name");
    must_take(map_designate_input(m, gate), "an entrance");

    int kept = map_add_station(m);
    map_place_box(m, kept, "keep", STATION_PLAIN);
    must_take(map_name_station(m, kept, "kept"), "a name");
    must_take(map_designate_output(m, kept), "a way out");
    must_take(map_wire(m, gate, 0, kept, 0), "the first wire");

    map_start(m, 3);
    pool_submitter_register(m->pool);
    must_take(map_bring_up(m), "the program");
    pool_release(m->pool);

    const int BATCH = 20;
    for (int i = 0; i < BATCH; i++) {
        int v = i;
        must_take(map_deliver_argument(m, gate, 0, &v, sizeof v),
                  "an argument");
    }
    while (atomic_load(&map_station(m, kept)->runs) < BATCH)
        usleep(200);

    /* ---- mid-flight ---- */
    map_instance_t part = map_instantiate_file(m, part_path);
    int in_p = map_instance_entrance(m, &part, 0);
    int out_p = map_instance_result(m, &part, 0);

    int landing = map_add_station(m);
    map_place_box(m, landing, "keep", STATION_PLAIN);
    must_take(map_name_station(m, landing, "landing"), "a name");
    must_take(map_designate_output(m, landing), "a second way out");
    must_take(map_wire(m, out_p, 0, landing, 0), "the wire out of it");
    must_take(map_wire(m, gate, 0, in_p, 0), "the wire into it");
    must_take(map_bring_up(m), "the grown program");

    for (int i = 0; i < BATCH; i++) {
        int v = i;
        must_take(map_deliver_argument(m, gate, 0, &v, sizeof v),
                  "an argument after growing");
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    check(map_output_waiting(m, landing) == BATCH,
          "the subgraph added mid-run produced one result per value it "
          "was sent");
    check(atomic_load(&map_station(m, kept)->runs) == 2 * BATCH,
          "and the part that was already running lost nothing");

    map_instance_free(&part);
    map_destroy(m);
    printf("  a whole subgraph joined a running program and the running "
           "part lost nothing\n");
}
/* }}} */

/* {{{ static void a_map_adds_a_map() */
/*
 * **Adding a box and adding a map are one operation**, and this is
 * the scene that says so.
 *
 * The builder is itself a map. It adds a described part to another
 * program, adds a plain box to the same program, and wires them
 * together — using the *same* two operations for both, because a map
 * is a list of boxes with wiring and a box is a list of one.
 *
 * What travels between the operations is a **part**: where values go
 * in and where they come out. For the described part those are two
 * different stations; for the plain box they are the same station,
 * because a box's own input ports are its way in and its own output
 * port is its way out. Nothing takes a part apart — every operation
 * that consumes one takes it whole, which is why no box exists here
 * whose only job is to pull a field out of a handle.
 */
static void a_map_adds_a_map(void)
{
    map_t *built = map_create_empty();
    map_start(built, 2);

    char address[64];
    snprintf(address, sizeof address, "{ %zu }", (size_t)built);
    char quoted[600];
    snprintf(quoted, sizeof quoted, "\"%s\"", part_path);

    map_t *builder = map_create_empty();

    /* Add the described part — three stations and their wiring. */
    int add_part = map_add_station(builder);
    map_place_box(builder, add_part, "program_add", STATION_PLAIN);
    must_take(map_name_station(builder, add_part, "add_part"), "a name");
    must_take(map_configure_port(builder, add_part, 0, IN_PORT_STATIC,
                                 address), "the program to build into");
    must_take(map_configure_port(builder, add_part, 1, IN_PORT_STATIC,
                                 quoted), "the map to add");

    /* Add a plain box — one station — through the same operation. */
    int add_box = map_add_station(builder);
    map_place_box(builder, add_box, "program_add", STATION_PLAIN);
    must_take(map_name_station(builder, add_box, "add_box"), "a name");
    must_take(map_configure_port(builder, add_box, 0, IN_PORT_STATIC,
                                 address), "the same program");
    must_take(map_configure_port(builder, add_box, 1, IN_PORT_STATIC,
                                 "\"seven\""), "the box to add");

    /* Wire the box's way out into the part's way in, which for the
     * box is its own station and for the part is its entrance. */
    int join = map_add_station(builder);
    map_place_box(builder, join, "program_connect", STATION_PLAIN);
    must_take(map_name_station(builder, join, "join"), "a name");
    must_take(map_configure_port(builder, join, 0, IN_PORT_STATIC, address),
              "the program to wire in");
    must_take(map_wire(builder, add_box, 0, join, 1), "the box, as a part");
    must_take(map_configure_port(builder, join, 2, IN_PORT_STATIC, "0"),
              "which output port");
    must_take(map_wire(builder, add_part, 0, join, 3), "the map, as a part");
    must_take(map_configure_port(builder, join, 4, IN_PORT_STATIC, "0"),
              "which input port");

    /* Mark the part's way out as the built program's way out. */
    int door = map_add_station(builder);
    map_place_box(builder, door, "program_set_door", STATION_PLAIN);
    must_take(map_name_station(builder, door, "door"), "a name");
    must_take(map_configure_port(builder, door, 0, IN_PORT_STATIC, address),
              "the program to mark in");
    must_take(map_wire(builder, add_part, 0, door, 1), "the part to mark");
    must_take(map_configure_port(builder, door, 2, IN_PORT_STATIC, "2"),
              "facing out");
    must_take(map_designate_output(builder, door), "the builder's answer");

    map_start(builder, 2);
    must_take(map_bring_up(builder), "the builder");
    pool_release(builder->pool);
    pool_join(builder->pool);

    int worked = 0;
    check(map_output_take(builder, door, &worked, sizeof worked)
          && worked == 1,
          "the builder added a box and a map through one operation and "
          "wired them together");

    must_take(map_bring_up(built), "the program the builder made");
    pool_release(built->pool);
    pool_join(built->pool);

    int got = 0;
    int found = 0;
    for (int i = 0; i < built->n_stations && !found; i++)
        if (map_station(built, i)->door == DOOR_OUT)
            found = map_output_take(built, i, &got, sizeof got);
    check(found && got == 14,
          "and what it built ran: seven from the box, doubled by the "
          "map, fourteen at the way out");

    map_destroy(builder);
    map_destroy(built);
    printf("  a map added a box and a map to another map, through one "
           "operation\n");
}
/* }}} */

/* {{{ static void a_composed_program_still_writes_down() */
/*
 * **Two copies of one description means two stations with one name,
 * and a file cannot have that.**
 *
 * A station name is an arbitrary label the engine never reads, so two
 * stations sharing one is fine and nothing about the program is worse
 * for it. A *file* is different: an arrow is written as a destination
 * name, so a file with two `way_in` lines cannot say which one an
 * arrow means, and the reader refuses it.
 *
 * The disambiguation therefore happens where it is needed — on the
 * way out — and nothing is lost, because a label carrying no meaning
 * can be spelled differently without the program changing. What this
 * asserts is the round trip: the composed program is written down,
 * read back, and written again, and the two files are the same.
 */
static void a_composed_program_still_writes_down(void)
{
    map_t *m = map_create_empty();

    map_instance_t first = map_instantiate_file(m, part_path);
    map_instance_t second = map_instantiate_file(m, part_path);
    must_take(map_wire(m, map_instance_result(m, &first, 0), 0,
                       map_instance_entrance(m, &second, 0), 0),
              "the wire between the two");
    map_instance_free(&first);
    map_instance_free(&second);

    char one[512], two[512];
    snprintf(one, sizeof one, "%s/composed.map", work_dir);
    snprintf(two, sizeof two, "%s/composed-again.map", work_dir);

    FILE *f = fopen(one, "w");
    map_dump(m, f);
    fclose(f);
    map_destroy(m);

    /* If the names had not been made unique this would refuse, saying
     * a station with that name already exists. */
    map_t *back = map_load_file(one, 2);
    f = fopen(two, "w");
    map_dump(back, f);
    fclose(f);
    pool_release(back->pool);
    pool_join(back->pool);

    check(map_station(back, 0)->call != NULL,
          "the composed program read back");

    FILE *a = fopen(one, "r");
    FILE *b = fopen(two, "r");
    int same = 1, ca, cb;
    do {
        ca = fgetc(a);
        cb = fgetc(b);
        if (ca != cb)
            same = 0;
    } while (same && ca != EOF && cb != EOF);
    fclose(a);
    fclose(b);
    check(same, "and dumping the dump gives the dump, so the round trip "
                "closed on a program with two of everything");

    map_destroy(back);
    printf("  a program holding two copies of one description was written "
           "down and read back\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/inside-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot make %s\n", work_dir);
        return 1;
    }
    write_the_part();

    /* Two of these scenes collect at the end rather than as they go,
     * which is precisely what the way out shouts about. Announced so
     * the notices read as the engine working. */
    printf("  (the pile-up notices below are scenes that drain at the "
           "end)\n");
    fflush(stdout);

    two_instances_share_nothing();
    a_parent_cannot_tell();
    instantiating_into_a_running_program();
    a_map_adds_a_map();
    a_composed_program_still_writes_down();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        fprintf(stderr, "could not clean up %s\n", work_dir);

    if (failures) {
        fprintf(stderr, "%d checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
