/*
 * 039-phase-5-decide-demo.c — a map that decides.
 *
 * What this is: the phase 5 demonstration. Every earlier map could
 * only transform; these maps choose, and every choice is visible in
 * the wiring rather than buried in a function. A sorting network
 * splits a stream into buckets through three comparators. One
 * comparator's three arrows produce all six comparison operators —
 * and then a shape no operator has a name for. An iterator deals
 * work across consumers of wildly uneven speed, exactly evenly, in
 * thoroughly unfair order. And a comparison that raw bytes would
 * have gotten backwards is shown beside the lie.
 *
 * How it does it, in general terms: registry boxes placed as
 * comparators and iterators, statics for thresholds, counting sinks
 * on every port, and arithmetic checked against what the fed values
 * demand. Each scene measures first and tells afterwards, so the
 * stories can be reworded without touching a measurement.
 *
 * The four stories are a coin sorter, a doorman with a height stick,
 * a dealer at a card table, and a ledger written partly in red ink.
 * Each was chosen for a different mechanic; see issue 707 for the
 * contract they answer to.
 */
#include "018-station.h"
#include "026-registry.h"
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

/* ------------------------------------------------------------------ */
/* Scene one: the sorting network.                                    */
/*                                                                    */
/* Told as a coin sorter. The claim is that the branching is geometry */
/* rather than instruction — nothing decides where a coin goes, the   */
/* shape of the machine does — and a coin sorter is the household     */
/* object that already works exactly that way.                        */
/* ------------------------------------------------------------------ */

static _Atomic int buckets[4];

/* {{{ the four counting trays */
static void bucket0__call(task_t *t) { (void)t; buckets[0]++; }
static void bucket1__call(task_t *t) { (void)t; buckets[1]++; }
static void bucket2__call(task_t *t) { (void)t; buckets[2]++; }
static void bucket3__call(task_t *t) { (void)t; buckets[3]++; }
/* }}} */

enum { SORT_BURSTS = 4 };

typedef struct sorter_facts {
    int coins;
    int per_burst;
    int seen[SORT_BURSTS][4];   /* the trays after each handful */
    int final[4];
    int expected[4];
    int exact;
} sorter_facts_t;

/* {{{ measure_sorter() */
static sorter_facts_t measure_sorter(void)
{
    sorter_facts_t facts;

    /* Any multiple of four works; the count is drawn so the columns
     * are not the same columns every run. */
    facts.per_burst = scene_pick(80, 140);
    facts.coins = facts.per_burst * SORT_BURSTS;

    /* Three deciders, four buckets:
     *   C50 splits the world at fifty; its low side meets C25, its
     *   equal-and-high sides meet C75. Equal and greater arrows
     *   landing on one destination is itself the point — ">=" is a
     *   wiring, not a setting. */
    map_t *m = map_create(7);
    map_place_box(m, 0, "keep", STATION_COMPARATOR); /* vs 50 */
    map_place_box(m, 1, "keep", STATION_COMPARATOR); /* vs 25 */
    map_place_box(m, 2, "keep", STATION_COMPARATOR); /* vs 75 */
    int one_int[1] = { sizeof(int) };
    map_place(m, 3, bucket0__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 4, bucket1__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 5, bucket2__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 6, bucket3__call, STATION_PLAIN, 1, one_int, 0);

    map_connect(m, 0, 0, 1, 0);   /* below fifty -> the 25 decider  */
    map_connect(m, 0, 1, 2, 0);   /* exactly fifty -> the 75 decider */
    map_connect(m, 0, 2, 2, 0);   /* above fifty  -> the 75 decider */
    map_connect(m, 1, 0, 3, 0);   /* below 25 -> bucket 0 */
    map_connect(m, 1, 1, 4, 0);   /* at or above 25 -> bucket 1 */
    map_connect(m, 1, 2, 4, 0);
    map_connect(m, 2, 0, 5, 0);   /* below 75 -> bucket 2 */
    map_connect(m, 2, 1, 6, 0);   /* at or above 75 -> bucket 3 */
    map_connect(m, 2, 2, 6, 0);

    map_statics_alloc(m, 3);
    map_static_set_text(m, 0, "50");
    map_static_set_text(m, 1, "25");
    map_static_set_text(m, 2, "75");
    map_slot_static(m, 0, 1, 0);
    map_slot_static(m, 1, 1, 1);
    map_slot_static(m, 2, 1, 2);

    /* What the arithmetic demands of the buckets. */
    for (int b = 0; b < 4; b++)
        facts.expected[b] = 0;
    for (int i = 0; i < facts.coins; i++) {
        int v = (i * 7) % 100;
        facts.expected[v < 25 ? 0 : v < 50 ? 1 : v < 75 ? 2 : 3]++;
    }

    map_start(m, 0);
    memset((void *)buckets, 0, sizeof buckets);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    for (int burst = 0; burst < SORT_BURSTS; burst++) {
        for (int i = burst * facts.per_burst;
             i < (burst + 1) * facts.per_burst; i++) {
            int v = (i * 7) % 100;
            map_deliver_value(m, 0, 0, &v);
        }
        usleep(30000);
        for (int b = 0; b < 4; b++)
            facts.seen[burst][b] = buckets[b];
    }
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);

    facts.exact = 1;
    for (int b = 0; b < 4; b++) {
        facts.final[b] = buckets[b];
        if (facts.final[b] != facts.expected[b])
            facts.exact = 0;
    }

    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ tell_sorter() */
