/*
 * 035-test-statics.c — proves static ports, whose value lives on
 * the port that reads it (issues 401, 402, 405).
 *
 * What this is: the tests that a value which is simply always there
 * behaves like one — never consumed, never affecting readiness,
 * identical on every claim until deliberately altered, and altered
 * without tearing. Plus the struct reader: brace text into bytes
 * that match a compiled initializer exactly, and every malformed
 * shape fatal at bind time.
 *
 * How it does it, in general terms: maps place generated boxes so
 * ports know their types, bind statics, and run; death cases fork a
 * child and expect it to abort. The torn-read test hammers a
 * two-field struct from a writer thread while claims stream, and any
 * task seeing fields from two different worlds fails it.
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

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Box types, redeclared as in the source. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { int a; vec3 pos; char note[16]; unsigned long stamp; } record;

/* {{{ check() / expect_death() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "statics test failed: %s\n", what);
        exit(1);
    }
}

/*
 * Run a scenario in a child and demand it dies abnormally — the
 * fatal-at-bind contract is as much the product as the parsing is.
 */
static void expect_death(void (*scenario)(void), const char *what)
{
    pid_t pid = fork();
    if (pid == 0) {
        /* Quiet the child's dying words; the parent only reads the
         * manner of death. */
        freopen("/dev/null", "w", stderr);
        scenario();
        _exit(0); /* surviving is the failure */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    /*
     * **Exit 70, or a signal.** Malformed constant text now ends the
     * program with the code meaning the calling code was wrong rather
     * than aborting (issue 106), which is what lets a caller tell a
     * fault it can correct and retry from one it cannot. A signal is
     * still accepted, because a few deaths further down have not been
     * moved onto a code yet and this helper's job is to prove the
     * value was refused rather than to police how.
     */
    int refused = WIFSIGNALED(status)
               || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    if (!refused) {
        fprintf(stderr, "statics test failed: %s — the child survived\n", what);
        exit(1);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 70) {
        fprintf(stderr, "statics test failed: %s — refused, but with exit "
                        "code %d rather than 70, which is the code meaning "
                        "the calling code was wrong\n",
                what, WEXITSTATUS(status));
        exit(1);
    }
}
/* }}} */

/* {{{ test_static_feeds_forever() */
static _Atomic long static_sum;
static _Atomic int static_runs;

static void tally_sum__call(cera_task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    static_sum += x;
    static_runs++;
}

static void test_static_feeds_forever(void)
{
    enum { VALUES = 50 };
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, "add", CERA_STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    cera_map_place(m, 1, tally_sum__call, CERA_STATION_PLAIN, 1, one_int, 0);
    cera_map_connect(m, 0, 0, 1, 0);

    cera_map_in_port_static_text(m, 0, 1, "1000");

    cera_map_start(m, 4);
    static_sum = 0;
    static_runs = 0;
    /* Only the buffer side is fed; the static side is always full,
     * so every single delivery completes a set. */
    for (int i = 0; i < VALUES; i++)
        cera_map_deliver_value(m, 0, 0, &i);
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    long expected = 0;
    for (int i = 0; i < VALUES; i++)
        expected += i + 1000;
    check(static_runs == VALUES, "one run per buffered value");
    check(static_sum == expected, "the static arrived identically every time");
    cera_map_destroy(m);
    printf("  a static fed %d runs without ever being consumed\n", VALUES);
}
/* }}} */

/* {{{ test_all_static_station_never_fires() */
static void test_all_static_station_never_fires(void)
{
    /* A station whose ports are all static is never discovered by
     * delivery — nothing can be written into it. That used to be the
     * property the seed sweep and the pull path stood on; both are
     * gone, and what starts such a station now is a write to one of
     * its own ports, which is an event. */
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "add", CERA_STATION_PLAIN);
    cera_map_in_port_static_text(m, 0, 0, "1");
    cera_map_in_port_static_text(m, 0, 1, "2");
    cera_map_start(m, 2);
    cera_pool_release(m->pool);
    /* If the vacuously-ready station were runnable by delivery, the
     * pool would never terminate (it would keep producing). It
     * terminates immediately instead. */
    cera_pool_join(m->pool);
    cera_map_destroy(m);
    printf("  an all-static station is never woken by delivery\n");
}
/* }}} */

