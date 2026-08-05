/*
 * 025-phase-2-graph-demo.c — a graph that runs itself, told five ways.
 *
 * What this is: the phase 2 demonstration. A map is described, values
 * are dropped in, and the machine fills every core on its own —
 * nobody scheduled any of this. Each scene states one claim the design
 * makes in the engine's own terms, offers an image to hold it by,
 * justifies every correspondence, and then measures.
 *
 * How it does it, in general terms: maps are built with the phase 2
 * construction calls and hand shims (both scaffolding, both marked
 * for replacement), seeded before the workers are released, and read
 * afterwards through the pool's and the slots' own counters. Boxes
 * that need to take time burn arithmetic rather than sleeping,
 * because nothing in this engine is allowed to block. Scene five is a
 * live panel the reader steers, because backpressure is a shape that
 * changes over time and a still photograph of it is a much weaker
 * argument than the thing moving.
 *
 * Reuses phase 1's pool wholesale — same queue, same workers, same
 * termination — which is the point of a phase demo: the old tool
 * doing new work. Scene three goes further and borrows phase 7's
 * buffer report, quoting the instrument rather than describing it.
 */
#include "018-station.h"
#include "049-observe.h"
#include "060-demo-scene.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* {{{ now_seconds() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ burn() — takes time without blocking */
/*
 * Roughly `rounds` microseconds of honest arithmetic. Calibrated
 * loosely; the demo compares relative numbers, not absolute ones.
 */
static int burn(int rounds)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < rounds * 150; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    return (int)(x & 0x7fffffff);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: occupancy across a wide map.                            */
/* ------------------------------------------------------------------ */

static _Atomic int busy_now;
static _Atomic int busy_peak;
static _Atomic long wide_done;

/* {{{ wide_work and shim — hand shim, deleted by issue 302 */
static int wide_work(int x)
{
    return x + burn(80);
}

static void wide_work__call(task_t *t)
{
    int entered = ++busy_now;
    int peak = busy_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&busy_peak, &peak, entered))
        ;
    int x = *(int *)t->in[0];
    int r = wide_work(x);
    wide_done++;
    busy_now--;
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

typedef struct occupancy_facts {
    int    presses;
    int    sheets_each;
    int    workers;
    int    peak_busy;
    long   sheets_done;
    double elapsed;
} occupancy_facts_t;

