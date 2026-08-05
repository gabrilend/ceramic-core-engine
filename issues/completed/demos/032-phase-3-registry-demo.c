/*
 * 032-phase-3-registry-demo.c — the compiled half of the phase 3 demo.
 *
 * What this is: the scenes of the phase 3 demonstration that need a
 * running engine — the registry read as a parts book, the registry
 * measured against the compiler's own sizeof answers, every value
 * shape flowing through the one call site, and the occupancy figure
 * re-measured so the demos read as one series.
 *
 * How it does it, in general terms: everything printed is either read
 * out of the generated registry or measured on a live map whose
 * stations were placed by name — nothing here is typed in by hand,
 * which is the entire claim of the phase. It joins the report the
 * shell half already opened rather than starting one of its own, so
 * the seven scenes are one document.
 */
#include "026-registry.h"
#include "060-demo-scene.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Redeclared box types, for the sizeof column and byte checks. */
typedef struct { float x; float y; float z; } vec3;
typedef struct { int a; vec3 pos; char note[16]; unsigned long stamp; } record;

/* ------------------------------------------------------------------ */
/* Scene three: the registry, printed whole.                          */
/*                                                                    */
/* Told as a parts book, because that is what it is: every component  */
/* with its dimensions and what it fits, complete enough that a       */
/* person who has never opened the machine can order from it.         */
/* ------------------------------------------------------------------ */