/* {{{ test_struct_constant_bytes() */
static _Atomic int record_checked;

static void check_record__call(cera_task_t *t)
{
    record got;
    memcpy(&got, t->in[0], sizeof got);

    /* The same value, written as a compiled initializer. */
    record want;
    memset(&want, 0, sizeof want);
    want.a = 5;
    want.pos.x = 1.5f;
    want.pos.y = 2.5f;
    want.pos.z = 3.5f;
    strcpy(want.note, "hey there");
    want.stamp = 42;

    record_checked++;
    if (got.a != want.a || memcmp(&got.pos, &want.pos, sizeof want.pos) != 0
        || strcmp(got.note, want.note) != 0 || got.stamp != want.stamp) {
        fprintf(stderr, "struct constant bytes differ from the initializer\n");
        exit(1);
    }
}

static _Atomic int padded_ok;

static void relay_record__call(cera_task_t *t)
{
    /* Harness relay so the record static has a home with a declared type:
     * claims land here, then flow to the checker. */
    record r;
    memcpy(&r, t->in[0], sizeof r);
    memcpy(t->out, &r, sizeof r);
}

/* {{{ claim_constant() */
/*
 * Read a port's constant exactly the way the engine's claim does:
 * under the station's own mutex, which is the lock a runtime write
 * also takes (issue 405). The engine's own claim is not exported —
 * it is a row in a dispatch table on the delivery path — so the test
 * performs the same two steps rather than calling it, and the point
 * of the test is that those two steps never see a half-written value.
 */
static void claim_constant(cera_map_t *m, int station, int port, void *into)
{
    cera_station_t *s = cera_map_station(m, station);
    cera_in_port_t *sl = &s->in_ports[port];
    pthread_mutex_lock(&s->mutex);
    memcpy(into, sl->constant, (size_t)sl->elem_size);
    pthread_mutex_unlock(&s->mutex);
}
/* }}} */

static void test_struct_constant_bytes(void)
{
    cera_map_t *m = cera_map_create(2);
    /* nudge takes (vec3, float): use its vec3 port for a struct
     * static... but the full every-kind case wants `record`. Place a
     * harness relay typed by hand for the record, with the type name
     * granted through a twin placed by name being unavailable —
     * so instead: use stamp_record's vec3 parameter for the nested
     * case and a hand relay for the full record below. */
    int one_record[1] = { sizeof(record) };
    cera_map_place(m, 0, relay_record__call, CERA_STATION_PLAIN, 1, one_record, sizeof(record));
    /*
     * Hand placement grants no type, so this port is given one the way
     * a placement function would — and that is now **two** things, not
     * one (issue 311b). The spelling, which messages and the dump
     * read; and the field table, which is what turning brace text into
     * bytes actually needs, because it says which field sits at which
     * offset.
     *
     * It used to be enough to set the spelling, because the reader
     * searched every emitted struct table for a matching name. It does
     * not search any more: a generated placement function knows the
     * type concretely and hands the address over. A hand-placed port
     * that is given only a name is therefore a port whose constant
     * cannot be read — which is the same limit hand placement has
     * always had, arriving where it can be seen.
     */
    cera_map_station(m, 0)->in_ports[0].type_name = "record";
    cera_map_station(m, 0)->in_ports[0].text = cera_struct_text_find("record");
    cera_map_place(m, 1, check_record__call, CERA_STATION_PLAIN, 1, one_record, 0);
    cera_map_connect(m, 0, 0, 1, 0);

    cera_map_in_port_static_text(m, 0, 0, "{ 5, { 1.5, 2.5, 3.5 }, \"hey there\", 42 }");

    /* All-static station: delivery cannot wake it, so read its
     * constant the way a claim does and feed the checker by hand. */
    record claimed;
    claim_constant(m, 0, 0, &claimed);
    cera_map_start(m, 2);
    cera_map_deliver_value(m, 1, 0, &claimed);
    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    check(record_checked == 1, "the checker ran");
    cera_map_destroy(m);
    (void)padded_ok;
    printf("  brace text became bytes identical to a compiled initializer\n");
}
/* }}} */

