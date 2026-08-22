/*
 * 082-test-one-construction-path.c — a program read from a file and a
 * program built by calling the surface are the same program (issue
 * 212).
 *
 * What this is: the proof that there is **one** way structure comes
 * into existence, rather than two that agree by coincidence and drift
 * the first time somebody changes one of them.
 *
 * Reading a file used to be able to do things nothing else could. It
 * counted the stations before allocating, it validated in a phase
 * only it could enter, and it seeded from inside that phase — a state
 * called *still loading* that no other caller could be in. Each of
 * those has been taken out and made an ordinary operation, and the
 * claim is that what remains of reading a file is a reader: each line
 * becomes calls anybody could make.
 *
 * How it does it, in general terms: write a small map to disk and
 * load it; build the same program by calling the surface; dump both
 * and compare the text byte for byte. Comparing dumps is the cheapest
 * proof available, because the dump already walks the live station
 * table and writes what is actually there — so two identical dumps
 * are two identical station tables, including the things a
 * hand-written comparison would forget to check.
 *
 * **Not equivalent. The same.** If the two paths ever diverge — a
 * default applied on one side only, a port left in a different state,
 * an arrow attached in a different order — the text differs and this
 * fails, naming the first line where they part.
 */
#include "018-station.h"
#include "026-registry.h"
#include "040-mapfile.h"
#include "049-observe.h"
#include "073-latebox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

/* {{{ static char *slurp() */
static char *slurp(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    static char buf[2][8192];
    static int which;
    char *out = buf[which++ % 2];
    size_t n = fread(out, 1, 8191, f);
    out[n] = '\0';
    fclose(f);
    return out;
}
/* }}} */

/* {{{ static void dump_to() */
static void dump_to(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    map_dump(m, f);
    fclose(f);
}
/* }}} */

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "the surface refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ static int a_station_joins_a_running_program() */
/*
 * **The other half of the claim**: the same operations, on a program
 * whose workers are in flight.
 *
 * The scene above proves that reading a file and calling the surface
 * build the same program. It proves it at rest — both programs are
 * assembled, then started, then compared. That leaves the more
 * interesting sentence untested: every one of these operations is
 * legal *at any moment*, because there is no state called "still
 * loading" for anything to be in.
 *
 * So this one runs the sequence a workbench would run. A program is
 * going, with values moving through it. A station is added, its ports
 * are given sources, it is wired in, and the program is brought up
 * again. It takes values and produces them; the part that was already
 * running is undisturbed and loses nothing.
 *
 * **The order is add, configure, wire, and the order is the point.**
 * A station comes into existence with somewhere to be and nothing to
 * do, and that is a state it may hold indefinitely — it cannot become
 * ready, so no worker can pick it up, so there is no window of
 * invalidity to guard. Configuring gives its ports sources; wiring is
 * the last step and is what lets the first value arrive. Nothing here
 * is quiesced, paused, or locked out for the duration.
 *
 * What is asserted at the end is three things: the new station ran
 * once per value it was sent, the results it produced are the right
 * numbers rather than merely the right count, and the station that
 * was already there ran exactly as many times as it was fed. That
 * last one is the "undisturbed" half, and it is the one worth having:
 * growing a program that is running must not cost the running program
 * a single value.
 */
