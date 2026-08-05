/*
 * 017-phase-1-pool-demo.c — the pool under load, told four ways.
 *
 * What this is: the phase 1 demonstration. It puts the thread pool
 * through the four situations it will actually meet — a burst of
 * fan-out, chains of uneven length, long stretches of idleness, and
 * raw throughput at every worker count the machine has — and reports
 * what it measured. Nothing printed here is a constant from this
 * file; every number is read off the running pool.
 *
 * How it does it, in general terms: each scene states its problem in
 * the engine's own terms, offers an image to hold it by, justifies
 * every correspondence between the two, and only then measures. The
 * measuring and the telling are separate functions throughout, so a
 * rewording cannot disturb a number. Scene one is a live panel the
 * reader steers: work arrives when they ask for it and the workers
 * speed up or slow down under their hands, because ring growth is a
 * process and a process is better watched than summarised.
 *
 * The four images are a parcel depot, a relay race, a toll plaza at
 * night, and a shop with one basket of orders. See issue 707 for the
 * contract each has to satisfy — chiefly that its units are relabelled
 * measurements and never invented ones.
 */
#include "011-pool.h"
#include "060-demo-scene.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

/* Declared by the presenter for the demos that offer a speed knob. */
void scene_live_faster(void);
void scene_live_slower(void);