/* {{{ death scenarios: malformed statics are fatal at bind */
static cera_map_t *doomed;

static void die_too_many(void)
{
    doomed = cera_map_create(1);
    cera_map_place_box(doomed, 0, "nudge", CERA_STATION_PLAIN); /* (vec3, float) */
    cera_map_in_port_static_text(doomed, 0, 0, "{ 1.0, 2.0, 3.0, 4.0 }");
}

static void die_too_few(void)
{
    doomed = cera_map_create(1);
    cera_map_place_box(doomed, 0, "nudge", CERA_STATION_PLAIN);
    cera_map_in_port_static_text(doomed, 0, 0, "{ 1.0, 2.0 }");
}

static void die_string_for_number(void)
{
    doomed = cera_map_create(1);
    cera_map_place_box(doomed, 0, "nudge", CERA_STATION_PLAIN);
    cera_map_in_port_static_text(doomed, 0, 0, "{ \"one\", 2.0, 3.0 }");
}

static void die_untyped_port(void)
{
    /* A hand-placed station carries element sizes and no type names,
     * so there is nothing to tell the reader what shape the text
     * should become. This replaces a scenario that stopped existing:
     * an entry the file never gave a value for, which was a hole in a
     * table, and there is no table (issue 401). */
    doomed = cera_map_create(1);
    int one_int[1] = { sizeof(int) };
    cera_map_place(doomed, 0, tally_sum__call, CERA_STATION_PLAIN, 1, one_int, 0);
    cera_map_in_port_static_text(doomed, 0, 0, "5");
}
/* }}} */

/* {{{ test_mutation_and_torn_reads() */
/*
 * One writer alternates a vec3 entry between two self-consistent
 * worlds; many claims stream by. Any claim seeing a mix of worlds is
 * a torn read — the exact failure the statics mutex exists to
 * prevent (issue 405).
 */
static _Atomic int torn_seen;
static _Atomic int mutation_claims;

static void check_world__call(cera_task_t *t)
{
    vec3 v;
    memcpy(&v, t->in[0], sizeof v);
    /* World A is all 1s; world B is all 2s. */
    int a = v.x == 1.0f && v.y == 1.0f && v.z == 1.0f;
    int b = v.x == 2.0f && v.y == 2.0f && v.z == 2.0f;
    mutation_claims++;
    if (!a && !b)
        torn_seen++;
    memcpy(t->out, &v.x, sizeof v.x);
}

static void *world_writer(void *arg)
{
    cera_map_t *m = arg;
    vec3 worlds[2] = { { 1, 1, 1 }, { 2, 2, 2 } };
    for (int i = 0; i < 4000; i++)
        cera_map_in_port_static_write(m, 0, 0, &worlds[i & 1], sizeof(vec3));
    return NULL;
}

static void test_mutation_and_torn_reads(void)
{
    enum { CLAIMS = 4000 };
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "magnitude_squared", CERA_STATION_PLAIN); /* (vec3) */
    cera_map_in_port_static_text(m, 0, 0, "{ 1, 1, 1 }");

    torn_seen = 0;
    pthread_t writer;
    pthread_create(&writer, NULL, world_writer, m);

    vec3 got;
    int local_claims = 0;
    for (int i = 0; i < CLAIMS; i++) {
        claim_constant(m, 0, 0, &got);
        int a = got.x == 1.0f && got.y == 1.0f && got.z == 1.0f;
        int b = got.x == 2.0f && got.y == 2.0f && got.z == 2.0f;
        if (!a && !b)
            torn_seen++;
        local_claims++;
    }
    pthread_join(writer, NULL);

    check(torn_seen == 0, "no claim ever saw fields from two worlds");
    check(local_claims == CLAIMS, "all claims ran");

    /* And the altered value is picked up: after the writer's last
     * write (world B), a fresh claim reads all 2s. */
    claim_constant(m, 0, 0, &got);
    check(got.x == 2.0f && got.y == 2.0f && got.z == 2.0f,
          "the last write is what the next claim sees");

    cera_map_destroy(m);
    (void)mutation_claims;
    (void)check_world__call;
    printf("  %d claims raced a writer through the station's own mutex: "
           "zero torn, last write visible\n", CLAIMS);
}
/* }}} */