static void tell_sorter(const sorter_facts_t *facts)
{
    static const char *const trays[4] = {
        "0..24", "25..49", "50..74", "75..99",
    };

    scene_open(1, "branching with no branch in any function");

    scene_problem(
        "A comparator station compares its input against a threshold "
        "and sends the value out of one of three ports: less, equal, or "
        "greater. Chain three of them and a stream splits four ways. "
        "The thing worth noticing is where that decision lives — not in "
        "any box function, none of which contains a conditional about "
        "buckets, but in which arrows exist between which ports. "
        "Rewiring changes the behaviour; recompiling is not involved.");

    scene_imagine(
        "a coin sorter: a box with graded slots and trays underneath. "
        "Pour a handful in and the coins arrive sorted, though nothing "
        "inside examined a coin, consulted a table, or made up its "
        "mind. There is no rule written anywhere in it.");

    scene_stands_for("a value entering the map", "a coin poured in",
                     "each arrives carrying everything needed to decide "
                     "where it belongs, so nothing has to be looked up "
                     "about it on the way through");
    scene_stands_for("a comparator station", "a graded slot",
                     "each applies one fixed test to everything that "
                     "reaches it and has no memory of what came before, "
                     "so the same coin always goes the same way");
    scene_stands_for("an arrow from a port", "where a slot leads",
                     "each is a physical connection rather than an "
                     "instruction, which is why changing one changes the "
                     "machine's behaviour without changing its parts");
    scene_stands_for("a station at a branch's end", "a tray",
                     "each is simply the place things end up, and it "
                     "does no sorting of its own — its identity is "
                     "entirely a consequence of what leads to it");

    scene_measured("coins poured in",
                   scene_text("%d coins", facts->coins),
                   scene_text("%d values", facts->coins));
    scene_measured("slots in the box",
                   "3", "3 comparator stations");
    scene_measured("where slots lead",
                   "9", "9 arrows");
    scene_measured("rules written down",
                   "0", "no branch in any box function");

    scene_blank();
    scene_line("the trays filling, a handful at a time:");
    for (int burst = 0; burst < SORT_BURSTS; burst++) {
        scene_line("  after %d coins:", (burst + 1) * facts->per_burst);
        for (int b = 0; b < 4; b++) {
            char bar[64];
            int hashes = facts->seen[burst][b] / 4;
            if (hashes > 48)
                hashes = 48;
            for (int h = 0; h < hashes; h++)
                bar[h] = '#';
            bar[hashes] = '\0';
            scene_line("    %-7s %4d  %s", trays[b], facts->seen[burst][b], bar);
        }
    }

    scene_measured("final distribution",
                   scene_text("%d/%d/%d/%d", facts->final[0], facts->final[1],
                              facts->final[2], facts->final[3]),
                   scene_text("arithmetic demands %d/%d/%d/%d",
                              facts->expected[0], facts->expected[1],
                              facts->expected[2], facts->expected[3]));
    scene_measured("agreement",
                   facts->exact ? "exact" : "WRONG", NULL);

    /* A sorter that misroutes is not a scene to narrate past: every
     * later claim in this demo rests on this one being right. */
    if (!facts->exact)
        exit(1);

    scene_finding(
        "Three stations and nine arrows, and not one function anywhere "
        "in this program contains an if about buckets. The branching is "
        "in the wiring, where it can be seen from outside, drawn, and "
        "rewired — which is the difference between a decision the "
        "machine makes and a decision the machine is.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: six operators, and a seventh shape with no name.        */
/*                                                                    */
/* Told as a doorman with a height stick and three doors. A           */
/* comparator has exactly three outcomes and the operator is only     */
/* which of them lead anywhere — so the story needs a fixed test and  */
/* a variable number of exits, which is what a doorway offers.        */
/* ------------------------------------------------------------------ */

static _Atomic int op_hits;
static _Atomic int shape_equal;
static _Atomic int shape_greater;

/* {{{ the counting doors */
static void op_sink__call(task_t *t)     { (void)t; op_hits++; }
static void shape_eq__call(task_t *t)    { (void)t; shape_equal++; }
static void shape_gt__call(task_t *t)    { (void)t; shape_greater++; }
/* }}} */

struct operator_wiring {
    const char *name;
    const char *doors;
    int         ports[2];
    int         n;
    int         multiplier;   /* how many of each three arrivals pass */
};

enum { OPERATORS = 6 };

typedef struct doorman_facts {
    int  each;                /* of each height, per run */
    int  admitted[OPERATORS];
    int  expected[OPERATORS];
    int  nameless_equal;
    int  nameless_greater;
    int  nameless_discarded;
} doorman_facts_t;

/* Which ports mean which operator: the entire "no operator setting"
 * argument as a dispatch table. */
static const struct operator_wiring operators[OPERATORS] = {
    { "<",  "shorter",           { 0 },    1, 1 },
    { "<=", "shorter, same",     { 0, 1 }, 2, 2 },
    { "==", "same",              { 1 },    1, 1 },
    { "!=", "shorter, taller",   { 0, 2 }, 2, 2 },
    { ">=", "same, taller",      { 1, 2 }, 2, 2 },
    { ">",  "taller",            { 2 },    1, 1 },
};

/* {{{ run_operator() */
/*
 * One comparator against fifty, with the named outcome ports wired
 * to a single counter. Equal numbers below, at, and above: the count
 * that arrives is the operator's truth table in numbers.
 */
static int run_operator(const int *ports, int n_ports, int each)
{
    map_t *m = map_create(2);
    map_place_box(m, 0, "keep", STATION_COMPARATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, op_sink__call, STATION_PLAIN, 1, one_int, 0);
    for (int i = 0; i < n_ports; i++)
        map_connect(m, 0, ports[i], 1, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "50");
    map_slot_static(m, 0, 1, 0);

    map_start(m, 2);
    op_hits = 0;
    for (int i = 0; i < each; i++) {
        int below = 10, at = 50, above = 90;
        map_deliver_value(m, 0, 0, &below);
        map_deliver_value(m, 0, 0, &at);
        map_deliver_value(m, 0, 0, &above);
    }
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    return op_hits;
}
/* }}} */

/* {{{ measure_doorman() */
static doorman_facts_t measure_doorman(void)
{
    doorman_facts_t facts;
    facts.each = scene_pick(15, 30);

    for (int i = 0; i < OPERATORS; i++) {
        facts.admitted[i] = run_operator(operators[i].ports,
                                         operators[i].n, facts.each);
        facts.expected[i] = facts.each * operators[i].multiplier;
    }

    /* The seventh shape: equal goes one way, greater another, less
     * nowhere. No comparison operator can say this; three arrows
     * say it easily. */
    map_t *m = map_create(3);
    map_place_box(m, 0, "keep", STATION_COMPARATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, shape_eq__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 2, shape_gt__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 1, 1, 0);
    map_connect(m, 0, 2, 2, 0);
    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "50");
    map_slot_static(m, 0, 1, 0);
    map_start(m, 2);
    shape_equal = 0;
    shape_greater = 0;
    for (int i = 0; i < facts.each; i++) {
        int below = 10, at = 50, above = 90;
        map_deliver_value(m, 0, 0, &below);
        map_deliver_value(m, 0, 0, &at);
        map_deliver_value(m, 0, 0, &above);
    }
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    facts.nameless_equal = shape_equal;
    facts.nameless_greater = shape_greater;
    facts.nameless_discarded = facts.each;
    return facts;
}
/* }}} */

/* {{{ tell_doorman() */
static void tell_doorman(const doorman_facts_t *facts)
{
    scene_open(2, "every comparison operator, from one station");

    scene_problem(
        "There is no setting on a comparator that says which operator "
        "it implements. There is only the three-way answer — less, "
        "equal, greater — and three ports. Wire port 2 alone and the "
        "station behaves as a greater-than. Wire ports 1 and 2 and it "
        "is a greater-or-equal. All six comparison operators are "
        "reachable this way, and so are arrangements that no operator "
        "has a name for, because the ports were never obliged to "
        "combine into a word.");

    scene_imagine(
        "a doorman holding a stick at a fixed height, with three doors "
        "behind him — one for shorter, one for the same, one for "
        "taller. The doorman never changes and the stick never moves. "
        "Which doors happen to be unlocked is the entire configuration "
        "of the place.");

    scene_stands_for("a comparator station", "the doorman with the stick",
                     "each applies the identical test to everybody and "
                     "has no discretion whatsoever — all of the variety "
                     "is downstream of it, never inside it");
    scene_stands_for("the threshold in a static slot", "the height of the stick",
                     "both are one fixed value the test is made against, "
                     "set beforehand and consulted rather than decided");
    scene_stands_for("ports 0, 1 and 2", "the three doors",
                     "each corresponds to exactly one of the three "
                     "possible answers, so between them they cover every "
                     "case and overlap in none");
    scene_stands_for("an arrow wired from a port", "an unlocked door",
                     "each turns one possible answer into somewhere to "
                     "go, and leaving one unwired is a real choice: it "
                     "means that answer discards the value");

    scene_measured("arrivals of each height",
                   scene_text("%d each", facts->each),
                   scene_text("%d values, three heights", facts->each * 3));

    scene_blank();
    scene_line("%-4s %-20s %8s %10s", "is", "doors unlocked",
               "admitted", "expected");
    for (int i = 0; i < OPERATORS; i++) {
        scene_line("%-4s %-20s %8d %10d   %s",
                   operators[i].name, operators[i].doors,
                   facts->admitted[i], facts->expected[i],
                   facts->admitted[i] == facts->expected[i] ? "" : "WRONG");
        if (facts->admitted[i] != facts->expected[i])
            exit(1);
    }

    scene_blank();
    scene_line("and one arrangement no operator has a name for:");
    scene_measured("same height, this way",
                   scene_text("%d", facts->nameless_equal),
                   "port 1 to one station");
    scene_measured("taller, that way",
                   scene_text("%d", facts->nameless_greater),
                   "port 2 to another");
    scene_measured("shorter, nowhere",
                   scene_text("%d discarded", facts->nameless_discarded),
                   "port 0 unwired");

    if (facts->nameless_equal != facts->each
        || facts->nameless_greater != facts->each)
        exit(1);

    scene_finding(
        "Six operators out of one station, and the station was never "
        "told which it was — the operator is a property of the wiring "
        "and nothing else. Then a seventh arrangement that no language "
        "has an operator for, expressed just as easily, because three "
        "independent doors were always more expressive than a word like "
        "\"greater-or-equal\". Unwiring a door is a decision too, and it "
        "is the one that discards.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: the spreader under uneven load.                       */
/*                                                                    */
/* Told as a dealer at a card table. Two facts have to land at once — */
/* the deal is perfectly even, and the order of play is not — and     */
/* dealing cards is where everybody already holds both ideas without  */
/* finding them contradictory.                                        */
/* ------------------------------------------------------------------ */

static _Atomic int spread_counts[3];
static char arrival_order[64];
static _Atomic int arrivals;

/* {{{ the three players, deliberately mismatched */
static void note_arrival(int port)
{
    int i = arrivals++;
    if (i < (int)sizeof arrival_order - 1)
        arrival_order[i] = (char)('0' + port);
    spread_counts[port]++;
}

static void burn_us(int rounds)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < rounds * 150; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    if (x == 0) abort();
}

static void quick_eater__call(task_t *t)  { (void)t; burn_us(10); note_arrival(0); }
static void middle_eater__call(task_t *t) { (void)t; burn_us(200); note_arrival(1); }
static void slow_eater__call(task_t *t)   { (void)t; burn_us(800); note_arrival(2); }
/* }}} */

typedef struct dealer_facts {
    int  hands;
    int  dealt[3];
    char order[64];
    int  even;
} dealer_facts_t;

/* {{{ measure_dealer() */
static dealer_facts_t measure_dealer(void)
{
    dealer_facts_t facts;

    /* Divisible by three, so "exactly even" is a claim the deal can
     * actually satisfy. */
    facts.hands = scene_pick(25, 40) * 3;

    map_t *m = map_create(4);
    map_place_box(m, 0, "keep", STATION_ITERATOR);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, quick_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 2, middle_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 3, slow_eater__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 1, 2, 0);
    map_connect(m, 0, 2, 3, 0);

    map_start(m, 0);
    memset((void *)spread_counts, 0, sizeof spread_counts);
    memset(arrival_order, 0, sizeof arrival_order);
    arrivals = 0;

    for (int i = 0; i < facts.hands; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);

    facts.even = 1;
    for (int p = 0; p < 3; p++) {
        facts.dealt[p] = spread_counts[p];
        if (facts.dealt[p] != facts.hands / 3)
            facts.even = 0;
    }
    memcpy(facts.order, arrival_order, sizeof facts.order);
    facts.order[sizeof facts.order - 1] = '\0';

    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ tell_dealer() */
static void tell_dealer(const dealer_facts_t *facts)
{
    scene_open(3, "spreading work evenly is not balancing it");

    scene_problem(
        "An iterator station holds a cursor and sends each value out of "
        "the next port in turn, advancing the cursor under the station "
        "mutex at enqueue time. That makes the distribution exactly "
        "even by count. It says nothing at all about how long each "
        "consumer takes, and the iterator never asks — so with "
        "consumers of wildly different speeds, the counts come out "
        "identical while the finishing order is a mess. Both halves of "
        "that are worth seeing, because confusing a spreader with a "
        "balancer is the mistake this design invites.");

    scene_imagine(
        "a dealer putting one card in front of each player in turn, "
        "round after round. The deal is exactly fair and takes no "
        "notice of anybody: one player studies each card for a minute, "
        "another slaps it down instantly, and the dealer's hands keep "
        "moving at the same rate regardless.");

    scene_stands_for("an iterator station", "the dealer",
                     "each distributes strictly in turn without looking "
                     "up, so fairness is guaranteed by the procedure "
                     "rather than by any judgement about the recipients");
    scene_stands_for("the cursor advancing at enqueue", "putting down one card",
                     "both happen at the moment of handing over rather "
                     "than the moment of use, which is exactly why the "
                     "count can be even while the timing is not");
    scene_stands_for("a station on one of its ports", "a player at the table",
                     "each receives its share regardless of whether it "
                     "has finished the last one, and each works entirely "
                     "at its own pace once it has");
    scene_stands_for("a box actually running", "a card being played",
                     "each takes however long it takes, independently, "
                     "which is the part the dealer has no visibility of "
                     "and no control over");

    scene_measured("cards dealt",
                   scene_text("%d hands", facts->hands),
                   scene_text("%d values", facts->hands));
    scene_measured("player one holds",
                   scene_text("%d cards", facts->dealt[0]),
                   "the quick one");
    scene_measured("player two holds",
                   scene_text("%d cards", facts->dealt[1]),
                   "about twenty times slower");
    scene_measured("player three holds",
                   scene_text("%d cards", facts->dealt[2]),
                   "about eighty times slower");
    scene_measured("the deal was",
                   facts->even ? "exactly even" : "UNEVEN", NULL);

    scene_blank();
    scene_line("the order play actually finished in:");
    scene_line("%.48s...", facts->order);

    if (!facts->even)
        exit(1);

    scene_finding(
        "Assignment is round-robin and happens when the task is "
        "enqueued; arrival is whenever each consumer gets round to "
        "finishing. Both halves are on view above and they disagree "
        "completely, which is the honest description of what an "
        "iterator is: a spreader, not a balancer. It will not notice "
        "that one consumer is drowning, because noticing would mean "
        "asking, and asking is what a gatherer is for.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the comparison that would have been wrong.             */
/*                                                                    */
/* Told as a ledger with debts written in red. The mistake being      */
/* prevented is reading a sign bit as part of a magnitude, and red    */
/* ink is the everyday notation where the same digits mean the        */
/* opposite thing depending on something that is not a digit.         */
/* ------------------------------------------------------------------ */

static _Atomic int wrong_scene_less;
static _Atomic int wrong_scene_greater;

/* {{{ the two verdict counters */
static void ws_less__call(task_t *t)    { (void)t; wrong_scene_less++; }
static void ws_greater__call(task_t *t) { (void)t; wrong_scene_greater++; }
/* }}} */

typedef struct ledger_facts {
    double             entry;
    double             threshold;
    int                routed_less;
    int                routed_greater;
    unsigned long long entry_bits;
    unsigned long long threshold_bits;
    int                bytes_say_greater;
} ledger_facts_t;

/* {{{ measure_ledger() */
static ledger_facts_t measure_ledger(void)
{
    ledger_facts_t facts;

    /* The values are fixed rather than drawn: this scene depends on
     * one being negative and the other a small positive, which is
     * precisely the case the byte comparison gets backwards. A drawn
     * pair could accidentally not demonstrate anything. */
    facts.entry = -2.0;
    facts.threshold = 0.5;

    map_t *m = map_create(3);
    map_place_box(m, 0, "mix", STATION_COMPARATOR);
    int one_double[1] = { sizeof(double) };
    map_place(m, 1, ws_less__call, STATION_PLAIN, 1, one_double, 0);
    map_place(m, 2, ws_greater__call, STATION_PLAIN, 1, one_double, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 0, 2, 2, 0);
    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "0.5");
    map_slot_static(m, 0, 2, 0);

    map_start(m, 2);
    wrong_scene_less = 0;
    wrong_scene_greater = 0;
    int count = 1;
    double factor = facts.entry;
    map_deliver_value(m, 0, 0, &count);
    map_deliver_value(m, 0, 1, &factor);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    facts.routed_less = wrong_scene_less;
    facts.routed_greater = wrong_scene_greater;

    /* The raw-bytes verdict: both doubles read as unsigned words.
     * The sign bit is the most significant bit, so a negative number
     * reads as enormous — this is the exact wrong answer a naive
     * byte comparison routes on. (memcmp on a little-endian machine
     * lies differently, starting from the low byte; the unsigned
     * read is the canonical form of the mistake.) */
    memcpy(&facts.entry_bits, &facts.entry, sizeof facts.entry_bits);
    memcpy(&facts.threshold_bits, &facts.threshold,
           sizeof facts.threshold_bits);
    facts.bytes_say_greater = facts.entry_bits > facts.threshold_bits;

    return facts;
}
/* }}} */

/* {{{ tell_ledger() */
static void tell_ledger(const ledger_facts_t *facts)
{
    scene_open(4, "why comparison cannot be done on the bytes");

    scene_problem(
        "A comparator has to order values whose type it does not know. "
        "The tempting shortcut is to compare the raw bytes, which is "
        "fast, requires no type information, and is wrong. In a double, "
        "the top bit is the sign, so reading the bytes as a plain "
        "unsigned number makes every negative value enormous — the "
        "ordering comes out inverted for exactly the cases that matter. "
        "This is why the generator emits a comparison function per "
        "type, and this scene shows the disagreement rather than "
        "asserting it.");

    scene_imagine(
        "an old ledger recording debts in red ink and credits in black. "
        "A clerk told only to read the digits will happily rank a "
        "two-pound debt above a fifty-pence credit — two is more than "
        "nought point five, after all. Every digit they read was "
        "correct.");

    scene_stands_for("the bytes of a value", "the digits on the page",
                     "both are the complete written form and both can be "
                     "read perfectly accurately while still supporting "
                     "an answer that is backwards");
    scene_stands_for("the sign bit", "the red ink",
                     "each is a mark that is not part of the magnitude "
                     "yet reverses the ordering entirely, and each is "
                     "invisible to anybody comparing magnitudes alone");
    scene_stands_for("a raw byte comparison", "the digit-reading clerk",
                     "both are fast, need no knowledge of what they are "
                     "handling, and are confidently wrong in precisely "
                     "the cases where being right matters");
    scene_stands_for("a generated compare function", "a clerk told what ink means",
                     "each was given the type's own rules in advance, so "
                     "each knows which marks change the answer instead "
                     "of having to guess from the shape of the page");

    scene_measured("the entry",
                   scene_text("%.1f", facts->entry), "in red — a debt");
    scene_measured("the threshold",
                   scene_text("%.1f", facts->threshold), "in black");
    scene_measured("the engine filed it",
                   facts->routed_less ? "below — correct"
                                      : "ABOVE — wrong", NULL);

    scene_blank();
    scene_line("the same two numbers, as the digit-reading clerk sees them:");
    scene_line("entry     0x%016llx", facts->entry_bits);
    scene_line("threshold 0x%016llx", facts->threshold_bits);
    scene_line("the clerk's verdict: %s",
               facts->bytes_say_greater
                   ? "\"above\" — the sign bit read as magnitude"
                   : "\"below\"");

    if (!facts->routed_less || facts->routed_greater)
        exit(1);
    if (!facts->bytes_say_greater) {
        fprintf(stderr, "the byte lie failed to lie — scene needs rethinking\n");
        exit(1);
    }

    scene_finding(
        "The two verdicts disagree, and that disagreement is the entire "
        "reason compare functions are generated per type rather than "
        "comparing bytes. The comparator does not know what a double "
        "is; it calls the function the generator wrote for that type, "
        "which does. Every type that can be a threshold gets one, "
        "written from the type's own declaration, at build time.");
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root>\n", argv[0]);
        return 1;
    }

    double before = now_seconds();

    demo_open(argv[1], "phase-5-decide-report.txt",
              "=== phase 5 demo: a map that decides ===");

    sorter_facts_t sorter = measure_sorter();
    tell_sorter(&sorter);

    doorman_facts_t doorman = measure_doorman();
    tell_doorman(&doorman);

    dealer_facts_t dealer = measure_dealer();
    tell_dealer(&dealer);

    ledger_facts_t ledger = measure_ledger();
    tell_ledger(&ledger);

    demo_close(scene_text(
        "=== branching lives in the wiring, visible, with no name "
        "needed ===\n(whole demo: %.2f s on the same pool as every phase "
        "before)", now_seconds() - before));
    return 0;
}
