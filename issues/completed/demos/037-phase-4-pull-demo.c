/*
 * 037-phase-4-pull-demo.c — values that are current, not merely correct.
 *
 * What this is: the phase 4 demonstration, about the direction of
 * flow. A pushed value was true once; a gathered value is true now.
 * Each scene measures one consequence: the same file read frozen and
 * fresh side by side, the tax gathering levies on delivery, the cost
 * of a chain, the cycle that is refused beside the silent death it
 * prevents, and a knob turned while the machine runs.
 *
 * How it does it, in general terms: maps place registry boxes, bind
 * statics and gatherers, and run while the main thread — registered
 * as an outside submitter — rewrites files and table entries in
 * mid-flight. The refused-cycle scene forks children so the reader
 * can see both fates: the check's message, and the messageless
 * signal the unchecked world dies of.
 *
 * The five stories are a station concourse, a shipping counter, a
 * chain of people passing a question along, two dictionaries that
 * define each other, and a dial on a factory wall. Each was chosen
 * for a different mechanic; see issue 707 for the contract they
 * answer to.
 */
#include "018-station.h"
#include "026-registry.h"
#include "060-demo-scene.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char watched_file[4096];

/* {{{ now_seconds() / write_file() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void write_file(const char *path, int value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fprintf(f, "%d\n", value);
    fclose(f);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: the same file, frozen and fresh.                        */
/*                                                                    */
/* Told as a printed timetable beside a departures board. Both are    */
/* true; only one is current. That distinction is the whole phase,    */
/* and a concourse is where most people have already met it.          */
/* ------------------------------------------------------------------ */

static _Atomic int frozen_last;
static _Atomic int fresh_last;
static _Atomic int both_arrived;

/* Harness sinks recording what each side reports. */
/* {{{ frozen_sink__call() */
static void frozen_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    frozen_last = x;
    both_arrived++;
}
/* }}} */

/* {{{ fresh_sink__call() */
static void fresh_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    fresh_last = x;
    both_arrived++;
}
/* }}} */