/* {{{ test_ports_are_independent() */
/*
 * Two ports given the same text end up with two independent values.
 *
 * This is the observable difference from the table (issue 401), and it
 * is what makes the old hazard stop being expressible rather than
 * merely documented. Under the table an entry's bytes were shaped by
 * whichever port bound it first, and both ports read those same bytes
 * — so two ports of different types read one value each their own way,
 * and writing through one was visible through the other.
 */
static void test_ports_are_independent(void)
{
    cera_map_t *m = cera_map_create(1);
    cera_map_place_box(m, 0, "add", CERA_STATION_PLAIN);   /* (int, int) */
    cera_map_in_port_static_text(m, 0, 0, "7");
    cera_map_in_port_static_text(m, 0, 1, "7");

    int a = 0, b = 0;
    claim_constant(m, 0, 0, &a);
    claim_constant(m, 0, 1, &b);
    check(a == 7 && b == 7, "both ports took the same written value");

    int changed = 99;
    cera_map_in_port_static_write(m, 0, 0, &changed, sizeof changed);
    claim_constant(m, 0, 0, &a);
    claim_constant(m, 0, 1, &b);
    check(a == 99, "the written port changed");
    check(b == 7, "its neighbour did not");

    cera_map_destroy(m);
    printf("  two ports given one value are independent afterwards\n");
}
/* }}} */

/* {{{ static void awkward_values_round_trip() */
/*
 * **Format, read, compare bytes** — for the three cases that could
 * not round-trip at all before (issue 408).
 *
 * A string was written out raw and read back by searching for the
 * next quote. So a value holding a **quote** ended its own text
 * early; a value holding a **tab or a newline** produced a map file
 * with a line break inside a line; and a byte **above 0x7F** went out
 * as whatever the reader's locale made of it. All three are values
 * the engine holds perfectly well and could not write down, which
 * makes it a correctness hole rather than a matter of polish — the
 * dump claims to round-trip, and for those values it did not.
 *
 * The test is bytes in, text out, bytes back, compared. Nothing about
 * the shape, nothing about the spelling: the value that went in has
 * to be the value that comes out.
 */
static void awkward_values_round_trip(void)
{
    static const struct {
        const char *note;
        record      value;
    } cases[] = {
        { "a quote inside the text",
          { 1, { 1.0f, 2.0f, 3.0f }, "say \"hello\" now", 10 } },
        { "a tab and a newline",
          { 2, { 0.5f, 0.5f, 0.5f }, "one\ttwo\nthree", 20 } },
        { "a byte above 0x7F",
          { 3, { 9.0f, 8.0f, 7.0f }, "caf\xc3\xa9 \x01\x7f", 30 } },
        { "a backslash, which introduces everything else",
          { 4, { 1.5f, 2.5f, 3.5f }, "a\\\\b\\\\c", 40 } },
    };

    int one_record[1] = { sizeof(record) };

    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        /* A hand-placed relay taking one record, given the spelling
         * and the field table the way a placement function would —
         * the same harness the struct-constant test uses, because the
         * demo boxes take no `record` parameter. */
        cera_map_t *m = cera_map_create(1);
        cera_map_place(m, 0, relay_record__call, CERA_STATION_PLAIN, 1, one_record,
                  sizeof(record));
        cera_in_port_t *sl = &cera_map_station(m, 0)->in_ports[0];
        sl->type_name = "record";
        sl->text = cera_struct_text_find("record");

        /* Bytes straight onto the port, bypassing text entirely, so
         * what is being round-tripped is a value rather than a
         * spelling. */
        sl->kind = CERA_IN_PORT_STATIC;
        sl->constant_set = 1;
        memcpy(sl->constant, &cases[i].value, sizeof(record));

        /* Out as text. */
        char text[1024];
        int wrote = in_port_constant_text(sl, text, sizeof text);
        check(wrote > 0 && wrote < (int)sizeof text, cases[i].note);

        /* And back in, onto a second port of the same shape. */
        cera_map_t *back = cera_map_create(1);
        cera_map_place(back, 0, relay_record__call, CERA_STATION_PLAIN, 1, one_record,
                  sizeof(record));
        cera_in_port_t *to = &cera_map_station(back, 0)->in_ports[0];
        to->type_name = "record";
        to->text = cera_struct_text_find("record");
        cera_map_in_port_static_text(back, 0, 0, text);

        record got;
        memcpy(&got, to->constant, sizeof got);
        if (memcmp(&got, &cases[i].value, sizeof got) != 0) {
            fprintf(stderr, "statics test failed: %s did not survive the "
                            "round trip\n  wrote: %s\n", cases[i].note, text);
            exit(1);
        }

        cera_map_destroy(m);
        cera_map_destroy(back);
    }

    printf("  four awkward values formatted, read back, and compared byte "
           "for byte\n");
}
/* }}} */

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "statics test failed: refused %s: %s\n",
                what, refusal);
        exit(1);
    }
}
/* }}} */