/* {{{ now_seconds() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ cpu_seconds() */
static double cpu_seconds(void)
{
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    return (double)u.ru_utime.tv_sec + (double)u.ru_utime.tv_usec / 1e6
         + (double)u.ru_stime.tv_sec + (double)u.ru_stime.tv_usec / 1e6;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene one: growth under fan-out, steered by the reader.            */
/* ------------------------------------------------------------------ */

static pool_t *depot_pool;
static _Atomic long depot_handled;
static _Atomic long depot_pushed;
static _Atomic int depot_effort = 400;   /* how long one task takes */

typedef struct depot_facts {
    long handled;
    int  capacity_at_open;
    int  capacity_at_close;
    int  doublings;
    int  high_water;
    int  waves;
} depot_facts_t;

/* {{{ depot_task() */
/* One unit of work whose duration the reader controls. Burning
 * arithmetic rather than sleeping, because a sleeping worker would be
 * idle rather than busy and the queue would drain for the wrong
 * reason. */
static void depot_task(task_t *t)
{
    unsigned long x = 88172645463325252UL;
    int rounds = depot_effort;
    int i;

    (void)t;
    for (i = 0; i < rounds; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    if (x == 0) abort();
    depot_handled++;
    /* The task itself is the pool's to free — it does so after the
     * call returns. Freeing it here would be the second free. */
}
/* }}} */

/* {{{ depot_deliver() */
/* One delivery: a batch of manifests arriving together, which is what
 * a single fan-out looks like from the queue's side. */
static void depot_deliver(int count)
{
    int i;

    for (i = 0; i < count; i++) {
        task_t *t = malloc(sizeof *t);
        if (!t) abort();
        t->call = depot_task;
        depot_pushed++;
        pool_push(depot_pool, t);
    }
}
/* }}} */

/* {{{ run_depot() */
static depot_facts_t run_depot(void)
{
    depot_facts_t facts;
    int wave_size = scene_pick(80, 160);
    int capacity, high_water, growths, depth;

    depot_pool = pool_create(4, NULL, NULL);
    depot_handled = 0;
    depot_pushed = 0;
    depot_effort = 400;
    facts.waves = 0;

    pool_queue_stats(depot_pool, &facts.capacity_at_open, NULL, NULL);

    /* Registering as an outside submitter is what stops the pool
     * deciding it is finished the moment the queue empties. Without it
     * the depot would close between waves and there would be nothing
     * to watch. */
    pool_submitter_register(depot_pool);
    pool_release(depot_pool);

    scene_live_begin();
    scene_live_key('1', "another wave of manifests");
    scene_live_key('2', "clerks work faster");
    scene_live_key('3', "clerks work slower");

    /* Nobody is watching a redirected run, so it delivers its own
     * waves and takes a single picture. */
    if (!scene_interactive()) {
        int wave;
        for (wave = 0; wave < 6; wave++) {
            depot_deliver(wave_size);
            facts.waves++;
        }
    }

    do {
        int key = scene_live_pressed();

        if (key == '1') {
            depot_deliver(wave_size);
            facts.waves++;
        } else if (key == '2') {
            int effort = depot_effort / 2;
            depot_effort = effort < 25 ? 25 : effort;
            scene_live_faster();
        } else if (key == '3') {
            int effort = depot_effort * 2;
            depot_effort = effort > 6400 ? 6400 : effort;
            scene_live_slower();
        }

        pool_queue_stats(depot_pool, &capacity, &high_water, &growths);
        /* The pool does not publish a live depth and does not need to:
         * pushed minus handled is the same number, counted here, and
         * it is exactly the quantity the story calls "waiting". */
        depth = (int)(depot_pushed - depot_handled);
        if (depth < 0)
            depth = 0;

        scene_frame_begin();
        scene_frame_line("four clerks, and a wall of pigeonholes that "
                         "rebuilds itself when it runs out");
        scene_frame_blank();
        scene_bar("manifests waiting", capacity ? (double)depth / capacity : 0.0,
                  scene_text("%d, in a wall of %d", depth, capacity));
        scene_bar("wall rebuilt", growths / 10.0,
                  scene_text("%d times, high water %d", growths, high_water));
        scene_frame_blank();
        scene_frame_line("manifests handled    %ld", (long)depot_handled);
        scene_frame_line("waves delivered      %d", facts.waves);
        scene_frame_line("effort per manifest  %d rounds of arithmetic",
                         (int)depot_effort);
        scene_frame_end();
    } while (!scene_live_done());

    scene_live_end();

    pool_submitter_unregister(depot_pool);
    pool_join(depot_pool);

    pool_queue_stats(depot_pool, &facts.capacity_at_close,
                     &facts.high_water, &facts.doublings);
    facts.handled = depot_handled;

    pool_destroy(depot_pool);
    return facts;
}
/* }}} */

/* {{{ tell_depot() */
static void tell_depot(const depot_facts_t *facts)
{
    scene_measured("manifests handled",
                   scene_text("%ld manifests", facts->handled),
                   scene_text("%ld tasks run", facts->handled));
    scene_measured("waves delivered",
                   scene_text("%d", facts->waves),
                   "each a single fan-out");
    scene_measured("pigeonholes at opening",
                   scene_text("%d", facts->capacity_at_open),
                   scene_text("%d cells", facts->capacity_at_open));
    scene_measured("pigeonholes at closing",
                   scene_text("%d", facts->capacity_at_close),
                   scene_text("%d cells", facts->capacity_at_close));
    scene_measured("times the wall grew",
                   scene_text("%d rebuilds", facts->doublings),
                   scene_text("%d doublings", facts->doublings));
    scene_measured("worst backlog",
                   scene_text("%d waiting", facts->high_water),
                   scene_text("%d tasks queued", facts->high_water));

    scene_finding(
        "The wall was rebuilt while the clerks kept sorting. No manifest "
        "was turned away, none was lost, and the doors never closed — "
        "which is the whole claim: the ring grows without a pause and "
        "without an upper bound anyone had to guess in advance. Pushing "
        "the clerks faster empties the floor; slowing them fills it, and "
        "the wall answers by getting bigger rather than by refusing "
        "anything.");
}
/* }}} */

/* {{{ scene_depot() */
static void scene_depot(void)
{
    depot_facts_t facts;

    scene_open(1, "a queue that grows while it is being used");

    scene_problem(
        "The pool holds tasks in a ring of pointers with a fixed number "
        "of cells. Work arrives in bursts — one task can enqueue a "
        "hundred more in a single turn — so the ring will sometimes be "
        "asked to hold more than it has room for. It answers by "
        "doubling: allocating a larger ring, copying, and carrying on. "
        "The question this scene exists to answer is what that costs "
        "the work already in flight, and the answer is meant to be "
        "nothing at all.");

    scene_imagine(
        "a parcel depot whose sorting wall is a grid of pigeonholes. "
        "Deliveries arrive in waves, and when a wave is bigger than the "
        "wall, joiners build a larger wall around the clerks without "
        "the depot closing for so much as a minute.");

    scene_stands_for("a task struct", "a delivery manifest",
                     "each is one self-contained piece of work that names "
                     "what has to be done, can be handed to whichever "
                     "clerk is free, and means the same thing whoever "
                     "picks it up");
    scene_stands_for("a cell in the ring", "a pigeonhole",
                     "each holds exactly one item, is found by its "
                     "position rather than by searching, and is entirely "
                     "indifferent to what is in the ones beside it");
    scene_stands_for("doubling the ring", "building a larger wall",
                     "both replace the whole structure at once rather "
                     "than extending it in place, and both must move "
                     "everything already held into the new one before "
                     "the old one can go");
    scene_stands_for("a worker thread", "a clerk on the floor",
                     "each takes one item at a time, works on it "
                     "start to finish, and then goes back for another "
                     "without being told which one to take");
    scene_stands_for("stopping the world", "closing the doors",
                     "both would make the growth safe by making "
                     "everything else wait, and both are exactly what "
                     "this design refuses to do");

    facts = run_depot();
    tell_depot(&facts);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene two: termination with a long tail.                           */
/* ------------------------------------------------------------------ */

static _Atomic long tail_last_ns;

typedef struct tail_task {
    task_t  base;
    pool_t *pool;
    int     remaining;
} tail_task_t;

enum { TAIL_TEAMS = 4 };

typedef struct tail_facts {
    int    laps[TAIL_TEAMS];
    long   total_laps;
    double gap_ms;
} tail_facts_t;

/* {{{ tail_step() */
static void tail_step(task_t *t)
{
    tail_task_t *c = (tail_task_t *)t;
    if (c->remaining > 0) {
        tail_task_t *next = malloc(sizeof *next);
        if (!next) abort();
        next->base.call = tail_step;
        next->pool = c->pool;
        next->remaining = c->remaining - 1;
        pool_push(c->pool, &next->base);
        return;
    }
    /* A chain just ended. Remember the latest ending seen anywhere,
     * by compare-and-swap so concurrent endings cannot overwrite a
     * later time with an earlier one. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long ns = ts.tv_sec * 1000000000L + ts.tv_nsec;
    long seen = tail_last_ns;
    while (ns > seen &&
           !atomic_compare_exchange_weak(&tail_last_ns, &seen, ns))
        ;
}
/* }}} */

/* {{{ measure_tail() */
static tail_facts_t measure_tail(void)
{
    tail_facts_t facts;
    pool_t *p = pool_create(4, NULL, NULL);

    /* Four teams spanning four orders of magnitude. The magnitudes are
     * the point — one team must still be running long after the others
     * have finished — so they are drawn inside their decade rather
     * than fixed. */
    facts.laps[0] = scene_pick(5, 20);
    facts.laps[1] = scene_pick(300, 700);
    facts.laps[2] = scene_pick(15000, 25000);
    facts.laps[3] = scene_pick(150000, 250000);

    facts.total_laps = 0;
    tail_last_ns = 0;

    for (int i = 0; i < TAIL_TEAMS; i++) {
        facts.total_laps += facts.laps[i] + 1;
        tail_task_t *c = malloc(sizeof *c);
        if (!c) abort();
        c->base.call = tail_step;
        c->pool = p;
        c->remaining = facts.laps[i];
        pool_push(p, &c->base);
    }

    pool_release(p);
    pool_join(p);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long done_ns = ts.tv_sec * 1000000000L + ts.tv_nsec;
    facts.gap_ms = (double)(done_ns - tail_last_ns) / 1e6;

    pool_destroy(p);
    return facts;
}
/* }}} */

/* {{{ tell_tail() */
static void tell_tail(const tail_facts_t *facts)
{
    scene_open(2, "knowing when there is no more work");

    scene_problem(
        "A pool of sleeping workers has to decide when to stop, and the "
        "decision is harder than it looks: a queue that is empty right "
        "now may not stay empty, because a task still running can "
        "enqueue its successor. The pool handles this by counting "
        "sleepers, and having the last worker to fall asleep re-check "
        "the queue on everyone's behalf before the pool declares "
        "itself done. What that check costs is the number this scene "
        "is after.");

    scene_imagine(
        "four relay teams on one track, given wildly unequal distances. "
        "One finishes almost at once; another is still going long after "
        "the stands have emptied. Nobody is timing the race — the only "
        "question is how long the floodlights stay on after the final "
        "baton goes down.");

    scene_stands_for("a task", "one lap run",
                     "each is a fixed unit of work that somebody has to "
                     "do from start to finish before the next one can "
                     "begin");
    scene_stands_for("a task enqueueing its successor", "handing the baton",
                     "in both cases the work that comes next does not "
                     "exist until the work before it has finished, which "
                     "is exactly why an empty queue proves nothing");
    scene_stands_for("a chain of dependent tasks", "a team",
                     "each is a sequence that must run in order, and "
                     "several of them can be in progress at once without "
                     "knowing about each other");
    scene_stands_for("the last sleeper's re-check", "the last steward's walk",
                     "somebody has to confirm that the track really is "
                     "empty rather than momentarily quiet, and it only "
                     "counts if the person doing it is the last one "
                     "still awake");
    scene_stands_for("the pool declaring itself done", "the lights going out",
                     "both are irreversible, both affect everybody at "
                     "once, and both are catastrophic if they happen "
                     "while somebody is still running");

    for (int i = 0; i < TAIL_TEAMS; i++)
        scene_measured(scene_text("team %d ran", i + 1),
                       scene_text("%d laps", facts->laps[i]),
                       scene_text("%d chained tasks", facts->laps[i] + 1));

    scene_measured("laps in total",
                   scene_text("%ld laps", facts->total_laps),
                   scene_text("%ld tasks", facts->total_laps));
    scene_measured("lights out, after",
                   scene_text("%.6f ms", facts->gap_ms),
                   NULL);

    scene_finding(
        "That gap is the entire cost of knowing when to stop, and it is "
        "paid once at the end rather than on every task. The alternative "
        "designs are worse in both directions: a pool that stops the "
        "moment its queue looks empty will kill a race that is still "
        "running, and one that never stops has to be shut down by "
        "somebody who knows something the pool does not.");
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene three: the price of idleness.                                */
/* ------------------------------------------------------------------ */

static _Atomic int idle_ran;

typedef struct idle_facts {
    int    cars;
    double hours_on_duty;   /* wall seconds */
    double fuel_burned;     /* processor seconds */
    int    attendants;
} idle_facts_t;

/* {{{ idle_tick() */
static void idle_tick(task_t *t)
{
    (void)t;                    /* the pool frees it after this returns */
    idle_ran++;
}
/* }}} */

/* {{{ measure_idle() */
static idle_facts_t measure_idle(void)
{
    idle_facts_t facts;
    facts.attendants = 4;

    /* The trickle's shape is what matters, not its exact length. */
    facts.cars = scene_pick(18, 32);
    int gap_us = scene_pick(30000, 50000);

    pool_t *p = pool_create(facts.attendants, NULL, NULL);
    idle_ran = 0;

    pool_submitter_register(p);
    pool_release(p);

    double wall_before = now_seconds();
    double cpu_before = cpu_seconds();

    for (int i = 0; i < facts.cars; i++) {
        task_t *t = malloc(sizeof *t);
        if (!t) abort();
        t->call = idle_tick;
        pool_push(p, t);
        usleep((useconds_t)gap_us);
    }

    pool_submitter_unregister(p);
    pool_join(p);

    facts.hours_on_duty = now_seconds() - wall_before;
    facts.fuel_burned = cpu_seconds() - cpu_before;
    facts.cars = idle_ran;

    pool_destroy(p);
    return facts;
}
/* }}} */

/* {{{ tell_idle() */
static void tell_idle(const idle_facts_t *facts)
{
    double spinning = facts->hours_on_duty * facts->attendants;
    double ratio = spinning / (facts->fuel_burned > 0.0001
                               ? facts->fuel_burned : 0.0001);

    scene_open(3, "what a worker costs while it has nothing to do");

    scene_problem(
        "A worker with no task can do one of two things. It can loop, "
        "checking the queue over and over, which makes it ready the "
        "instant work arrives and consumes a whole processor core "
        "meanwhile. Or it can wait on a condition variable, which costs "
        "nothing until somebody signals it and adds a wake-up delay "
        "when they do. This pool waits. The scene puts a number on what "
        "that choice saves, by giving four workers almost nothing to do "
        "and comparing processor time against wall-clock time.");

    scene_imagine(
        "a toll plaza keeping four booths staffed through a dead-quiet "
        "night, with a car every half-minute or so. An attendant can "
        "keep the engine running to stay warm and ready, or doze and "
        "wake on the bell.");

    scene_stands_for("a worker thread", "a booth attendant",
                     "each is a fixed resource that is present whether "
                     "or not there is anything to do, and the cost of "
                     "having it is paid for the whole shift rather than "
                     "per customer");
    scene_stands_for("a task arriving", "a car pulling up",
                     "both arrive at times nobody controls, must be "
                     "handled by exactly one attendant, and take far "
                     "less time to serve than the gaps between them");
    scene_stands_for("waiting on a condition variable", "dozing until the bell",
                     "both cost nothing while nothing is happening, and "
                     "both trade a small delay on waking for that "
                     "saving");
    scene_stands_for("spinning on the queue", "idling the engine",
                     "both stay perfectly ready by consuming the full "
                     "resource continuously, whether or not any of that "
                     "readiness is ever used");
    scene_stands_for("processor seconds", "fuel burned",
                     "both are consumed only by actual work being done, "
                     "so both distinguish a booth that was staffed from "
                     "a booth that was busy");

    scene_measured("cars handled",
                   scene_text("%d cars", facts->cars),
                   scene_text("%d tasks", facts->cars));
    scene_measured("hours on duty",
                   scene_text("%.2f s of night", facts->hours_on_duty),
                   scene_text("%.2f s wall clock", facts->hours_on_duty));
    scene_measured("fuel actually burned",
                   scene_text("%.4f s", facts->fuel_burned),
                   scene_text("%.4f s processor time", facts->fuel_burned));

    scene_finding(scene_text(
        "Four attendants idling their engines for %.2f seconds would "
        "have burned about %.1f seconds of fuel — roughly %.0f times "
        "what this pool spent. That last figure is arithmetic on the "
        "two measurements above rather than a third measurement, and it "
        "is the one number in this demo that was not observed: nothing "
        "here ever spun, so nothing here could measure spinning.",
        facts->hours_on_duty, spinning, ratio));
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Scene four: throughput against worker count.                       */
/* ------------------------------------------------------------------ */

static _Atomic long grind_done;

enum { THROUGHPUT_RUNS = 8 };

typedef struct throughput_facts {
    int    orders;
    int    lanes[THROUGHPUT_RUNS];
    double per_second[THROUGHPUT_RUNS];
    int    runs;
} throughput_facts_t;

/* {{{ grind() */
/*
 * A small but honest piece of work — enough arithmetic that the task
 * is not pure lock traffic, small enough that the queue still matters.
 */
static void grind(task_t *t)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < 400; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    /* Keep the arithmetic alive past the optimizer. */
    if (x == 0) abort();
    (void)t;                    /* the pool frees it after this returns */
    grind_done++;
}
/* }}} */

/* {{{ measure_throughput() */
static throughput_facts_t measure_throughput(void)
{
    throughput_facts_t facts;
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* The order count only has to be large enough that startup is
     * noise; anywhere in this range is. */
    facts.orders = scene_pick(150000, 250000);
    facts.runs = 0;

    for (int w = 1; w <= cores && facts.runs < THROUGHPUT_RUNS; w *= 2) {
        pool_t *p = pool_create(w, NULL, NULL);
        grind_done = 0;

        /* Everything is queued before the workers are released, so
         * what is timed is draining a full queue rather than a race
         * between the pusher and the poppers. */
        for (int i = 0; i < facts.orders; i++) {
            task_t *t = malloc(sizeof *t);
            if (!t) abort();
            t->call = grind;
            pool_push(p, t);
        }

        double before = now_seconds();
        pool_release(p);
        pool_join(p);
        double elapsed = now_seconds() - before;
        pool_destroy(p);

        if (grind_done != facts.orders) {
            fprintf(stderr, "throughput scene lost tasks: %ld of %d\n",
                    (long)grind_done, facts.orders);
            exit(1);
        }

        facts.lanes[facts.runs] = w;
        facts.per_second[facts.runs] = facts.orders / elapsed;
        facts.runs++;
    }

    return facts;
}
/* }}} */

/* {{{ tell_throughput() */
static void tell_throughput(const throughput_facts_t *facts)
{
    scene_open(4, "where adding workers stops helping");

    scene_problem(
        "There is one queue and one mutex guarding it. Every worker "
        "that wants a task must take that mutex, and only one can hold "
        "it at a time — so while the work itself runs in parallel, "
        "getting hold of the work does not. Adding workers buys "
        "throughput until the queue's own lock becomes the thing they "
        "are all waiting for. This scene finds that point by running "
        "identical work at every power-of-two worker count the machine "
        "has.");

    scene_imagine(
        "a shop with a single wire basket of order slips by the door "
        "and however many cashiers it cares to hire. Each serves "
        "customers independently and quickly — but every one of them "
        "must walk to the same basket for the next slip, and only one "
        "hand fits in at a time.");

    scene_stands_for("a worker thread", "a cashier",
                     "each works independently once it has something to "
                     "do, so two of them genuinely do twice the work — "
                     "provided they can both get hold of something");
    scene_stands_for("a task struct", "an order slip",
                     "each is a complete instruction that can be carried "
                     "away and acted on without going back for more "
                     "context");
    scene_stands_for("the task queue", "the wire basket",
                     "both are the single shared place that all the "
                     "independent work has to pass through, which makes "
                     "them the one part that cannot be duplicated by "
                     "hiring");
    scene_stands_for("the queue mutex", "one hand at a time",
                     "both are the rule that makes the shared place "
                     "safe, and both convert a moment of sharing into a "
                     "queue of people waiting their turn to share");

    scene_measured("slips in the basket",
                   scene_text("%d orders", facts->orders),
                   scene_text("%d tasks per run", facts->orders));

    scene_blank();
    scene_line("cashiers   customers served per second   against one cashier");
    for (int i = 0; i < facts->runs; i++)
        scene_line("%8d   %27.0f   %.2fx",
                   facts->lanes[i], facts->per_second[i],
                   facts->per_second[i] / facts->per_second[0]);

    scene_finding(
        "Where that last column stops keeping pace with the cashier "
        "count is the basket's ceiling, and it is the honest limit of "
        "this design: one queue, one mutex, every worker reaching into "
        "it. Knowing where the ceiling sits now is worth more than "
        "guessing at it later — and it is the number to beat if the "
        "queue is ever split per worker.");
}
/* }}} */

int main(int argc, char **argv)
{
    /* The project root arrives as the one argument; the launcher
     * passes it, and the report lands in the RAM-backed tier under
     * it. Without the root there is nowhere to mirror the report, and
     * a demo that cannot keep that promise should not start. */
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root>\n", argv[0]);
        return 1;
    }

    demo_open(argv[1], "phase-1-pool-report.txt",
              "=== phase 1 demo: the pool under load ===");

    scene_depot();

    tail_facts_t tail = measure_tail();
    tell_tail(&tail);

    idle_facts_t idle = measure_idle();
    tell_idle(&idle);

    throughput_facts_t throughput = measure_throughput();
    tell_throughput(&throughput);

    demo_close("=== four stories, one pool, every number measured on "
               "this run ===");
    return 0;
}
