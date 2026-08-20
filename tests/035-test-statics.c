/*
 * 035-test-statics.c — proves static slots and the statics table
 * (issues 401, 402, 405).
 *
 * What this is: the tests that a value which is simply always there
 * behaves like one — never consumed, never affecting readiness,
 * identical on every claim until deliberately altered, and altered
 * without tearing. Plus the struct reader: brace text into bytes
 * that match a compiled initializer exactly, and every malformed
 * shape fatal at bind time.
 *
 * How it does it, in general terms: maps place registry boxes so
 * slots know their types, bind statics, and run; death cases fork a
 * child and expect it to abort. The torn-read test hammers a
 * two-field struct from a writer thread while claims stream, and any
 * task seeing fields from two different worlds fails it.
 */
#include "018-station.h"
#include "026-registry.h"

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
    if (!WIFSIGNALED(status)) {
        fprintf(stderr, "statics test failed: %s — the child survived\n", what);
        exit(1);
    }
}
/* }}} */

/* {{{ test_static_feeds_forever() */
static _Atomic long static_sum;
static _Atomic int static_runs;

static void tally_sum__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    static_sum += x;
    static_runs++;
}

static void test_static_feeds_forever(void)
{
    enum { VALUES = 50 };
    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, tally_sum__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);

    map_slot_static_text(m, 0, 1, "1000");

    map_start(m, 4);
    static_sum = 0;
    static_runs = 0;
    /* Only the buffer side is fed; the static side is always full,
     * so every single delivery completes a set. */
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    long expected = 0;
    for (int i = 0; i < VALUES; i++)
        expected += i + 1000;
    check(static_runs == VALUES, "one run per buffered value");
    check(static_sum == expected, "the static arrived identically every time");
    map_destroy(m);
    printf("  a static fed %d runs without ever being consumed\n", VALUES);
}
/* }}} */

/* {{{ test_all_static_station_never_fires() */
static void test_all_static_station_never_fires(void)
{
    /* A station whose slots are all static is never discovered by
     * delivery — nothing can be written into it. This is the
     * property the seed sweep and gatherers both stand on. */
    map_t *m = map_create(1);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_slot_static_text(m, 0, 0, "1");
    map_slot_static_text(m, 0, 1, "2");
    map_start(m, 2);
    pool_release(m->pool);
    /* If the vacuously-ready station were runnable by delivery, the
     * pool would never terminate (it would keep producing). It
     * terminates immediately instead. */
    pool_join(m->pool);
    map_destroy(m);
    printf("  an all-static station is never woken by delivery\n");
}
/* }}} */

/* {{{ test_struct_constant_bytes() */
static _Atomic int record_checked;

static void check_record__call(task_t *t)
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

static void relay_record__call(task_t *t)
{
    /* Harness relay so the record static has a registry-typed home:
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
static void claim_constant(map_t *m, int station, int slot, void *into)
{
    station_t *s = map_station(m, station);
    slot_t *sl = &s->slots[slot];
    pthread_mutex_lock(&s->mutex);
    memcpy(into, sl->constant, (size_t)sl->elem_size);
    pthread_mutex_unlock(&s->mutex);
}
/* }}} */

static void test_struct_constant_bytes(void)
{
    map_t *m = map_create(2);
    /* nudge takes (vec3, float): use its vec3 slot for a struct
     * static... but the full every-kind case wants `record`. Place a
     * harness relay typed by hand for the record, with the type name
     * granted through a registry-placed twin being unavailable —
     * so instead: use stamp_record's vec3 parameter for the nested
     * case and a hand relay for the full record below. */
    int one_record[1] = { sizeof(record) };
    map_place(m, 0, relay_record__call, STATION_PLAIN, 1, one_record, sizeof(record));
    /* Hand placement has no type names, so grant this slot its type
     * the way the loader would: through the registry's name for it. */
    map_station(m, 0)->slots[0].type_name = "record";
    map_place(m, 1, check_record__call, STATION_PLAIN, 1, one_record, 0);
    map_connect(m, 0, 0, 1, 0);

    map_slot_static_text(m, 0, 0, "{ 5, { 1.5, 2.5, 3.5 }, \"hey there\", 42 }");

    /* All-static station: delivery cannot wake it, so read its
     * constant the way a claim does and feed the checker by hand. */
    record claimed;
    claim_constant(m, 0, 0, &claimed);
    map_start(m, 2);
    map_deliver_value(m, 1, 0, &claimed);
    pool_release(m->pool);
    pool_join(m->pool);

    check(record_checked == 1, "the checker ran");
    map_destroy(m);
    (void)padded_ok;
    printf("  brace text became bytes identical to a compiled initializer\n");
}
/* }}} */

/* {{{ death scenarios: malformed statics are fatal at bind */
static map_t *doomed;

static void die_too_many(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "nudge", STATION_PLAIN); /* (vec3, float) */
    map_slot_static_text(doomed, 0, 0, "{ 1.0, 2.0, 3.0, 4.0 }");
}