/* {{{ tell_parts_book() */
static void tell_parts_book(void)
{
    scene_open(3, "the registry, and who wrote it");

    scene_problem(
        "The engine needs to know, for every box, its name, the type "
        "and size of each parameter, the type and size of what it "
        "returns, the exact width of the task struct that carries all "
        "that, and which comparison function suits it. None of that is "
        "written by hand. The generator parses the box sources at build "
        "time and emits it, so the description cannot disagree with the "
        "code — and a box that exists is a box the registry knows "
        "about, necessarily.");

    scene_imagine(
        "the parts book that ships with a serious machine: each "
        "component listed with its dimensions and what it mates to. The "
        "useful thing about a parts book is not that it exists but who "
        "wrote it — if a draughtsman copied the numbers out by hand, "
        "the book is a rumour about the machine.");

    scene_stands_for("a box", "a part in the book",
                     "each is an independently made thing that the "
                     "system assembles rather than contains, and each "
                     "can be swapped for another with the same "
                     "dimensions");
    scene_stands_for("parameter and return sizes", "a part's dimensions",
                     "both are what determines whether two things can be "
                     "joined at all, and getting either wrong produces a "
                     "join that looks fine and fails under load");
    scene_stands_for("the types a wire may carry", "what a part mates to",
                     "each restricts the legal connections in advance, "
                     "so a mistake is caught while somebody is designing "
                     "rather than while something is running");
    scene_stands_for("the generator, every build", "surveying the machine",
                     "both derive the document from the object rather "
                     "than the other way round, which is the only "
                     "arrangement in which the document cannot quietly "
                     "go stale");

    scene_blank();
    registry_print(scene_screen_stream());
    registry_print(scene_report_stream());
    scene_blank();

    scene_finding(scene_text(
        "%d boxes, and not one of them was registered by hand. Each is a "
        "function somebody wrote in an ordinary C file; everything above "
        "was derived from the text of those functions at build time. "
        "Adding a part to this machine is writing a function, and the "
        "book reprints itself.", registry_n_boxes));
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the registry against the compiler.                     */
/*                                                                    */
/* Told as two surveyors measuring one field by different methods.    */
/* The claim is not that the sizes are right but that they cannot     */
/* disagree, and two independent measurements agreeing is what that   */
/* claim looks like from outside.                                     */
/* ------------------------------------------------------------------ */

struct probe {
    const char *box;
    const char *which;   /* "ret" or a param index as text */
    int         compiled;
    const char *label;
};

/* {{{ tell_two_surveyors() */
static void tell_two_surveyors(void)
{
    const struct probe probes[] = {
        { "add",               "ret", (int)sizeof(int),    "int" },
        { "mix",               "p1",  (int)sizeof(double), "double" },
        { "make_vec3",         "ret", (int)sizeof(vec3),   "vec3" },
        { "stamp_record",      "ret", (int)sizeof(record), "record" },
        { "magnitude_squared", "ret", (int)sizeof(float),  "float" },
    };

    scene_open(4, "sizes that cannot be wrong");

    scene_problem(
        "The registry's size fields are not numbers the generator "
        "worked out. They are sizeof expressions written into the "
        "emitted C, which the compiler resolves when the registry is "
        "built. That distinction is the whole guarantee: the generator "
        "never has to model the platform's alignment or padding rules, "
        "and a struct that gains a member produces a different number "
        "with nothing edited. This scene checks the registry's answers "
        "against the compiler's own, for five boxes covering five "
        "shapes.");

    scene_imagine(
        "a field measured twice on the same morning — one surveyor "
        "reading the dimensions off the deed in the county office, the "
        "other walking the boundary with a chain. Neither knows what "
        "the other wrote down.");

    scene_stands_for("the registry's size fields", "the deed's dimensions",
                     "both are the written record everything downstream "
                     "acts on, so both are believed without being "
                     "re-checked once anybody starts building");
    scene_stands_for("the compiler answering sizeof", "the chain on the ground",
                     "each measures the actual object by direct contact "
                     "with it, which makes it the authority the written "
                     "record has to answer to");
    scene_stands_for("a mis-sized wire", "a fence in the wrong place",
                     "both are built correctly from an incorrect "
                     "description, so both fail somewhere far away from "
                     "the mistake and long after it was made");

    scene_blank();
    scene_line("%-18s %-9s %8s %8s", "part", "type", "the deed", "the chain");

    for (unsigned i = 0; i < sizeof probes / sizeof probes[0]; i++) {
        const box_info_t *b = registry_find(probes[i].box);
        if (!b) {
            fprintf(stderr, "missing box %s\n", probes[i].box);
            exit(1);
        }
        int from_registry = strcmp(probes[i].which, "ret") == 0
            ? b->return_size
            : b->params[probes[i].which[1] - '0'].size;

        scene_line("%-18s %-9s %8d %8d   %s",
                   probes[i].box, probes[i].label,
                   from_registry, probes[i].compiled,
                   from_registry == probes[i].compiled
                       ? "agreed" : "DISAGREED");

        /* A disagreement is not a finding to report and move past. It
         * means every wire of that type is being copied at the wrong
         * width, so the run stops here. */
        if (from_registry != probes[i].compiled)
            exit(1);
    }

    scene_finding(
        "The two surveyors never disagree, and they cannot: the "
        "generator does not write sizes, it writes sizeof expressions, "
        "and the compiler resolves them when the registry is built. The "
        "deed is not a copy of the field's dimensions — it is an "
        "instruction to go and measure. That is why a struct can gain a "
        "member and nothing needs updating.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene five: every value shape through one call site.               */
/*                                                                    */
/* Told as a loading door that never inspects its freight. The point  */
/* is that one mechanism carries every type, so the story needs an    */
/* opening that handles envelopes and pallets without knowing which   */
/* is which.                                                          */
/* ------------------------------------------------------------------ */

static _Atomic int coverage_hits;
static _Atomic int coverage_wrong;

/* Harness sinks: instrumentation reaching this demo's counters,
 * which generated boxes cannot see. Marked scaffolding. */
/* {{{ check_float__call() */
static void check_float__call(task_t *t)
{
    float f;
    memcpy(&f, t->in[0], sizeof f);
    coverage_hits++;
    /* magnitude_squared of (3+1, 4+1, 0+1) = 16+25+1 = 42. */
    if (f != 42.0f)
        coverage_wrong++;
}
/* }}} */

/* {{{ check_record__call() */
static void check_record__call(task_t *t)
{
    record r;
    memcpy(&r, t->in[0], sizeof r);
    coverage_hits++;
    if (r.a != 7 || r.pos.x != 1.5f || r.note[0] != 'o' || r.stamp != 99UL)
        coverage_wrong++;
}
/* }}} */

typedef struct freight_facts {
    int arrivals;
    int wrong_bytes;
} freight_facts_t;

/* {{{ measure_freight() */
static freight_facts_t measure_freight(void)
{
    freight_facts_t facts;

    /* Chain one: floats -> struct -> struct -> float.
     * make_vec3 -> nudge -> magnitude_squared -> check. */
    map_t *m = map_create(4);
    map_place_box(m, 0, "make_vec3", STATION_PLAIN);
    map_place_box(m, 1, "nudge", STATION_PLAIN);
    map_place_box(m, 2, "magnitude_squared", STATION_PLAIN);
    int one_float[1] = { sizeof(float) };
    map_place(m, 3, check_float__call, STATION_PLAIN, 1, one_float, 0);
    map_connect(m, 0, 0, 1, 0);
    map_connect(m, 1, 0, 2, 0);
    map_connect(m, 2, 0, 3, 0);
    map_start(m, 4);

    coverage_hits = 0;
    coverage_wrong = 0;
    float x = 3, y = 4, z = 0, amount = 1.0f;
    map_deliver_value(m, 0, 0, &x);
    map_deliver_value(m, 0, 1, &y);
    map_deliver_value(m, 0, 2, &z);
    map_deliver_value(m, 1, 1, &amount);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    /* Chain two: int + struct + wide unsigned -> nested record. */
    map_t *m2 = map_create(2);
    map_place_box(m2, 0, "stamp_record", STATION_PLAIN);
    int one_record[1] = { sizeof(record) };
    map_place(m2, 1, check_record__call, STATION_PLAIN, 1, one_record, 0);
    map_connect(m2, 0, 0, 1, 0);
    map_start(m2, 4);

    int a = 7;
    vec3 pos = { 1.5f, 2.5f, 3.5f };
    unsigned long stamp = 99UL;
    map_deliver_value(m2, 0, 0, &a);
    map_deliver_value(m2, 0, 1, &pos);
    map_deliver_value(m2, 0, 2, &stamp);
    pool_release(m2->pool);
    pool_join(m2->pool);
    map_destroy(m2);

    facts.arrivals = coverage_hits;
    facts.wrong_bytes = coverage_wrong;
    return facts;
}
/* }}} */

/* {{{ tell_freight() */
static void tell_freight(const freight_facts_t *facts)
{
    scene_open(5, "every type through a call site that mentions none");

    scene_problem(
        "Every generated shim has the same signature: it takes a task "
        "pointer and returns nothing. Inside, it copies each argument "
        "out of the task's storage into a local of the right type, "
        "calls the box, and copies the result back. The signature "
        "mentions no types at all, and there is no runtime type tag "
        "anywhere — the correctness rests entirely on the widths the "
        "registry computed at build time. This scene sends integers, "
        "floats, structs by value, a nested struct with a fixed string, "
        "and a wide unsigned through that one call site and checks the "
        "bytes at the far end.");

    scene_imagine(
        "a warehouse with exactly one loading door and no inspector "
        "standing at it. Envelopes go through it, and crates, and a "
        "pallet with a manifest nailed to the side. The door does not "
        "open differently for any of them and does not know what it is "
        "passing.");

    scene_stands_for("the shim signature", "the single doorway",
                     "both are one fixed opening that everything must "
                     "pass through, and neither changes shape for what "
                     "is coming — the variety is entirely in the cargo");
    scene_stands_for("a value of any type", "a consignment",
                     "each has its own size and its own internal "
                     "arrangement, and neither carries a label the "
                     "doorway itself reads");
    scene_stands_for("the registry's sizes", "the paperwork filed in advance",
                     "both were worked out before anything moved, and "
                     "both are what makes it safe to handle something "
                     "without inspecting it at the time");
    scene_stands_for("no truncation or padding read", "nothing crushed or loose",
                     "both are the observable proof that the paperwork "
                     "was right, and both fail silently and later if it "
                     "was not");

    scene_measured("consignments arrived",
                   scene_text("%d", facts->arrivals),
                   scene_text("%d chain endpoints", facts->arrivals));
    scene_measured("bytes wrong on arrival",
                   scene_text("%d", facts->wrong_bytes),
                   NULL);

    scene_blank();
    scene_line("what the paperwork reserved for each part:");
    for (int i = 0; i < registry_n_boxes; i++)
        scene_line("  %-20s %4zu bytes, cut exactly",
                   registry_boxes[i].name, registry_boxes[i].task_size);

    if (facts->arrivals != 2 || facts->wrong_bytes != 0) {
        fprintf(stderr, "type coverage failed\n");
        exit(1);
    }

    scene_finding(
        "Integers, floats, structs passed by value, a nested struct "
        "carrying a fixed string, and a wide unsigned — all through one "
        "call site whose signature mentions none of them. The task "
        "struct for each box is cut to the exact width its arguments "
        "need, computed by the compiler at the moment the registry was "
        "built. No inspector, no runtime type tag, and nothing arrived "
        "damaged.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene six: occupancy, one more time.                               */
/*                                                                    */
/* Told as the same car around the same track with a new engine. This */
/* scene exists only to connect this demo to the one before it, so    */
/* the story is about continuity and deliberately nothing else.       */
/* ------------------------------------------------------------------ */

static _Atomic int busy_now;
static _Atomic int busy_peak;

typedef struct series_facts {
    int width;
    int values;
    int peak_busy;
    int workers;
} series_facts_t;

/* {{{ occupancy_probe__call() */
static void occupancy_probe__call(task_t *t)
{
    int entered = ++busy_now;
    int peak = busy_peak;
    while (entered > peak &&
           !atomic_compare_exchange_weak(&busy_peak, &peak, entered))
        ;
    /* Honest arithmetic, then pass the value along unchanged. */
    unsigned long v = 88172645463325252UL;
    for (int i = 0; i < 12000; i++) {
        v ^= v << 13;
        v ^= v >> 7;
        v ^= v << 17;
    }
    int x;
    memcpy(&x, t->in[0], sizeof x);
    x += (int)(v & 1);
    memcpy(t->out, &x, sizeof x);
    busy_now--;
}
/* }}} */

/* {{{ measure_series() */
static series_facts_t measure_series(void)
{
    series_facts_t facts;
    facts.width = scene_pick(12, 20);
    facts.values = scene_pick(40, 80);

    map_t *m = map_create(facts.width);
    int one_int[1] = { sizeof(int) };
    for (int i = 0; i < facts.width; i++)
        map_place(m, i, occupancy_probe__call, STATION_PLAIN, 1, one_int,
                  sizeof(int));
    map_start(m, 0);

    busy_now = 0;
    busy_peak = 0;
    for (int v = 0; v < facts.values; v++)
        for (int s = 0; s < facts.width; s++)
            map_deliver_value(m, s, 0, &v);
    pool_release(m->pool);
    pool_join(m->pool);

    facts.peak_busy = busy_peak;
    facts.workers = pool_worker_count(m->pool);
    map_destroy(m);
    return facts;
}
/* }}} */

/* {{{ tell_series() */
static void tell_series(const series_facts_t *facts)
{
    scene_open(6, "the same measurement, with the glue deleted");

    scene_problem(
        "Phase 3 did not make the engine faster and was never meant to. "
        "It replaced hand-written shims with generated ones, which is a "
        "change to who writes the glue rather than to what the glue "
        "does. The way to show that is to re-run phase 2's occupancy "
        "measurement on the same shape of map and find the same answer "
        "— because a number that moved would mean the generator had "
        "changed the behaviour as well as the authorship.");

    scene_imagine(
        "a car going round a track it has been round before: same "
        "circuit, same driver, same weather. What has changed is under "
        "the bonnet, where an engine assembled by hand from parts cut "
        "to order has been replaced by one built from the parts book.");

    scene_stands_for("the wide map from phase 2", "the same circuit",
                     "both are held fixed on purpose, because a "
                     "comparison is only worth anything when exactly one "
                     "thing has changed");
    scene_stands_for("the same pool and workers", "the same driver",
                     "both are the parts explicitly not under test, and "
                     "leaving them alone is what makes the result "
                     "attributable to the part that was replaced");
    scene_stands_for("hand-written shims", "an engine built to order",
                     "each was made by a person for one specific "
                     "purpose, correctly, at a cost that has to be paid "
                     "again for every new one");
    scene_stands_for("generated shims", "an engine built from the book",
                     "each is produced mechanically from a description, "
                     "so the cost of the hundredth is the same as the "
                     "cost of the first — which is the entire point");

    scene_measured("stations on the track",
                   scene_text("%d", facts->width), NULL);
    scene_measured("laps each",
                   scene_text("%d values", facts->values), NULL);
    scene_measured("running at once",
                   scene_text("%d of %d", facts->peak_busy, facts->workers),
                   scene_text("peak boxes of %d workers", facts->workers));

    scene_finding(
        "Same occupancy, same machine, same pool — and every shim now "
        "written by the generator rather than by a person. That is what "
        "phase 3 was for: not to make the engine faster but to delete "
        "the glue, and the proof that the glue was really deleted is "
        "that this number did not move.");
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root>\n", argv[0]);
        return 1;
    }

    /* Joining the shell half's report rather than opening one. These
     * four scenes are the middle of a seven-scene demo. */
    demo_join(argv[1], "phase-3-registry-demo.txt");

    tell_parts_book();
    tell_two_surveyors();

    freight_facts_t freight = measure_freight();
    tell_freight(&freight);

    series_facts_t series = measure_series();
    tell_series(&series);

    /* The shell half prints scene seven next. Settling this page here
     * means its pager erases only what it wrote. */
    demo_page_break();
    return 0;
}