/* {{{ static void a_wire_computes_a_constant() */
/*
 * **A constant computed at startup rather than written down** (issue
 * 405).
 *
 * A value arriving at a static port overwrites the constant instead
 * of queueing into a buffer the port does not have. What that buys is
 * the pattern somebody would otherwise ask the engine for a feature
 * to get: read a thing once at the beginning, and let everything
 * afterwards work from that moment. Here it is a station that runs
 * once, wired into a downstream station's static port.
 *
 * **This is not the back channel returning**, and the distinction is
 * the whole point. The back channel had a *box* reach out and write a
 * value with nothing in the wiring showing it, so two stations could
 * be talking with no arrow between them and the picture lied. The box
 * here is untouched — it takes its arguments, returns one value,
 * remembers nothing, and has no idea what happens next. **The wire is
 * what says this value overwrites a static**, and a wire is visible
 * in the map file, in the dump, and on a canvas.
 *
 * The scene: `seven` runs once because it has no inputs, and its
 * seven lands on the second port of an `add` whose first port is fed
 * ordinary values. Every sum afterwards is that seven, plus whatever
 * arrived — which the first port's values prove by coming out
 * offset by exactly seven each.
 */
static _Atomic long computed_sum;
static _Atomic int computed_runs;

static void tally_computed__call(cera_task_t *t)
{
    int v;
    memcpy(&v, t->in[0], sizeof v);
    atomic_fetch_add(&computed_sum, v);
    atomic_fetch_add(&computed_runs, 1);
}