static int a_station_joins_a_running_program(void)
{
    map_t *m = map_create_empty();

    /* The part that is already going: values in, doubled, kept. */
    int doubler = map_add_station(m);
    map_place_box(m, doubler, "double_it", STATION_PLAIN);
    must_take(map_name_station(m, doubler, "doubler"), "a name");

    int kept = map_add_station(m);
    map_place_box(m, kept, "keep", STATION_PLAIN);
    must_take(map_name_station(m, kept, "kept"), "a name");
    must_take(map_wire(m, doubler, 0, kept, 0), "the first wire");

    map_start(m, 4);
    /* A standing promise that more work may arrive, so the last
     * sleeper does not decide the program is finished between two
     * deliveries (issue 104). */
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    const int BATCH = 50;
    for (int i = 0; i < BATCH; i++) {
        int v = i;
        map_deliver_value(m, doubler, 0, &v);
    }

    /*
     * ---- and now, mid-flight ----
     *
     * **Wait for the first batch to land before growing, and the wait
     * is not tidiness.** A wire drawn from a station that is
     * currently producing means those values start arriving down it
     * *immediately* — that is what a wire is, and issue 212 says so
     * rather than guarding against it. So a station wired in while
     * fifty values are still moving receives however many of them had
     * not been handled yet, which is a number decided by the
     * scheduler.
     *
     * That is correct behaviour and an untestable assertion. Letting
     * the first batch finish first is what makes "the new station saw
     * exactly the second batch" a fact rather than a race, and it
     * costs the scene nothing: the operations being proven are the
     * same ones either way, and the program is still running
     * throughout — no worker is stopped, nothing is quiesced, the
     * pool is never paused.
     */
    while (atomic_load(&map_station(m, kept)->runs) < BATCH)
        usleep(200);


    /* Add: a place, and a box in it. Nothing can reach it yet. */
    int adder = map_add_station(m);
    map_place_box(m, adder, "add", STATION_PLAIN);
    must_take(map_name_station(m, adder, "adder"), "a name");

    int results = map_add_station(m);
    map_place_box(m, results, "keep", STATION_PLAIN);
    must_take(map_name_station(m, results, "results"), "a name");
    must_take(map_designate_output(m, results), "the results door");

    /* This scene collects at the end rather than as it goes, which is
     * precisely the condition the output door shouts about: results
     * accumulating with nobody taking them. Announced so the lines
     * below read as the engine working rather than as trouble. */
    printf("  (the pile-up notices below are this scene not draining "
           "until the end)\n");
    fflush(stdout);

    /* Configure: the second addend is a constant. Written while the
     * program runs, through the same call a map file's `in 1 = 1000`
     * becomes. */
    must_take(map_configure_port(m, adder, 1, IN_PORT_STATIC, "1000"),
              "a constant bound to a running program");

    /* Wire: last, which is when the first value can arrive. */
    must_take(map_wire(m, adder, 0, results, 0), "the results wire");
    must_take(map_wire(m, doubler, 0, adder, 0), "the wire that starts it");

    for (int i = 0; i < BATCH; i++) {
        int v = i;
        map_deliver_value(m, doubler, 0, &v);
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    int failed = 0;

    if (atomic_load(&map_station(m, adder)->runs) != BATCH) {
        fprintf(stderr, "the station added mid-run ran %d times, not %d\n",
                (int)atomic_load(&map_station(m, adder)->runs), BATCH);
        failed = 1;
    }

    /*
     * The right numbers, not merely the right count. Every value in
     * the second batch was doubled and had a thousand added, so the
     * results are 1000, 1002, 1004 and so on — but they come out in
     * whatever order finished first, which is a schedule and not
     * something to assert. So: tick each expected value off a list
     * and demand the list ends empty.
     */
    int seen[64] = { 0 };
    int taken = 0;
    for (;;) {
        int got = 0;
        if (!map_output_take(m, results, &got, sizeof got))
            break;
        taken++;
        int which = (got - 1000) / 2;
        if (got != 1000 + 2 * which || which < 0 || which >= BATCH) {
            fprintf(stderr, "a result of %d is not two times anything "
                            "plus a thousand\n", got);
            failed = 1;
            break;
        }
        seen[which]++;
    }
    if (taken != BATCH) {
        fprintf(stderr, "%d results came out of the door, not %d\n",
                taken, BATCH);
        failed = 1;
    }
    for (int i = 0; i < BATCH && !failed; i++)
        if (seen[i] != 1) {
            fprintf(stderr, "the result for input %d appeared %d times\n",
                    i, seen[i]);
            failed = 1;
        }

    /* The undisturbed half: everything sent, before and after the
     * program grew, reached the station that was always there. */
    if (atomic_load(&map_station(m, kept)->runs) != 2 * BATCH) {
        fprintf(stderr, "the station that was already there ran %d times, "
                        "not %d — growing the program cost it values\n",
                (int)atomic_load(&map_station(m, kept)->runs), 2 * BATCH);
        failed = 1;
    }

    map_destroy(m);
    if (!failed)
        printf("  a station was added, configured and wired into a running "
               "program; it produced %d results and the running part lost "
               "nothing\n", taken);
    return failed;
}
/* }}} */

int main(void)
{
    char dir[256], map_path[320], from_file[320], from_calls[320];
    snprintf(dir, sizeof dir, "%s/one-path-%d",
             registry_late_source_dir(), (int)getpid());
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p %s", dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "cannot make %s\n", dir);
        return 1;
    }
    snprintf(map_path, sizeof map_path, "%s/both.map", dir);
    snprintf(from_file, sizeof from_file, "%s/from-file.map", dir);
    snprintf(from_calls, sizeof from_calls, "%s/from-calls.map", dir);

    /*
     * Deliberately not the simplest program that could work. It has a
     * constant, a port given a starting depth, a fan-out, and a
     * station left with a port nobody has wired — each of which is
     * something one path could get right and the other wrong.
     */
    static const char *const text =
        "station source seven p\n"
        "  out 0 - adder.0\n"
        "station adder add p\n"
        "  in 1 = 1000\n"
        "  out 0 - twice.0\n"
        "  out 0 - waiting.0\n"
        /* The way out. Every program declares one (issue 209), and
         * this is another thing the two paths have to agree about:
         * the file says it in a fourth word, the surface says it in a
         * call, and the dumps have to come out the same. */
        "station twice double_it p result\n"
        "  in 0 x64\n"
        "station waiting add p\n"
        "  in 1 -\n";
    FILE *f = fopen(map_path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", map_path);
        return 1;
    }
    fputs(text, f);
    fclose(f);

    /* The program deliberately leaves one port unwired, so that the
     * two paths are compared on something incomplete as well as on
     * something finished. Both the warning below and its twin from
     * the second program are the engine noticing exactly that. */
    printf("  (the two warnings below are what this program is for)\n");
    fflush(stdout);

    /* One program, read. */
    map_t *read = map_load_file(map_path, 2);
    dump_to(read, from_file);

    /*
     * The same program, said out loud. Every one of these is an
     * operation a debugger or a workbench could perform on a running
     * program — which is the point: there is nothing here that only
     * a file is allowed to do.
     */
    map_t *built = map_create_empty();
    const char *const names[] = { "source", "adder", "twice", "waiting" };
    const char *const boxes[] = { "seven", "add", "double_it", "add" };
    for (int i = 0; i < 4; i++) {
        int at = map_add_station(built);
        if (at != i) {
            fprintf(stderr, "stations came out in a different order: "
                            "wanted %d, got %d\n", i, at);
            return 1;
        }
        map_place_box(built, at, boxes[i], STATION_PLAIN);
        must_take(map_name_station(built, at, names[i]), "a name");
    }

    must_take(map_configure_port(built, 1, 1, IN_PORT_STATIC, "1000"),
              "a constant");
    map_in_port_start_depth(built, 2, 0, 64);
    must_take(map_configure_port(built, 3, 1, IN_PORT_NONE, NULL),
              "a port left unwired");
    must_take(map_designate_output(built, 2), "the way out");

    map_connect(built, 0, 0, 1, 0);   /* source -> adder.0 */
    map_connect(built, 1, 0, 2, 0);   /* adder  -> twice.0 */
    map_connect(built, 1, 0, 3, 0);   /* adder  -> waiting.0, a fan-out */

    map_start(built, 2);
    must_take(map_bring_up(built), "the finished program");
    dump_to(built, from_calls);

    char *a = slurp(from_file);
    char *b = slurp(from_calls);
    if (strcmp(a, b) != 0) {
        /* Name the first line where they part, because "these differ"
         * about two hundred-line files is not a finding anybody can
         * act on. */
        const char *pa = a, *pb = b;
        int line = 1;
        while (*pa && *pb && *pa == *pb) {
            if (*pa == '\n') line++;
            pa++; pb++;
        }
        fprintf(stderr, "the two paths built different programs, first "
                        "differing at line %d\n", line);
        fprintf(stderr, "  from a file:  %.60s\n", pa);
        fprintf(stderr, "  from calls:   %.60s\n", pb);
        return 1;
    }

    printf("  a program read from a file and one built by calling the "
           "surface dumped identically\n");

    /*
     * A deepened buffer survives the round trip, which is what this
     * whole scene nearly failed on.
     *
     * The dump used to write such a port as a depth followed by a
     * dash, and a bare dash means a port with **no source**. So two
     * different things were spelled the same way: a program with a
     * deepened buffer could be written down and could not be read
     * back, and an arrow into that port was refused on the way in.
     * The format gained a form for it — a depth with nothing after
     * it — and this is the assertion that the two are told apart.
     */
    if (map_station(read, 2)->in_ports[0].capacity != 64) {
        fprintf(stderr, "the deepened buffer came back %d slots deep\n",
                map_station(read, 2)->in_ports[0].capacity);
        return 1;
    }
    if (atomic_load(&map_station(read, 2)->in_ports[0].kind) != IN_PORT_RING) {
        fprintf(stderr, "the deepened buffer came back as something other "
                        "than a buffer\n");
        return 1;
    }
    if (atomic_load(&map_station(read, 3)->in_ports[1].kind) != IN_PORT_NONE) {
        fprintf(stderr, "the port with no source came back as something "
                        "else\n");
        return 1;
    }
    printf("  a deepened buffer and a port with no source stayed different "
           "things across the round trip\n");

    /* Let both run down rather than tearing them apart mid-flight:
     * the fan-out puts one value into a station that can never claim
     * it, and destroying a pool with work still in it says so. */
    pool_release(read->pool);
    pool_join(read->pool);
    pool_release(built->pool);
    pool_join(built->pool);

    map_destroy(read);
    map_destroy(built);

    /* The same operations, on a program that is already running. */
    if (a_station_joins_a_running_program() != 0)
        return 1;

    return 0;
}