/* {{{ scene_frozen_versus_fresh() */
static void scene_frozen_versus_fresh(void)
{
    /* Two arbitrary numbers, drawn per run. What matters is only that
     * they differ and that the second arrives after the map is
     * already running. */
    int at_press = scene_pick(100, 199);
    int later = scene_pick(900, 999);

    write_file(watched_file, at_press);

    /* Two identical sub-graphs: add(0, file-value) -> sink. The
     * frozen side's file value was read at load and bound as a
     * static — true once. The fresh side gathers read_int_file at
     * every task — true now. */
    map_t *m = map_create(5);
    map_place_box(m, 0, "add", STATION_PLAIN);           /* frozen adder */
    map_place_box(m, 1, "add", STATION_PLAIN);           /* fresh adder */
    map_place_box(m, 2, "read_int_file", STATION_PLAIN); /* the gatherable */
    int one_int[1] = { sizeof(int) };
    map_place(m, 3, frozen_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_place(m, 4, fresh_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 3, 0);
    map_connect(m, 1, 0, 4, 0);

    map_statics_alloc(m, 2);
    /* The frozen side: the file as it stands at load, read here and
     * sealed into the table — the push-era way. */
    map_static_set_text(m, 0, scene_text("%d", at_press));
    map_slot_static(m, 0, 1, 0);
    /* The fresh side: the path is static; the value is pulled. */
    map_static_set_text(m, 1, watched_file);
    map_slot_static(m, 2, 0, 1);
    map_slot_gather(m, 1, 1, 2);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    scene_open(1, "a value that was true, and a value that is true");

    scene_problem(
        "An input slot can be filled two ways. A static slot is read "
        "once when the map is loaded and its value is sealed into the "
        "statics table, so every task built from then on carries the "
        "same copy. A gatherer slot has no stored value at all: when a "
        "task is assembled, the engine runs the source box and takes "
        "whatever it returns at that moment. Both are correct. They "
        "answer different questions, and this scene puts the same file "
        "behind both and then changes the file underneath them.");

    scene_imagine(
        "a station concourse holding two accounts of the same trains. "
        "One is a timetable printed overnight and pinned to the wall. "
        "The other is the departures board, which is not a record of "
        "anything — it is a question asked again every time somebody "
        "looks up.");

    scene_stands_for("a static slot", "the printed timetable",
                     "both were correct at one specific moment and are "
                     "unchanged since, which makes them reliable about "
                     "the past and silent about the present");
    scene_stands_for("a gatherer slot", "the departures board",
                     "neither stores an answer at all — both re-ask the "
                     "question every single time somebody needs one, so "
                     "what they give back cannot be out of date");
    scene_stands_for("sealing the statics table", "going to press",
                     "both are the irreversible moment when a value "
                     "stops tracking the world and becomes a copy of how "
                     "the world looked");
    scene_stands_for("assembling one task", "one traveller looking up",
                     "each is a separate occasion on which the value is "
                     "needed, and it is the number of those occasions "
                     "that decides how often the question gets asked");

    int zero = 0;
    both_arrived = 0;
    map_deliver_value(m, 0, 0, &zero);
    map_deliver_value(m, 1, 0, &zero);
    while (both_arrived < 2)
        usleep(1000);

    scene_measured("the world says",
                   scene_text("%d", at_press), "the file's contents");
    scene_measured("the timetable reports",
                   scene_text("%d", (int)frozen_last), NULL);
    scene_measured("the board reports",
                   scene_text("%d", (int)fresh_last), NULL);

    write_file(watched_file, later);
    both_arrived = 0;
    map_deliver_value(m, 0, 0, &zero);
    map_deliver_value(m, 1, 0, &zero);
    while (both_arrived < 2)
        usleep(1000);

    scene_blank();
    scene_line("the world changes underneath them both:");
    scene_measured("the world now says",
                   scene_text("%d", later), "the file, rewritten mid-run");
    scene_measured("the timetable reports",
                   scene_text("%d", (int)frozen_last),
                   "unchanged, and not wrong");
    scene_measured("the board reports",
                   scene_text("%d", (int)fresh_last),
                   "asked again, answered again");

    scene_finding(
        "A pushed value was true once. A gathered value is true now. "
        "The timetable did not fail — it answered the question it was "
        "built to answer, which is what the world looked like when the "
        "map was loaded. Choosing between them is a decision made per "
        "wire, and phase 4 exists so that it is a decision rather than "
        "an accident.");

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: the gather tax, measured.                               */
/*                                                                    */
/* Told as a rate card against a telephone. The cost being priced is  */
/* per-item and paid by whoever is standing at the counter, which is  */
/* the part people get wrong when they first meet gathering.          */
/* ------------------------------------------------------------------ */

static _Atomic int tax_runs;

/* {{{ tax_sink__call() */
static void tax_sink__call(task_t *t)
{
    (void)t;
    tax_runs++;
}
/* }}} */

/* {{{ run_tax_map() */
/* The same shape twice: add(x, seven) -> sink, with the seven either
 * a static (no pull) or gathered from the deliberately slow source.
 * The wall-clock difference per task is the tax. */
static double run_tax_map(int gathered, int values)
{
    map_t *m = map_create(3);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "slow_seven", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 2, tax_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 2, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, "7");
    if (gathered)
        map_slot_gather(m, 0, 1, 1);
    else
        map_slot_static(m, 0, 1, 0);

    map_start(m, 4);
    tax_runs = 0;

    /* The timer wraps the seeding too: gathering happens at task
     * assembly, which for seeded values is right here on this
     * thread — a subtlety this demo itself surfaced. The tax is paid
     * wherever the task is built, not where it runs. */
    double before = now_seconds();
    for (int i = 0; i < values; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;

    if (tax_runs != values) {
        fprintf(stderr, "tax map ran %d of %d\n", (int)tax_runs, values);
        exit(1);
    }
    map_destroy(m);
    return elapsed;
}
/* }}} */

/* {{{ scene_gather_tax() */
static void scene_gather_tax(void)
{
    int parcels = scene_pick(2500, 3500);
    double flat = run_tax_map(0, parcels);
    double taxed = run_tax_map(1, parcels);

    scene_open(2, "what freshness costs, and who pays it");

    scene_problem(
        "Gathering is not free and the bill lands somewhere specific. "
        "Every task built from a station with a gatherer slot runs that "
        "slot's source box during assembly — once per task, on whichever "
        "worker is doing the assembling, before the task is even "
        "queued. It is not amortised, not batched, and not done in the "
        "background. This scene prices it by building the same map "
        "twice, with the second input static and then gathered.");

    scene_imagine(
        "a shipping clerk pricing every parcel that crosses the "
        "counter. There is a rate card pinned above the desk, written "
        "out this morning, and there is a telephone to head office that "
        "will give the rate as it stands this second.");

    scene_stands_for("a static slot", "reading the rate card",
                     "both give an answer immediately and at no cost, "
                     "because the work of finding it out was done once, "
                     "earlier, for everybody");
    scene_stands_for("a gatherer slot", "telephoning head office",
                     "both get a current answer by doing real work at "
                     "the moment of asking, so the cost is paid again "
                     "for every single item");
    scene_stands_for("running the source box", "the call itself",
                     "each is the actual expense — however long that "
                     "takes is exactly how long the thing waiting on it "
                     "is held up");
    scene_stands_for("the worker assembling the task", "the clerk at the counter",
                     "in both cases the one making the call is the one "
                     "who was in the middle of something else, which is "
                     "why the queue behind them grows while they wait");

    scene_measured("parcels priced",
                   scene_text("%d parcels", parcels),
                   scene_text("%d tasks", parcels));
    scene_measured("with the rate card",
                   scene_text("%.2f ms all day", flat * 1e3),
                   scene_text("%.2f us each", flat * 1e6 / parcels));
    scene_measured("telephoning each time",
                   scene_text("%.2f ms all day", taxed * 1e3),
                   scene_text("%.2f us each", taxed * 1e6 / parcels));

    scene_finding(scene_text(
        "The call is charged once per parcel, and charged on the "
        "delivery path — to whichever worker is assembling the task, "
        "not to some helpful service in the background. That is exactly "
        "what \"true at the moment it is used\" costs, and here it came "
        "to roughly %.0f times the price of reading the card. It is "
        "also why a network request has no business behind a gatherer: "
        "the clerk would be on hold, and the queue at the counter is "
        "real.",
        taxed / (flat > 0 ? flat : 1)));
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: chain depth, recorded and measured.                   */
/*                                                                    */
/* Told as a question passed along a line of people. The point is not */
/* the waiting but that the line's length is known in advance, so the */
/* story needs an office with a chart on the wall.                    */
/* ------------------------------------------------------------------ */

/* {{{ scene_chain_depth() */
static void scene_chain_depth(void)
{
    int questions = scene_pick(2500, 3500);
    map_t *m = map_create(5);
    map_place_box(m, 0, "add", STATION_PLAIN);
    map_place_box(m, 1, "double_it", STATION_PLAIN);
    map_place_box(m, 2, "double_it", STATION_PLAIN);
    map_place_box(m, 3, "seven", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 4, tax_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 4, 0);
    map_slot_gather(m, 2, 0, 3);
    map_slot_gather(m, 1, 0, 2);
    map_slot_gather(m, 0, 1, 1);
    int depth = m->gather_depth;

    map_start(m, 4);
    tax_runs = 0;
    /* As in scene two, the walk happens at assembly — the timer
     * must cover the seeding that assembles. */
    double before = now_seconds();
    for (int i = 0; i < questions; i++)
        map_deliver_value(m, 0, 0, &i);
    pool_release(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;
    map_destroy(m);

    scene_open(3, "a cost that is known before anything runs");

    scene_problem(
        "A gatherer's source can itself have gatherer slots, so pulling "
        "one value can set off a chain of pulls. That could be alarming "
        "— an unbounded amount of work hiding behind a single slot — "
        "except that the shape of the chain is fixed by the wiring "
        "rather than by the data. The engine walks it when the wire is "
        "made and records the depth, so the worst case is a property of "
        "the map, available before a single value has moved.");

    scene_imagine(
        "an office where you need a number, so you ask the person whose "
        "job it is. They ask the person behind them, who asks the "
        "archive, and the answer comes back down the line it went up. "
        "The useful thing about this office is that the line's length "
        "is drawn on a chart, and the chart was drawn before anybody "
        "asked anything.");

    scene_stands_for("one gatherer pulling", "asking the person in front",
                     "each is a single hop that gets you no answer by "
                     "itself, only closer to somebody who might have one");
    scene_stands_for("a chain of gatherers", "the line of people",
                     "both are fixed in length and order before any "
                     "question is asked, and both are traversed all the "
                     "way to the end and back for every question");
    scene_stands_for("gather depth", "the chart on the wall",
                     "both are written down at the time the arrangement "
                     "is made rather than discovered when somebody is "
                     "already waiting, which is what makes the cost "
                     "predictable instead of merely survivable");
    scene_stands_for("task assembly", "standing there waiting",
                     "both are the single stretch of time during which "
                     "the whole chain is walked, blocking one worker "
                     "and one worker only");

    scene_measured("people in the line",
                   scene_text("%d deep", depth),
                   scene_text("gather depth %d", depth));
    scene_measured("questions asked",
                   scene_text("%d", questions),
                   scene_text("%d tasks", questions));
    scene_measured("waiting per question",
                   scene_text("%.2f us", elapsed * 1e6 / questions),
                   "the whole walk, top to bottom");

    scene_finding(
        "Depth is a property of the map rather than of the run: it is "
        "counted when the wire is made, not discovered when an answer "
        "is late. So the cost of a chain is bounded and knowable before "
        "anything executes — somebody can read the chart and decide the "
        "line is too long, which is a good conversation to have at "
        "wiring time and a terrible one to have at three in the "
        "morning.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: the refusal, and the silence it prevents.              */
/*                                                                    */
/* Told as two dictionaries that define each other. The failure being */
/* prevented is unbounded recursion, which has no symptoms until it   */
/* has no survivors — and a reference loop is the everyday shape of   */
/* exactly that.                                                      */
/* ------------------------------------------------------------------ */

/* {{{ endless_recursion() */
/*
 * The fate of an unchecked gather cycle, reproduced: each call is a
 * frame, nothing ever returns. The recursion hides behind a volatile
 * the compiler cannot fold, because the compiler — quite reasonably —
 * refuses to build a provably endless call.
 */
static void endless_recursion(volatile int *depth)
{
    volatile int frame[64];
    frame[0] = ++*depth;
    if (*depth >= 0)
        endless_recursion(depth);
    /* Touching the frame after the call keeps the recursion out of
     * tail position — otherwise the optimizer turns the doom into a
     * polite loop that never overflows anything. */
    *depth += frame[0];
}
/* }}} */

/* {{{ scene_refused_cycle() */
static void scene_refused_cycle(void)
{
    scene_open(4, "the wiring mistake with no symptoms");

    scene_problem(
        "If two gatherers are wired to pull from each other, assembling "
        "a task calls into a chain with no end. There is no error "
        "condition to detect at runtime: each call is individually "
        "legitimate, the recursion simply never bottoms out, and the "
        "process dies of a stack overflow with no message and no line "
        "number. So the engine walks the wiring at the moment a "
        "gatherer is connected and refuses the connection instead. This "
        "scene shows both fates side by side, in child processes, "
        "because one of them is fatal.");

    scene_imagine(
        "two dictionaries on a shelf, where the first defines every "
        "word by pointing at the second and the second defines every "
        "word by pointing at the first. Nothing is misspelled and no "
        "page is missing — a reader may follow references for the rest "
        "of their life without arriving at a meaning.");

    scene_stands_for("a gatherer pulling", "a definition that points elsewhere",
                     "each defers the actual answer to somewhere else, "
                     "which is useful exactly as long as somewhere else "
                     "eventually stops deferring");
    scene_stands_for("two gatherers pointing at each other",
                     "the circular pair of books",
                     "both are locally valid at every single step and "
                     "globally impossible, which is why nothing can "
                     "catch them by examining one step at a time");
    scene_stands_for("the cycle walk at connect time", "the librarian shelving",
                     "both inspect the whole arrangement at the one "
                     "moment when it is being changed, which is the only "
                     "moment when the problem is cheap to see and cheap "
                     "to fix");
    scene_stands_for("a stack overflow", "the reader who never comes out",
                     "both are what happens when nobody checked: no "
                     "message, no explanation, and nothing left to ask");

    /* First child: tries to wire two gatherers into each other. The
     * engine refuses with names; the child dies saying why. */
    int pipefd[2];
    if (pipe(pipefd) != 0) exit(1);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        map_t *m = map_create(2);
        map_place_box(m, 0, "double_it", STATION_PLAIN);
        map_place_box(m, 1, "double_it", STATION_PLAIN);
        map_slot_gather(m, 0, 0, 1);
        map_slot_gather(m, 1, 0, 0);
        _exit(0);
    }
    close(pipefd[1]);
    char message[512] = {0};
    size_t filled = 0;
    for (;;) {
        ssize_t got = read(pipefd[0], message + filled,
                           sizeof message - 1 - filled);
        if (got <= 0)
            break;
        filled += (size_t)got;
        if (filled >= sizeof message - 1)
            break;
    }
    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);

    /* Trailing newline trimmed: the presenter ends its own lines, and
     * the engine's message arrives with one already attached. */
    if (filled > 0 && message[filled - 1] == '\n')
        message[filled - 1] = '\0';

    scene_blank();
    scene_line("what the librarian says, verbatim:");
    scene_line("%s", filled > 0 ? message : "(nothing)");

    /* Second child: what the same map would do without the check —
     * unbounded recursion on a modest stack. It dies of a signal,
     * with no message, no line, no names. That silence is what the
     * walk was bought to prevent. */
    pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        struct rlimit small = { 1 << 20, 1 << 20 };
        setrlimit(RLIMIT_STACK, &small);
        volatile int depth = 0;
        endless_recursion(&depth);
        _exit(0);
    }
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status))
        scene_measured("without the librarian",
                       scene_text("killed by signal %d", WTERMSIG(status)),
                       "no message, no names");

    scene_finding(
        "Both of those are failures and only one is survivable. The "
        "refusal names both stations and the reason, and it costs a "
        "single walk over the wiring at the moment a wire is made — "
        "before anything runs at all. The alternative is a stack "
        "overflow in production with nothing in the log, which is the "
        "same mistake wearing a disguise nobody can see through.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene five: a knob turned while it runs.                           */
/*                                                                    */
/* Told as a dial on a factory wall, because the surprising part is   */
/* not that the value changes but that nothing stops while it does —  */
/* and a factory is a place where stopping would be conspicuous.      */
/* ------------------------------------------------------------------ */

static _Atomic long knob_sum;
static _Atomic int knob_seen;

/* {{{ knob_sink__call() */
static void knob_sink__call(task_t *t)
{
    int x;
    memcpy(&x, t->in[0], sizeof x);
    knob_sum += x;
    knob_seen++;
}
/* }}} */

/* {{{ scene_knob_turned() */
static void scene_knob_turned(void)
{
    int setting = scene_pick(500, 1500);
    int lowest = setting;
    int highest = setting;
    int turns = 0;
    long produced_before_first_turn = 0;
    int zero = 0;
    long last_output = 0;

    map_t *m = map_create(2);
    map_place_box(m, 0, "add", STATION_PLAIN);
    int one_int[1] = { sizeof(int) };
    map_place(m, 1, knob_sink__call, STATION_PLAIN, 1, one_int, 0);
    map_connect(m, 0, 0, 1, 0);

    map_statics_alloc(m, 1);
    map_static_set_text(m, 0, scene_text("%d", setting));
    map_slot_static(m, 0, 1, 0);

    map_start(m, 4);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    knob_sum = 0;
    knob_seen = 0;

    scene_open(5, "changing a value the whole map reads, without stopping");

    scene_problem(
        "The statics table is one array the whole map shares, and every "
        "task assembled from a static slot takes a copy out of it. "
        "Writing an entry mid-run therefore changes what every "
        "subsequent task is built with — and changes nothing about the "
        "tasks already built, which carry copies of their own. The "
        "write goes through the same mutex a box would use, so it does "
        "not need the map to be stopped, drained, or reloaded. Turn the "
        "dial below and watch where the change shows up.");

    scene_imagine(
        "a factory line running all day with a dial on the wall that "
        "sets one number every machine reads. Somebody walks over "
        "mid-shift and turns it. Nothing stops, nothing restarts, and "
        "nobody is told — the only evidence is a bend in the "
        "measurements coming off the end.");

    scene_stands_for("one entry in the statics table", "the dial on the wall",
                     "both are a single shared setting in a known place "
                     "that anything may read at any time, and neither "
                     "belongs to any one machine");
    scene_stands_for("a write through the statics mutex", "turning the dial",
                     "both take effect immediately and completely, and "
                     "both are safe to do while everything is running "
                     "because there is exactly one of them and only one "
                     "hand can be on it at a time");
    scene_stands_for("a task carrying its own copies", "a part already on the line",
                     "each was made to the setting that was in force "
                     "when it started, and no later change can reach "
                     "back and alter something already made");
    scene_stands_for("an output value", "a measurement off the end of the line",
                     "both are the only externally visible sign that "
                     "anything happened at all, which is what makes them "
                     "the honest place to look for the effect");

    scene_live_begin();
    scene_live_key('1', "turn the dial up");
    scene_live_key('2', "turn the dial down");

    if (!scene_interactive()) {
        /* A scripted turn, so a redirected run still shows the bend. */
        int i;
        for (i = 0; i < 200; i++)
            map_deliver_value(m, 0, 0, &zero);
        usleep(50000);
        produced_before_first_turn = knob_seen;
        setting = scene_pick(4000, 6000);
        highest = setting;
        map_static_write(m, 0, &setting, sizeof setting);
        turns = 1;
        for (i = 0; i < 200; i++)
            map_deliver_value(m, 0, 0, &zero);
        usleep(50000);
    }

    do {
        int key = scene_live_pressed();
        int i;

        if (key == '1' || key == '2') {
            if (key == '1')
                setting += 500;
            else
                setting = setting > 500 ? setting - 500 : 0;
            if (setting < lowest) lowest = setting;
            if (setting > highest) highest = setting;
            if (!turns)
                produced_before_first_turn = knob_seen;
            turns++;
            /* The turn — through the same call a box could make. */
            map_static_write(m, 0, &setting, sizeof setting);
        }

        /* A trickle of work, so there is always something being made
         * to whatever the dial currently says. */
        for (i = 0; i < 20; i++)
            map_deliver_value(m, 0, 0, &zero);

        if (knob_seen > 0)
            last_output = knob_sum / knob_seen;

        scene_frame_begin();
        scene_frame_line("a line that never stops, and a dial anyone may "
                         "turn while it runs");
        scene_frame_blank();
        scene_bar("the dial", highest ? (double)setting / (highest + 500) : 0.0,
                  scene_text("%d", setting));
        scene_frame_blank();
        scene_frame_line("parts made so far    %d", (int)knob_seen);
        scene_frame_line("latest part measures %d", (int)(setting));
        scene_frame_line("running mean of all  %ld", last_output);
        scene_frame_line("times the dial moved %d", turns);
        scene_frame_end();
    } while (!scene_live_done());

    scene_live_end();

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    scene_measured("dial started at",
                   scene_text("%d", lowest == highest ? setting : lowest),
                   "the value loaded from the map");
    scene_measured("times it was turned",
                   scene_text("%d", turns),
                   "writes through the statics mutex");
    scene_measured("dial ended at",
                   scene_text("%d", setting),
                   "mid-run, nothing stopped");
    scene_measured("parts made",
                   scene_text("%d", (int)knob_seen),
                   scene_text("%ld before the first turn",
                              produced_before_first_turn));
    scene_measured("mean across the run",
                   scene_text("%ld", last_output),
                   "which no single setting produced");

    scene_finding(
        "The write went through the same call a box makes, took the "
        "same mutex, and became visible to every task assembled after "
        "it. Tasks already built kept the value they were built with — "
        "which is not a compromise but the definition: a task carries "
        "its own copies, so a dial turned now cannot reach backwards "
        "into work already in flight. That is also why the mean across "
        "the whole run matches no setting the dial ever held.");
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root>\n", argv[0]);
        return 1;
    }

    snprintf(watched_file, sizeof watched_file,
             "%s/tmp/shared-memory/phase-4-watched.txt", argv[1]);

    demo_open(argv[1], "phase-4-pull-report.txt",
              "=== phase 4 demo: values that are current, "
              "not merely correct ===");

    scene_frozen_versus_fresh();
    scene_gather_tax();
    scene_chain_depth();
    scene_refused_cycle();
    scene_knob_turned();

    demo_close("=== pushed: true once. gathered: true now. "
               "both on purpose ===");

    unlink(watched_file);
    return 0;
}