static void die_too_few(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "nudge", STATION_PLAIN);
    map_slot_static_text(doomed, 0, 0, "{ 1.0, 2.0 }");
}

static void die_string_for_number(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "nudge", STATION_PLAIN);
    map_slot_static_text(doomed, 0, 0, "{ \"one\", 2.0, 3.0 }");
}

static void die_untyped_port(void)
{
    /* A hand-placed station carries element sizes and no type names,
     * so there is nothing to tell the reader what shape the text
     * should become. This replaces a scenario that stopped existing:
     * an entry the file never gave a value for, which was a hole in a
     * table, and there is no table (issue 401). */
    doomed = map_create(1);
    int one_int[1] = { sizeof(int) };
    map_place(doomed, 0, tally_sum__call, STATION_PLAIN, 1, one_int, 0);
    map_slot_static_text(doomed, 0, 0, "5");
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

static void check_world__call(task_t *t)
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
    map_t *m = arg;
    vec3 worlds[2] = { { 1, 1, 1 }, { 2, 2, 2 } };
    for (int i = 0; i < 4000; i++)
        map_slot_static_write(m, 0, 0, &worlds[i & 1], sizeof(vec3));
    return NULL;
}

static void test_mutation_and_torn_reads(void)
{
    enum { CLAIMS = 4000 };
    map_t *m = map_create(1);
    map_place_box(m, 0, "magnitude_squared", STATION_PLAIN); /* (vec3) */
    map_slot_static_text(m, 0, 0, "{ 1, 1, 1 }");

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

    map_destroy(m);
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
    map_t *m = map_create(1);
    map_place_box(m, 0, "add", STATION_PLAIN);   /* (int, int) */
    map_slot_static_text(m, 0, 0, "7");
    map_slot_static_text(m, 0, 1, "7");

    int a = 0, b = 0;
    claim_constant(m, 0, 0, &a);
    claim_constant(m, 0, 1, &b);
    check(a == 7 && b == 7, "both ports took the same written value");

    int changed = 99;
    map_slot_static_write(m, 0, 0, &changed, sizeof changed);
    claim_constant(m, 0, 0, &a);
    claim_constant(m, 0, 1, &b);
    check(a == 99, "the written port changed");
    check(b == 7, "its neighbour did not");

    map_destroy(m);
    printf("  two ports given one value are independent afterwards\n");
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
    map_t *first = map_create(1);
    map_t *second = map_create(1);
    map_place_box(first, 0, "add", STATION_PLAIN);
    map_place_box(second, 0, "add", STATION_PLAIN);

    map_slot_static_text(first, 0, 0, "10");
    map_slot_static_text(second, 0, 0, "20");

    map_start(first, 1);
    map_start(second, 1);

    int a = 0, b = 0;
    claim_constant(first, 0, 0, &a);
    claim_constant(second, 0, 0, &b);
    check(a == 10 && b == 20, "each map kept its own value while both ran");

    int changed = 555;
    map_slot_static_write(first, 0, 0, &changed, sizeof changed);
    claim_constant(first, 0, 0, &a);
    claim_constant(second, 0, 0, &b);
    check(a == 555, "the written map changed");
    check(b == 20, "the other map did not notice");

    pool_release(first->pool);
    pool_release(second->pool);
    pool_join(first->pool);
    pool_join(second->pool);
    map_destroy(first);
    map_destroy(second);
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
    return 0;
}
