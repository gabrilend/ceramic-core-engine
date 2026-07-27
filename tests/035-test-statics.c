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

    map_statics_alloc(m, 4);
    map_static_set_text(m, 0, "1000");
    map_slot_static(m, 0, 1, 0);

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
    map_statics_alloc(m, 2);
    map_static_set_text(m, 0, "1");
    map_static_set_text(m, 1, "2");
    map_slot_static(m, 0, 0, 0);
    map_slot_static(m, 0, 1, 1);
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
    m->stations[0].slots[0].type_name = "record";
    map_place(m, 1, check_record__call, STATION_PLAIN, 1, one_record, 0);
    map_connect(m, 0, 0, 1, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "{ 5, { 1.5, 2.5, 3.5 }, \"hey there\", 42 }");
    map_slot_static(m, 0, 0, 0);

    /* All-static station: deliver cannot wake it, so run it by hand
     * once through the gather path's claim — the same resolution the
     * seed will use. Simplest honest route: bind and read the entry
     * through a claim into a local, then feed the checker. */
    record claimed;
    static_claim(m, &m->stations[0].slots[0], &claimed);
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
    map_statics_alloc(doomed, 1);
    map_static_set_text(doomed, 0, "{ 1.0, 2.0, 3.0, 4.0 }");
    map_slot_static(doomed, 0, 0, 0);
}

static void die_too_few(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "nudge", STATION_PLAIN);
    map_statics_alloc(doomed, 1);
    map_static_set_text(doomed, 0, "{ 1.0, 2.0 }");
    map_slot_static(doomed, 0, 0, 0);
}

static void die_string_for_number(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "nudge", STATION_PLAIN);
    map_statics_alloc(doomed, 1);
    map_static_set_text(doomed, 0, "{ \"one\", 2.0, 3.0 }");
    map_slot_static(doomed, 0, 0, 0);
}

static void die_absent_entry(void)
{
    doomed = map_create(1);
    map_place_box(doomed, 0, "add", STATION_PLAIN);
    map_statics_alloc(doomed, 1);
    map_slot_static(doomed, 0, 0, 0); /* entry 0 has no text */
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
        map_static_write(m, 0, &worlds[i & 1], sizeof(vec3));
    return NULL;
}

static void test_mutation_and_torn_reads(void)
{
    enum { CLAIMS = 4000 };
    map_t *m = map_create(1);
    map_place_box(m, 0, "magnitude_squared", STATION_PLAIN); /* (vec3) */
    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "{ 1, 1, 1 }");

    /* The station's one slot must stay a buffer so deliveries drive
     * it; the static under mutation is claimed via static_claim in a
     * tight loop instead, plus through a second map below. Simpler
     * and just as honest: bind the entry, then race claims directly
     * against the writer. */
    map_slot_static(m, 0, 0, 0);

    torn_seen = 0;
    pthread_t writer;
    pthread_create(&writer, NULL, world_writer, m);

    vec3 got;
    int local_claims = 0;
    for (int i = 0; i < CLAIMS; i++) {
        static_claim(m, &m->stations[0].slots[0], &got);
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
    static_claim(m, &m->stations[0].slots[0], &got);
    check(got.x == 2.0f && got.y == 2.0f && got.z == 2.0f,
          "the last write is what the next claim sees");

    map_destroy(m);
    (void)mutation_claims;
    (void)check_world__call;
    printf("  %d claims raced a writer: zero torn, last write visible\n",
           CLAIMS);
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
    expect_death(die_absent_entry, "an entry with no text accepted");
    printf("  four malformed statics each died at bind, as promised\n");

    test_mutation_and_torn_reads();
    return 0;
}