static void a_wire_computes_a_constant(void)
{
    cera_map_t *m = cera_map_create_empty();

    int source = cera_map_add_station(m);
    cera_map_place_box(m, source, "seven", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, source, "source"), "a name");

    int adder = cera_map_add_station(m);
    cera_map_place_box(m, adder, "add", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, adder, "adder"), "a name");
    /* Port 1 is a constant, and starts as one somebody wrote down. */
    cera_map_in_port_static_text(m, adder, 1, "0");

    int tally = cera_map_add_station(m);
    int one_int = (int)sizeof(int);
    cera_map_place(m, tally, tally_computed__call, CERA_STATION_PLAIN, 1, &one_int, 0);
    must_take(cera_map_name_station(m, tally, "tally"), "a name");

    /* The wire that makes the constant computed. */
    must_take(cera_map_wire(m, source, 0, adder, 1),
              "a wire into a static port");
    must_take(cera_map_wire(m, adder, 0, tally, 0), "a wire to the tally");

    int gate = cera_map_add_station(m);
    cera_map_place_box(m, gate, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, gate, "gate"), "a name");
    must_take(cera_map_designate_argument(m, gate, 0, 0), "an entrance");
    must_take(cera_map_wire(m, gate, 0, adder, 0), "the ordinary input");

    int out = cera_map_add_station(m);
    cera_map_place_box(m, out, "keep", CERA_STATION_PLAIN);
    must_take(cera_map_name_station(m, out, "out"), "a name");
    must_take(cera_map_designate_result(m, out, 0, 0), "a way out");

    cera_map_start(m, 2);
    cera_pool_submitter_register(m->pool);
    must_take(cera_map_bring_up(m), "the program");
    cera_pool_release(m->pool);

    /* The source ran at bring-up — it has no inputs — so seven is
     * already sitting on the adder's second port. Wait for it, so
     * what follows is a fact rather than a schedule. */
    while (atomic_load(&cera_map_station(m, source)->runs) < 1)
        usleep(200);

    const int BATCH = 20;
    for (int i = 0; i < BATCH; i++) {
        int v = i;
        must_take(cera_map_deliver_argument(m, gate, 0, &v, sizeof v),
                  "an argument");
    }

    cera_pool_submitter_unregister(m->pool);
    cera_pool_join(m->pool);

    check(atomic_load(&computed_runs) == BATCH,
          "every value went through the adder");
    /* Each sum is its input plus the computed seven. */
    long expected = 0;
    for (int i = 0; i < BATCH; i++)
        expected += i + 7;
    check(atomic_load(&computed_sum) == expected,
          "and every one of them was added to the seven a wire put "
          "there, not to the zero somebody wrote down");
    check(atomic_load(&cera_map_station(m, source)->runs) == 1,
          "the station that computed the constant ran once, which is "
          "what makes it a constant");

    cera_map_destroy(m);
    printf("  a wire computed a constant: %d values, each plus the seven "
           "a station put on a static port\n", BATCH);
}
/* }}} */

/* {{{ test_two_maps_at_once() */
/*
 * Two maps alive in one process at the same time, not seeing each
 * other's values.
 *
 * This is the property the whole change buys, and it could not be
 * tested at all before: a process-wide pointer named one map as *the*
 * map, so a second one silently took the first's place as the target
 * of every box-initiated statics write and every timing sample. The
 * pointer existed for the box-reachable write; removing that write
 * removed the reason for the pointer, and the restriction went with
 * it (issue 405).
 */
static void test_two_maps_at_once(void)
{
    cera_map_t *first = cera_map_create(1);
    cera_map_t *second = cera_map_create(1);
    cera_map_place_box(first, 0, "add", CERA_STATION_PLAIN);
    cera_map_place_box(second, 0, "add", CERA_STATION_PLAIN);

    cera_map_in_port_static_text(first, 0, 0, "10");
    cera_map_in_port_static_text(second, 0, 0, "20");

    cera_map_start(first, 1);
    cera_map_start(second, 1);

    int a = 0, b = 0;
    claim_constant(first, 0, 0, &a);
    claim_constant(second, 0, 0, &b);
    check(a == 10 && b == 20, "each map kept its own value while both ran");

    int changed = 555;
    cera_map_in_port_static_write(first, 0, 0, &changed, sizeof changed);
    claim_constant(first, 0, 0, &a);
    claim_constant(second, 0, 0, &b);
    check(a == 555, "the written map changed");
    check(b == 20, "the other map did not notice");

    cera_pool_release(first->pool);
    cera_pool_release(second->pool);
    cera_pool_join(first->pool);
    cera_pool_join(second->pool);
    cera_map_destroy(first);
    cera_map_destroy(second);
    printf("  two maps ran side by side and did not see each other\n");
}
/* }}} */

int main(void)
{
    test_static_feeds_forever();
    test_all_static_station_never_fires();
    test_struct_constant_bytes();

    expect_death(die_too_many, "too many values accepted");
    expect_death(die_too_few, "too few values accepted");
    expect_death(die_string_for_number, "a string where a number belongs accepted");
    expect_death(die_untyped_port, "a constant on a port with no type accepted");
    printf("  four malformed statics each died where they were given\n");

    test_mutation_and_torn_reads();
    test_ports_are_independent();
    test_two_maps_at_once();
    a_wire_computes_a_constant();
    awkward_values_round_trip();
    return 0;
}