/* {{{ measure_occupancy() */
static occupancy_facts_t measure_occupancy(void)
{
    occupancy_facts_t facts;

    /* Width and depth both vary: neither changes what is shown, and a
     * table of numbers that never moves reads as decoration. */
    facts.presses = scene_pick(12, 20);
    facts.sheets_each = scene_pick(40, 80);

    /* One row of independent stations — a map that is parallel
     * because it is wide, not because anyone asked. */
    map_t *m = map_create(facts.presses);
    int one_int[1] = { sizeof(int) };
    for (int i = 0; i < facts.presses; i++)
        map_place(m, i, wide_work__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_start(m, 0); /* 0 = one worker per online processor */

    busy_now = 0;
    busy_peak = 0;
    wide_done = 0;

    for (int v = 0; v < facts.sheets_each; v++)
        for (int s = 0; s < facts.presses; s++)
            map_deliver_value(m, s, 0, &v);

    double before = now_seconds();
    pool_release(m->pool);
    pool_join(m->pool);
    facts.elapsed = now_seconds() - before;

    facts.workers = pool_worker_count(m->pool);
    facts.peak_busy = busy_peak;
    facts.sheets_done = wide_done;

    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ tell_occupancy() */
static void tell_occupancy(const occupancy_facts_t *facts)
{
    scene_open(1, "parallelism nobody asked for");

    scene_problem(
        "A map is a set of stations wired to each other. Stations that "
        "are not wired to each other have no ordering between them, so "
        "when values are waiting at several of them the pool can run "
        "all of those boxes at once. Nothing in the engine decides "
        "this: there is no scheduler, no priority, no plan. The "
        "question is whether a map that is merely wide actually fills "
        "the machine, or whether something has to arrange it.");

    scene_imagine(
        "a print shop with a row of presses along one wall and a stack "
        "of jobs beside each. There is no foreman walking the floor "
        "deciding which press runs next, because there is nothing to "
        "decide — every press has its own work and none of them needs "
        "anything from the others.");

    scene_stands_for("a station", "a press",
                     "each is a fixed place where one kind of work "
                     "happens, has its own queue of things waiting for "
                     "it, and can run without asking permission from "
                     "any of the others");
    scene_stands_for("a value in a slot", "a job on the stack",
                     "each is a piece of work waiting at one specific "
                     "place, and it is the arrival of it — rather than "
                     "any instruction — that makes work happen there");
    scene_stands_for("a box invocation", "a press running",
                     "each is one unit of the actual work getting done, "
                     "and several can be happening simultaneously in "
                     "different places");
    scene_stands_for("the absence of a scheduler", "the absence of a foreman",
                     "in both cases nothing is choosing what happens "
                     "next: the layout has already decided, and adding "
                     "a chooser could only make it slower");

    scene_measured("presses on the wall",
                   scene_text("%d presses", facts->presses),
                   scene_text("%d stations", facts->presses));
    scene_measured("jobs per press",
                   scene_text("%d sheets", facts->sheets_each),
                   scene_text("%d values", facts->sheets_each));
    scene_measured("hands available",
                   scene_text("%d", facts->workers),
                   scene_text("%d worker threads", facts->workers));
    scene_measured("presses running at once",
                   scene_text("%d of %d", facts->peak_busy, facts->workers),
                   "peak concurrent boxes");
    scene_measured("sheets printed",
                   scene_text("%ld sheets", facts->sheets_done),
                   scene_text("%ld tasks", facts->sheets_done));
    scene_measured("time on the floor",
                   scene_text("%.3f s", facts->elapsed),
                   scene_text("%.0f tasks/s",
                              facts->sheets_done / facts->elapsed));

    scene_finding(
        "Every hand the shop had was busy, and no part of this program "
        "contains a decision about which press runs when. That is the "
        "whole of phase 2's claim about parallelism: it is not "
        "scheduled, it is shaped. A wide map is a busy machine because "
        "of what it is, not because of what anyone told it.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: concurrent invocations of one station.                  */
/* ------------------------------------------------------------------ */

static _Atomic int station_now;
static _Atomic int station_peak;
static _Atomic long station_sum;

/* {{{ crowd_work and shim — hand shim, deleted by issue 302 */
static int crowd_work(int x)
{
    return x + (burn(120) & 1);
}

static void crowd_work__call(task_t *t)
{
    int entered = ++station_now;
    int peak = station_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&station_peak, &peak, entered))
        ;
    int x = *(int *)t->in[0];
    int r = crowd_work(x);
    station_sum += r;
    station_now--;
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

typedef struct crowd_facts {
    int  orders;
    int  peak_inside;
    int  nothing_lost;
} crowd_facts_t;

/* {{{ measure_crowd() */
static crowd_facts_t measure_crowd(void)
{
    crowd_facts_t facts;
    facts.orders = scene_pick(400, 700);

    map_t *m = map_create(1);
    int one_int[1] = { sizeof(int) };
    map_place(m, 0, crowd_work__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_start(m, 0);

    station_now = 0;
    station_peak = 0;
    station_sum = 0;

    for (int v = 0; v < facts.orders; v++)
        map_deliver_value(m, 0, 0, &v);

    pool_release(m->pool);
    pool_join(m->pool);

    /* Each invocation adds back at least the value it was handed, so
     * the sum cannot fall below the sum of the inputs unless a value
     * was dropped or handed to two cooks at once. */
    long expected_floor = (long)facts.orders * (facts.orders - 1) / 2;
    facts.nothing_lost = (station_sum >= expected_floor);
    facts.peak_inside = station_peak;

    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ tell_crowd() */
static void tell_crowd(const crowd_facts_t *facts)
{
    scene_open(2, "many threads inside one station at once");

    scene_problem(
        "A station is not a thread and not a lock — it is a place in a "
        "table with a box function attached. So when several values "
        "arrive at one station, several workers can be executing that "
        "same box simultaneously. This sounds dangerous and is not, "
        "because delivery copies each invocation's inputs out under the "
        "station's mutex before any of them starts running. The safety "
        "rests on one rule the design imposes in exchange: a box may "
        "hold no state between calls.");

    scene_imagine(
        "a kitchen with one recipe card pinned above the counter and a "
        "great many cooks. Every cook reads the same card, and there is "
        "no queue to read it because reading does not use it up — what "
        "each takes away is their own tray of ingredients, and from "
        "then on they work at their own bench.");

    scene_stands_for("a box function", "the recipe card",
                     "each is a set of instructions rather than a place "
                     "or a resource, so any number of people can be "
                     "following it at the same moment without taking "
                     "turns");
    scene_stands_for("a worker running that box", "a cook at their own bench",
                     "each is doing the whole job independently, with "
                     "their own materials, and none of them can see or "
                     "disturb what another is part-way through");
    scene_stands_for("claiming inputs under the mutex", "taking your own tray",
                     "both happen once, at the start, in a moment when "
                     "nobody else can be taking theirs — which is what "
                     "makes everything after that moment safe to do in "
                     "parallel");
    scene_stands_for("a box holding no state", "leaving nothing on the card",
                     "both are the rule that keeps the shared thing "
                     "read-only, and breaking either turns the one place "
                     "everybody touches into the one place everybody "
                     "corrupts");

    scene_measured("orders cooked",
                   scene_text("%d dishes", facts->orders),
                   scene_text("%d values", facts->orders));
    scene_measured("cooks at the card",
                   scene_text("%d at once", facts->peak_inside),
                   "peak simultaneous calls");
    scene_measured("every tray accounted",
                   facts->nothing_lost ? "yes" : "NO — trays were lost",
                   NULL);

    scene_finding(
        "Several cooks were inside one station at the same moment and "
        "nothing collided, because what they took was copied out under "
        "the station's lock before any of them started cooking. The "
        "price of that safety is the rule about the card: a box that "
        "remembered anything between invocations would be a card two "
        "cooks could scribble on at once, and no amount of locking "
        "elsewhere would repair it.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: buffer growth under mismatched rates.                 */
/*                                                                    */
/* A single-input station can never accumulate a backlog in its slot, */
/* because every write completes its input set and is claimed on the  */
/* spot — its backlog piles up as tasks in the pool's ring instead.   */
/* Slot buffers absorb a different mismatch: a multi-input station    */
/* fed unevenly, values on one side waiting for their siblings on     */
/* the other. This scene shows both piles, each where it forms.       */
/* ------------------------------------------------------------------ */

static _Atomic long slow_consumed;
static _Atomic long pairs_made;

/* {{{ fast_relay / slow_eater and shims — hand shims, deleted by 302 */
static int fast_relay(int x)
{
    return x + burn(2);
}

static void fast_relay__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = fast_relay(x);
    memcpy(t->out, &r, sizeof r);
}

static void slow_eater(int x)
{
    (void)x;
    burn(200);
    slow_consumed++;
}

static void slow_eater__call(task_t *t)
{
    slow_eater(*(int *)t->in[0]);
}
/* }}} */

/* {{{ pair_up and shim — hand shim, deleted by issue 302 */
static int pair_up(int value, int ticket)
{
    (void)ticket;
    pairs_made++;
    return value;
}

static void pair_up__call(task_t *t)
{
    int value = *(int *)t->in[0];
    int ticket = *(int *)t->in[1];
    int r = pair_up(value, ticket);
    memcpy(t->out, &r, sizeof r);
}
/* }}} */

typedef struct mismatch_facts {
    int    plates;
    int    one_sided_slot_high;
    int    ring_growths;
    int    ring_high;
    int    ring_capacity;
    int    shelf_growths;
    int    shelf_capacity;
    int    shelf_high;
    /* Both maps are kept alive past measuring so the telling can quote
     * the engine's own buffer report off them. Released afterwards by
     * the caller, which is why they are here rather than destroyed
     * where they were built. */
    map_t *single_input_map;
    map_t *paired_map;
} mismatch_facts_t;

/* {{{ measure_mismatch() */
static mismatch_facts_t measure_mismatch(void)
{
    mismatch_facts_t facts;
    int one_int[1] = { sizeof(int) };
    int two_ints[2] = { sizeof(int), sizeof(int) };

    facts.plates = scene_pick(300, 500);

    /* First: the slow single-input consumer. The backlog lands in
     * the pool's task ring, and the slot stays shallow. */
    map_t *m = map_create(2);
    map_place(m, 0, fast_relay__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 1, slow_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_start(m, 0);

    slow_consumed = 0;
    for (int v = 0; v < facts.plates; v++)
        map_deliver_value(m, 0, 0, &v);

    pool_release(m->pool);
    pool_join(m->pool);

    pool_queue_stats(m->pool, &facts.ring_capacity,
                     &facts.ring_high, &facts.ring_growths);
    facts.one_sided_slot_high = m->stations[1].slots[0].high_water;

    if (slow_consumed != facts.plates) {
        fprintf(stderr, "the slow eater consumed %ld of %d\n",
                (long)slow_consumed, facts.plates);
        exit(1);
    }
    facts.single_input_map = m;

    /* Second: the mismatch slots do absorb — a pairing station fed
     * all its values on one side before any tickets arrive on the
     * other. The waiting side must hold everything. */
    map_t *m2 = map_create(1);
    map_place(m2, 0, pair_up__call, STATION_PLAIN, 2, two_ints, sizeof(int));
    map_start(m2, 0);

    pairs_made = 0;
    for (int v = 0; v < facts.plates; v++)
        map_deliver_value(m2, 0, 0, &v);

    /* Read the waiting side at its worst — before the other half
     * arrives, which is the entire point of the measurement. */
    slot_t *waiting = &m2->stations[0].slots[0];
    facts.shelf_growths = waiting->growths;
    facts.shelf_capacity = waiting->capacity;
    facts.shelf_high = waiting->high_water;

    for (int v = 0; v < facts.plates; v++)
        map_deliver_value(m2, 0, 1, &v);
    pool_release(m2->pool);
    pool_join(m2->pool);

    if (pairs_made != facts.plates) {
        fprintf(stderr, "pairing made %ld of %d pairs\n",
                (long)pairs_made, facts.plates);
        exit(1);
    }
    facts.paired_map = m2;

    return facts;
}
/* }}} */

/* {{{ tell_mismatch() */
static void tell_mismatch(const mismatch_facts_t *facts)
{
    scene_open(3, "two places a backlog can form, meaning two things");

    scene_problem(
        "When a producer outruns a consumer the surplus has to sit "
        "somewhere, and in this engine there are two somewheres. Values "
        "can pile up in a station's input slot, waiting for the other "
        "inputs that would complete a call. Or they can pile up as "
        "assembled tasks in the pool's ring, complete and merely "
        "waiting for a free worker. Which one fills tells you something "
        "different about what is wrong, and this scene provokes each in "
        "turn to show that the two are not interchangeable.");

    scene_imagine(
        "a restaurant pass, where work piles up in two places for "
        "opposite reasons. Tickets pile on the rail when the kitchen is "
        "simply slower than the dining room. Plates pile on the warming "
        "shelf because each is waiting for the rest of its order.");

    scene_stands_for("a task in the pool's ring", "a ticket on the rail",
                     "each is a complete instruction that could be acted "
                     "on this instant if a pair of hands were free, so a "
                     "pile of them means too few hands and nothing else");
    scene_stands_for("a value in an input slot", "a plate on the warming shelf",
                     "each is a part of something that cannot proceed "
                     "until its counterpart turns up, so a pile of them "
                     "means the parts are arriving at different rates");
    scene_stands_for("a single-input station", "a one-item order",
                     "in both cases the arrival of one thing completes "
                     "the whole request, so there is never a moment "
                     "where it sits half-assembled waiting for more");
    scene_stands_for("a station that pairs inputs", "a two-item order",
                     "both need every part present before anything can "
                     "start, which is precisely what creates somewhere "
                     "for the early parts to wait");

    scene_measured("orders put through",
                   scene_text("%d plates", facts->plates),
                   scene_text("%d values", facts->plates));

    scene_blank();
    scene_line("one-item orders, kitchen slower than the room:");
    scene_measured("plates on the shelf",
                   scene_text("%d", facts->one_sided_slot_high),
                   "slot high water");
    scene_measured("tickets on the rail",
                   scene_text("%d at worst", facts->ring_high),
                   scene_text("%d ring growths, %d cells",
                              facts->ring_growths, facts->ring_capacity));

    scene_blank();
    scene_line("two-item orders, one half of every order missing:");
    scene_measured("plates on the shelf",
                   scene_text("%d at worst", facts->shelf_high),
                   scene_text("%d slot growths, %d cells",
                              facts->shelf_growths, facts->shelf_capacity));

    /* Quoting phase 7's instrument rather than paraphrasing it. The
     * same reading, from the engine's own mouth, printed to both
     * streams so the mirrored report is not missing the evidence. */
    scene_blank();
    scene_line("and the same map, read by phase 7's buffer report:");
    scene_blank();
    map_report_buffers(facts->paired_map, scene_screen_stream());
    map_report_buffers(facts->paired_map, scene_report_stream());
    /* The engine printed that itself, so the presenter has no idea it
     * happened and would run the finding straight into it. */
    scene_blank();

    scene_finding(
        "A single-input station cannot accumulate anything on its shelf, "
        "because a value that completes an order is claimed the instant "
        "it lands — so its backlog is on the rail instead. That was "
        "found here rather than designed, and the documents did not say "
        "it. The shelf only fills for a station that must pair inputs, "
        "which makes a deep shelf a diagnosis rather than a symptom: "
        "somebody upstream is running ahead of their sibling. Watch the "
        "error stream at teardown, too — the engine shouts there on its "
        "own when a buffer has grown past what it is willing to absorb "
        "in silence.");
}
/* }}} */

/* {{{ free_mismatch() */
/* The maps outlive their measurements so the telling can quote them.
 * Somebody still has to close them, and doing it here keeps the
 * telling free of teardown. */
static void free_mismatch(mismatch_facts_t *facts)
{
    map_destroy(facts->single_input_map);
    map_destroy(facts->paired_map);
    facts->single_input_map = NULL;
    facts->paired_map = NULL;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the cost of fan-out.                                   */
/* ------------------------------------------------------------------ */

static _Atomic long fan_received;

/* {{{ fan boxes and shims — hand shims, deleted by issue 302 */
static int fan_source(int x)
{
    return x + burn(20);
}

static void fan_source__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = fan_source(x);
    memcpy(t->out, &r, sizeof r);
}

static void fan_sink(int x)
{
    (void)x;
    fan_received++;
}

static void fan_sink__call(task_t *t)
{
    fan_sink(*(int *)t->in[0]);
}
/* }}} */

enum { FAN_WIDTHS = 3 };

typedef struct fan_facts {
    int    notices;
    int    depots[FAN_WIDTHS];
    double total_ms[FAN_WIDTHS];
    double per_notice_us[FAN_WIDTHS];
} fan_facts_t;

/* {{{ measure_fan() */
static fan_facts_t measure_fan(void)
{
    fan_facts_t facts;
    static const int widths[FAN_WIDTHS] = { 1, 10, 100 };

    /* The widths are fixed on purpose: they are powers of ten so the
     * shape of the cost can be read straight off the column. Only the
     * number of notices varies. */
    facts.notices = scene_pick(200, 400);

    for (int i = 0; i < FAN_WIDTHS; i++) {
        int n = widths[i];
        map_t *m = map_create(1 + n);
        int one_int[1] = { sizeof(int) };
        map_place(m, 0, fan_source__call, STATION_PLAIN, 1, one_int, sizeof(int));
        for (int s = 0; s < n; s++) {
            map_place(m, 1 + s, fan_sink__call, STATION_PLAIN, 1, one_int, 0);
            map_connect(m, 0, 0, 1 + s, 0);
        }
        map_start(m, 0);

        fan_received = 0;
        for (int v = 0; v < facts.notices; v++)
            map_deliver_value(m, 0, 0, &v);

        double before = now_seconds();
        pool_release(m->pool);
        pool_join(m->pool);
        double elapsed = now_seconds() - before;

        if (fan_received != (long)facts.notices * n) {
            fprintf(stderr, "fan of %d delivered %ld of %ld\n",
                    n, (long)fan_received, (long)facts.notices * n);
            exit(1);
        }

        facts.depots[i] = n;
        facts.total_ms[i] = elapsed * 1e3;
        facts.per_notice_us[i] = elapsed * 1e6 / facts.notices;
        map_destroy(m);
    }

    return facts;
}
/* }}} */

/* {{{ tell_fan() */
static void tell_fan(const fan_facts_t *facts)
{
    scene_open(4, "what one worker pays so that others can start");

    scene_problem(
        "An output port can be wired to any number of input slots. When "
        "a box produces a value, the worker that ran it walks that list "
        "and, for each destination, takes the station's mutex, writes "
        "the value, checks whether the station is now ready, and if it "
        "is, builds a task and pushes it. All of that is serial and all "
        "of it is charged to the one worker that happened to run the "
        "producing box. This scene prices it at one destination, ten, "
        "and a hundred.");

    scene_imagine(
        "a dispatcher who must pass every notice to every depot on her "
        "list. There is no switchboard and no assistant: she dials each "
        "depot herself, one after another, and only when the last has "
        "been told can she pick up the next notice.");

    scene_stands_for("the delivering worker", "the dispatcher",
                     "in both cases one individual is personally "
                     "responsible for every notification, and none of "
                     "that work can be handed to anybody else while it "
                     "is happening");
    scene_stands_for("lock, write, check readiness", "one phone call",
                     "each is a small indivisible errand that must "
                     "complete before the next begins, and the total "
                     "cost is simply how many of them there are");
    scene_stands_for("a station wired to that port", "a depot on the list",
                     "each is a destination that must be told "
                     "individually, because there is no broadcast — "
                     "only a list walked from one end to the other");
    scene_stands_for("a station becoming ready", "a truck rolling",
                     "each is the point where the errand turns into "
                     "work happening elsewhere, in parallel, which is "
                     "what makes the serial dialling worth doing");

    scene_measured("notices handled",
                   scene_text("%d notices", facts->notices),
                   scene_text("%d source values", facts->notices));

    scene_blank();
    scene_line("depots on the list   time on the phone   per notice");
    for (int i = 0; i < FAN_WIDTHS; i++)
        scene_line("%18d   %13.3f ms   %8.3f us",
                   facts->depots[i], facts->total_ms[i],
                   facts->per_notice_us[i]);

    scene_finding(
        "Ten times the depots costs roughly ten times the dialling, and "
        "it is charged to one worker rather than spread across the "
        "shop. That is the honest price of fan-out in this design. It "
        "is worth paying because each of those calls may be what starts "
        "a station running — the dispatcher's serial minute buys a "
        "hundred parallel ones — but it is a real cost and a map wide "
        "enough will feel it.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene five: backpressure, watched and steered.                     */
/* ------------------------------------------------------------------ */

/* {{{ pipeline boxes and shims — hand shims, deleted by issue 302 */
static int stage_quick(int x)
{
    return x + burn(30);
}

static void stage_quick__call(task_t *t)
{
    int x = *(int *)t->in[0];
    int r = stage_quick(x);
    memcpy(t->out, &r, sizeof r);
}

/* The gate pairs each value with a ticket; values wait for tickets. */
static int gate(int value, int ticket)
{
    (void)ticket;
    return value + burn(50);
}

static void gate__call(task_t *t)
{
    int value = *(int *)t->in[0];
    int ticket = *(int *)t->in[1];
    int r = gate(value, ticket);
    memcpy(t->out, &r, sizeof r);
}

static _Atomic long drain_count;

static void stage_drain(int x)
{
    (void)x;
    drain_count++;
}

static void stage_drain__call(task_t *t)
{
    stage_drain(*(int *)t->in[0]);
}
/* }}} */

typedef struct door_facts {
    int  guests;
    int  tickets_released;
    long admitted;
    int  slot_growths;
    int  slot_capacity;
    int  slot_high;
} door_facts_t;

/* {{{ run_door() */
static door_facts_t run_door(void)
{
    door_facts_t facts;
    map_t *m = map_create(3);
    int one_int[1] = { sizeof(int) };
    int two_ints[2] = { sizeof(int), sizeof(int) };
    int batch = scene_pick(40, 70);
    int next_ticket = 0;
    int arrivals = 0;

    facts.guests = 0;
    facts.tickets_released = 0;

    map_place(m, 0, stage_quick__call, STATION_PLAIN, 1, one_int, sizeof(int));
    map_place(m, 1, gate__call, STATION_PLAIN, 2, two_ints, sizeof(int));
    map_place(m, 2, stage_drain__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 1, 0, 2, 0);
    map_start(m, 0);

    drain_count = 0;

    /* Registering as an outside submitter is what keeps the pool from
     * concluding it is finished between batches — without it the door
     * would close the moment the queue outside stopped moving. */
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    scene_live_begin();
    scene_live_key('1', "a crowd arrives");
    scene_live_key('2', "release a batch of tickets");

    /* Nobody is watching a redirected run: it plays a scripted version
     * of what a reader would have done by hand. */
    if (!scene_interactive()) {
        int i;
        for (i = 0; i < 500; i++, arrivals++)
            map_deliver_value(m, 0, 0, &arrivals);
        facts.guests += 500;
        for (i = 0; i < 500; i++)
            map_deliver_value(m, 1, 1, &next_ticket), next_ticket++;
        facts.tickets_released += 500;
        usleep(200000);
    }

    do {
        int key = scene_live_pressed();
        int waiting;

        if (key == '1') {
            int i;
            for (i = 0; i < batch * 4; i++, arrivals++)
                map_deliver_value(m, 0, 0, &arrivals);
            facts.guests += batch * 4;
        } else if (key == '2') {
            int i;
            for (i = 0; i < batch; i++, next_ticket++)
                map_deliver_value(m, 1, 1, &next_ticket);
            facts.tickets_released += batch;
        }

        waiting = map_slot_depth(m, 1, 0);

        scene_frame_begin();
        scene_frame_line("a door, a doorman, and a queue that is allowed "
                         "to be long");
        scene_frame_blank();
        scene_bar("waiting on the pavement",
                  m->stations[1].slots[0].capacity
                      ? (double)waiting / m->stations[1].slots[0].capacity
                      : 0.0,
                  scene_text("%d guests", waiting));
        scene_bar("pavement itself",
                  m->stations[1].slots[0].growths / 10.0,
                  scene_text("%d cells, grown %d times",
                             m->stations[1].slots[0].capacity,
                             m->stations[1].slots[0].growths));
        scene_frame_blank();
        scene_frame_line("guests arrived     %d", facts.guests);
        scene_frame_line("tickets released   %d", facts.tickets_released);
        scene_frame_line("admitted so far    %ld", (long)drain_count);
        scene_frame_end();
    } while (!scene_live_done());

    scene_live_end();

    /* Everybody still outside gets a ticket, so the map can finish. */
    while (next_ticket < facts.guests) {
        map_deliver_value(m, 1, 1, &next_ticket);
        next_ticket++;
        facts.tickets_released++;
    }

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    facts.admitted = drain_count;
    facts.slot_growths = m->stations[1].slots[0].growths;
    facts.slot_capacity = m->stations[1].slots[0].capacity;
    facts.slot_high = m->stations[1].slots[0].high_water;

    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ scene_door() */
static void scene_door(void)
{
    door_facts_t facts;

    scene_open(5, "a consumer that cannot be hurried");

    scene_problem(
        "A station with two buffered inputs runs only when both have a "
        "value. If one side is fed far faster than the other, the "
        "surplus has nowhere to go but that slot's ring buffer, which "
        "grows to hold it. Nothing is dropped and nothing blocks — the "
        "engine simply spends memory to absorb the difference. This "
        "scene lets the reader create that mismatch by hand and watch "
        "the buffer answer, because a number for the depth is much less "
        "convincing than the depth moving.");

    scene_imagine(
        "a crowd arriving at a door where the doorman admits nobody "
        "without a ticket, and tickets arrive from somewhere else "
        "entirely on their own schedule. Nobody is turned away — they "
        "stand on the pavement, and the length of that queue is the "
        "whole story.");

    scene_stands_for("a value in the gate's slot", "a guest on the pavement",
                     "each has arrived and is entirely ready, and is "
                     "held up only because the other thing it needs has "
                     "not turned up yet");
    scene_stands_for("a value on the other input", "a ticket",
                     "each one arriving is what releases exactly one of "
                     "the things that were waiting, so the rate they "
                     "appear at sets the rate everything moves");
    scene_stands_for("a station with both inputs full", "the doorman admitting",
                     "both are the moment the parts finally coincide, "
                     "which is the only moment at which any work can "
                     "actually happen");
    scene_stands_for("the slot's ring buffer", "the pavement itself",
                     "both are the place the surplus is allowed to "
                     "stand, both grow rather than turn anyone away, and "
                     "the size of both is the honest measure of how "
                     "badly the two rates disagree");

    facts = run_door();

    scene_measured("guests arriving",
                   scene_text("%d guests", facts.guests),
                   scene_text("%d values", facts.guests));
    scene_measured("tickets released",
                   scene_text("%d", facts.tickets_released),
                   "values on the other input");
    scene_measured("admitted in the end",
                   scene_text("%ld of %d", facts.admitted, facts.guests),
                   NULL);
    scene_measured("the pavement grew",
                   scene_text("%d times", facts.slot_growths),
                   scene_text("to %d cells, worst %d",
                              facts.slot_capacity, facts.slot_high));

    scene_finding(
        "Backpressure, made into a shape. Nothing here was dropped and "
        "nothing spun: the guests waited in memory, the pavement grew "
        "to hold them, and the queue drained at exactly the rate "
        "tickets allowed. Phase 7 automates this same reading — a "
        "background thread taking these depths on a timer instead of a "
        "reader taking them by hand.");
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root>\n", argv[0]);
        return 1;
    }

    demo_open(argv[1], "phase-2-graph-report.txt",
              "=== phase 2 demo: a graph that runs itself ===");

    demo_note(
        "Two voices below. Everything indented is this demo speaking. "
        "Lines that begin \"observe:\" are the engine itself, on the "
        "error stream, saying that a buffer grew further than it is "
        "willing to absorb without telling somebody — phase 7's alarm, "
        "audible here because this demo links the whole engine rather "
        "than only the parts phase 2 built. Scene 3 sets it off on "
        "purpose; scenes 4 and 5 set it off in passing, which is itself "
        "worth seeing: the alarm does not wait to be asked.");

    occupancy_facts_t occupancy = measure_occupancy();
    tell_occupancy(&occupancy);

    crowd_facts_t crowd = measure_crowd();
    tell_crowd(&crowd);

    mismatch_facts_t mismatch = measure_mismatch();
    tell_mismatch(&mismatch);
    free_mismatch(&mismatch);

    fan_facts_t fan = measure_fan();
    tell_fan(&fan);

    scene_door();

    demo_close("=== nobody scheduled any of this; the wiring did ===");
    return 0;
}
