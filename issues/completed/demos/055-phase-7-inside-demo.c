/*
 * 055-phase-7-inside-demo.c — watching it think.
 *
 * What this is: the phase 7 demonstration, and the last one. Every
 * earlier demo reported what happened; this one shows it happening,
 * and then changes it without stopping. The claim on trial: the
 * engine has no hidden state — everything it is doing can be seen,
 * and most of it can be altered.
 *
 * How it does it, in general terms: one map, loaded from text, run
 * under a live view drawn from the station table while values flow
 * in from outside. A deliberate bottleneck is found using only the
 * engine's own reports, relieved by moving a wire mid-run, and both
 * readings shown on camera. The dump proves the picture and the
 * engine agree; a refused rewire proves a bad instruction cannot
 * kill the plant.
 *
 * This is the compiled half. It joins the report the driving script
 * opened, because the script owns scene six — the instrumentation's
 * own cost, which needs this program compiled twice and cannot be
 * measured from inside either copy.
 *
 * The five stories here are a control room, a coned-off lane, a set
 * of as-built drawings, a translation checked by translating back,
 * and a safety interlock. See issue 707 for the contract they answer
 * to.
 *
 * With --overhead, runs a bare throughput loop instead and prints one
 * line; the script runs that mode from both builds.
 */
#include "040-mapfile.h"
#include "049-observe.h"
#include "060-demo-scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* {{{ now_seconds() / write_text() */
static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) exit(1);
    fputs(text, f);
    fclose(f);
}
/* }}} */

/*
 * The demo map. A pump spreads work through an iterator whose two
 * ports BOTH feed the slow cruncher — the deliberate bottleneck —
 * while an identical spare idles. Two gather-capable sevens exist so
 * the refused-cycle scene has real stations to name.
 *
 * The idle spare is what makes the loader warn at startup, before
 * scene one: it has a buffered input no arrow feeds. That warning is
 * correct and the demo provokes it on purpose, so the driving script
 * says so before this program runs rather than leaving a reader to
 * wonder what went wrong.
 *
 * Stations by index: 0 head, 1 pump, 2 crunch, 3 spare, 4 drain,
 * 5 fresha, 6 ga, 7 gb.
 */
static const char *MAP_TEXT =
    "head seven p\n"
    "  out 0 - pump.0\n"
    "pump keep i\n"
    "  out 0 - crunch.0\n"
    "  out 1 - crunch.0\n"
    "spare slow_double p\n"
    "  out 0 - drain.0\n"
    "crunch slow_double p\n"
    "  out 0 - drain.0\n"
    "drain swallow p\n"
    "fresha seven p\n"
    "ga keep p\n"
    "  in 0 fresha\n"
    "gb keep p\n"
    "  in 0 ga\n";

enum { S_HEAD = 0, S_PUMP = 1, S_SPARE = 2, S_CRUNCH = 3, S_DRAIN = 4,
       S_FRESHA = 5, S_GA = 6, S_GB = 7 };

/* {{{ live_view() */
/* The gauges, read off the station table rather than off the file —
 * which is the whole point of the scene it belongs to. */
static void live_view(map_t *m, const char *moment)
{
    scene_line("[%s]", moment);
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        char bar[32];
        char depths[64];

        depths[0] = '\0';
        bar[0] = '\0';
        for (int j = 0; j < s->n_slots; j++)
            if (s->slots[j].kind == SLOT_RING) {
                int depth = map_slot_depth(m, i, j);
                int hashes = depth / 20;
                if (hashes > 24)
                    hashes = 24;
                for (int h = 0; h < hashes; h++)
                    bar[h] = '#';
                bar[hashes] = '\0';
                snprintf(depths, sizeof depths, "waiting %-5d", depth);
            }

        scene_line("  %-9s runs %-7ld %s%s",
                   m->station_names[i], (long)s->runs, depths, bar);
    }
}
/* }}} */

/* {{{ scene_overhead() — the script's second compile runs this */
static void scene_overhead(const char *work_dir)
{
    /* Long enough for stable numbers; short runs measure the wind. */
    enum { VALUES = 30000 };
    char map_path[1024];
    snprintf(map_path, sizeof map_path, "%s/overhead.map", work_dir);
    write_text(map_path,
        "head seven p\n"
        "  out 0 - relay.0\n"
        "relay double_it p\n"
        "  out 0 - drain.0\n"
        "drain swallow p\n");

    map_t *m = map_load_file(map_path, 0);
    pool_submitter_register(m->pool);
    pool_release(m->pool);
    double before = now_seconds();
    for (int i = 0; i < VALUES; i++)
        map_deliver_value(m, 1, 0, &i);
    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    double elapsed = now_seconds() - before;

    /* One bare number on standard output. The script reads it and does
     * the telling, because only the script knows there are two of
     * these and what the pair means. */
    printf("%.0f\n", VALUES * 2 / elapsed);

    map_destroy(m);
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <project-root> [--overhead]\n", argv[0]);
        return 1;
    }

    const char *root = argv[1];
    char work_dir[512];
    snprintf(work_dir, sizeof work_dir, "/dev/shm/%s/phase-7-work",
             strrchr(root, '/') ? strrchr(root, '/') + 1 : root);
    char command[2048];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0)
        return 1;

    if (argc > 2 && strcmp(argv[2], "--overhead") == 0) {
        scene_overhead(work_dir);
        return 0;
    }

    demo_join(root, "phase-7-inside-report.txt");

    char map_path[600];
    snprintf(map_path, sizeof map_path, "%s/inside.map", work_dir);
    write_text(map_path, MAP_TEXT);

    map_t *m = map_load_file(map_path, 0);
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    /* --- scene 1: the live map, drawn from the table ------------- */
    int first_wave = scene_pick(500, 700);
    int second_wave = first_wave;

    scene_open(1, "reading a map that is running");

    scene_problem(
        "Every station keeps counters as it works: how many times its "
        "box has run, how many values it has produced, how deep each of "
        "its input slots is right now. Those live in the station table "
        "in memory, which is the same structure delivery walks — so "
        "reading them is reading the machine rather than reading a "
        "description of it. This scene draws the whole map from that "
        "table while values are flowing through it, and never once "
        "consults the file the map was loaded from.");

    scene_imagine(
        "a plant with a control room whose gauges are wired to the "
        "machines rather than to the drawings. A wall showing what the "
        "plant was designed to do would be decoration; a wall showing "
        "what it is doing right now is the reason somebody is sitting "
        "there.");

    scene_stands_for("a station's counters", "a gauge on the wall",
                     "each reports one specific quantity continuously "
                     "and without being asked, so what it shows is a "
                     "fact about now rather than a summary of a period");
    scene_stands_for("runs and slot depth", "the needle's position",
                     "both move only because the thing being measured "
                     "moved, which is what makes a still reading "
                     "meaningful rather than merely decorative");
    scene_stands_for("the station table", "the sensors on the machines",
                     "both are attached to the working parts themselves, "
                     "so what they report cannot disagree with what is "
                     "happening — there is nothing in between to drift");
    scene_stands_for("the map file on disk", "the drawings in the office",
                     "both describe what was intended and neither is "
                     "updated by anything that happens on the floor, "
                     "which is why neither can be trusted about now");

    double wave1_start = now_seconds();
    for (int i = 0; i < first_wave; i++) {
        map_deliver_value(m, S_PUMP, 0, &i);
        if (i == first_wave / 2) {
            scene_blank();
            live_view(m, "mid-flood: both iterator ports feed one cruncher");
        }
    }
    /* Wait for the wave to drain so the timing is honest. */
    while (m->stations[S_DRAIN].runs < first_wave)
        usleep(5000);
    double wave1 = now_seconds() - wave1_start;

    scene_blank();
    live_view(m, "first wave drained");

    scene_finding(scene_text(
        "Two readings of the same plant, taken while it worked, and "
        "neither of them consulted the map file. %d values went in and "
        "the gauges account for every one of them. Notice that crunch "
        "carries the traffic and spare has never run: the wall is "
        "already telling somebody something, which is scene two.",
        first_wave));

    /* --- scene 2: the bottleneck, found and relieved ------------- */
    scene_open(2, "finding the hot station, then moving a wire under load");

    scene_problem(
        "This map's iterator has both of its ports wired to the same "
        "slow station, while an identical spare sits unused. That is a "
        "deliberate defect, and the point is that nobody has to know it "
        "in advance: the contention report ranks stations by time spent "
        "waiting on mutexes and time spent inside boxes, and the "
        "offender is simply at the top. The repair is a disconnect and "
        "a connect on a running map — no restart, no drain, no reload. "
        "What happens to throughput afterwards is the part worth "
        "watching, because it is not what most people expect.");

    scene_imagine(
        "two lanes running to the same bridge, with the second coned "
        "off so everything funnels into the first. The traffic reports "
        "do not guess at this — they say which lane is carrying the "
        "cars. Moving a cone is a small thing and can be done with "
        "traffic still flowing.");

    scene_stands_for("a station on an iterator port", "a lane to the bridge",
                     "each is one route the work can take, capable of "
                     "carrying its share, and idle purely because "
                     "nothing is currently being sent down it");
    scene_stands_for("which port is wired where", "where the cones are placed",
                     "both are the whole of the routing decision, both "
                     "are external to the machinery they direct, and "
                     "both can be changed without touching it");
    scene_stands_for("the contention report", "the traffic report",
                     "each is produced by measuring the actual flow "
                     "rather than by reasoning about the layout, so it "
                     "names the real bottleneck and not the suspected "
                     "one");
    scene_stands_for("a rewire under load", "moving a cone in live traffic",
                     "both take effect immediately for everything that "
                     "arrives afterwards, and neither disturbs what is "
                     "already in motion");

    scene_blank();
    scene_line("the engine's own reading of where the pain is:");
    scene_blank();
    map_report_stations(m, scene_screen_stream(), REPORT_BY_CONTENTION);
    map_report_stations(m, scene_report_stream(), REPORT_BY_CONTENTION);
    scene_blank();

    int rc1 = map_rewire_disconnect(m, S_PUMP, 1, S_CRUNCH, 0);
    int rc2 = map_rewire_connect(m, S_PUMP, 1, S_SPARE, 0);

    long crunch_before = m->stations[S_CRUNCH].runs;
    double wave2_start = now_seconds();
    for (int i = 0; i < second_wave; i++)
        map_deliver_value(m, S_PUMP, 0, &i);
    while (m->stations[S_DRAIN].runs < first_wave + second_wave)
        usleep(5000);
    double wave2 = now_seconds() - wave2_start;

    scene_measured("the cone lifted",
                   rc1 == 0 ? "second port freed" : "REFUSED",
                   "disconnect, mid-run");
    scene_measured("the cone set down",
                   rc2 == 0 ? "second lane opened" : "REFUSED",
                   "connect, mid-run");
    scene_measured("first wave, one lane",
                   scene_text("%.0f cars/second", first_wave / wave1),
                   scene_text("%d values", first_wave));
    scene_measured("second wave, two",
                   scene_text("%.0f cars/second", second_wave / wave2),
                   scene_text("%d values", second_wave));
    scene_measured("crunch took",
                   scene_text("%ld more", (long)m->stations[S_CRUNCH].runs
                              - crunch_before),
                   "after the change");
    scene_measured("spare took",
                   scene_text("%ld", (long)m->stations[S_SPARE].runs),
                   "having run nothing before");

    scene_finding(
        "The load moved, on camera, with nothing restarted and nothing "
        "lost. And the queue barely shortened — which is the finding "
        "worth keeping. The pool was already running the slow station's "
        "invocations on every core, so a slow box is not by itself a "
        "bottleneck here. The reports exist to show who pays, not to "
        "promise that moving a cone will make anything faster; a person "
        "who assumed otherwise has just been corrected by their own "
        "instruments, which is what instruments are for.");

    /* --- scene 3: the dump tells the new truth ------------------- */
    char dump1_path[600], dump2_path[608];
    snprintf(dump1_path, sizeof dump1_path, "%s/dump1.map", work_dir);
    snprintf(dump2_path, sizeof dump2_path, "%s/dump2.map", work_dir);
    FILE *d1 = fopen(dump1_path, "w");
    map_dump(m, d1);
    fclose(d1);

    scene_open(3, "a description that is generated, not stored");

    scene_problem(
        "The map was edited a moment ago while running, so the file it "
        "was loaded from is now a description of a program that no "
        "longer exists. The dump does not have this problem, because it "
        "is not a copy of anything: it walks the station table and "
        "writes out what is actually wired to what, in the same syntax "
        "the loader reads. Any system that can be changed at runtime "
        "needs this, or it becomes undescribable the first time "
        "somebody changes it.");

    scene_imagine(
        "a building with two sets of drawings — the plan the architect "
        "submitted, and the as-built set made afterwards, recording "
        "where the pipes actually went once the site had its say. Both "
        "are kept, and only one is safe to renovate from.");

    scene_stands_for("the map file on disk", "the architect's plan",
                     "both record an intention at a moment in the past "
                     "and neither is updated by anything that happens "
                     "afterwards, so both go quietly out of date without "
                     "ever becoming visibly wrong");
    scene_stands_for("the dump", "the as-built drawings",
                     "each is produced by surveying the thing itself "
                     "rather than by copying an earlier document, which "
                     "is why each is correct by construction");
    scene_stands_for("the runtime rewiring", "what the site changed",
                     "both are legitimate decisions made after the plan "
                     "was drawn, and both are exactly the reason the "
                     "plan alone can no longer be relied on");
    scene_stands_for("anything trusting the description",
                     "the next renovation",
                     "both act on what the document says, so both "
                     "inherit its errors — which is what makes the "
                     "choice of document a safety question");

    FILE *check = fopen(dump1_path, "r");
    char line[256];
    while (fgets(line, sizeof line, check)) {
        char *newline = strchr(line, '\n');
        char *text = line;
        if (newline)
            *newline = '\0';
        /* Dump lines are indented to show which station they belong
         * to. That indent is the file's layout, not part of the wire,
         * and carrying it into a column would push the column. */
        while (*text == ' ' || *text == '\t')
            text++;
        if (strstr(text, "out 1 - spare.0"))
            scene_measured("the as-built says", text, "taken just now");
    }
    fclose(check);
    scene_measured("the plan still says", "out 1 - crunch.0",
                   "unchanged on disk");

    scene_finding(
        "The file is not wrong about what was asked for and it is no "
        "longer a description of the program. The dump is, because it "
        "was written by walking the station table rather than by "
        "copying anything. That is the only arrangement under which a "
        "running system can be edited and still be describable.");

    /* --- scene 4: the round trip, quiet and total ---------------- */
    map_t *m2 = map_load_file(dump1_path, 2);
    FILE *d2 = fopen(dump2_path, "w");
    map_dump(m2, d2);
    fclose(d2);
    pool_release(m2->pool);
    pool_join(m2->pool);
    map_destroy(m2);
    snprintf(command, sizeof command, "cmp -s %s %s", dump1_path, dump2_path);
    int identical = (system(command) == 0);

    scene_open(4, "proving the dump lost nothing");

    scene_problem(
        "A dump is only worth having if it is complete, and "
        "completeness is hard to argue directly — you would have to "
        "enumerate everything a map can contain and check each one. The "
        "round trip settles it in a single comparison instead: load the "
        "dump into a fresh map, dump that, and compare the two files "
        "byte for byte. Anything the first dump failed to record would "
        "be missing from the second map and therefore from the second "
        "dump, so identical files mean nothing was dropped.");

    scene_imagine(
        "testing a translation without speaking either language by "
        "handing it to a second translator and asking for it back. If "
        "what returns is the original word for word, nothing was lost "
        "in either direction — not a tense, not a plural, not the one "
        "word that had no equivalent.");

    scene_stands_for("the live map in memory", "the original text",
                     "each is the thing whose meaning is at stake, and "
                     "each is the standard everything else is judged "
                     "against rather than being judged itself");
    scene_stands_for("the dump", "the translation",
                     "each is a complete restatement in a different "
                     "medium, and each is exactly as good as the amount "
                     "it managed to carry across");
    scene_stands_for("loading the dump", "translating it back",
                     "both reconstruct from the restatement alone, with "
                     "no access to the original, which is what makes the "
                     "comparison afterwards meaningful");
    scene_stands_for("byte-identical files", "word for word",
                     "both are a single check that stands in for "
                     "hundreds, because anything at all that had been "
                     "lost would have to show up as a difference");

    scene_measured("dumped, loaded, dumped",
                   identical ? "byte-identical" : "DIFFERENT",
                   identical ? "nothing lost either way"
                             : "a loader bug has a witness");

    if (!identical)
        exit(1);

    scene_finding(
        "Everything the map knows survives a trip through text and back "
        "into memory: every station, every wire, every static, every "
        "gatherer, in an order stable enough to compare with cmp. The "
        "picture and the engine agree, which is what makes the picture "
        "worth printing at all.");

    /* --- scene 5: a refused rewire cannot kill the plant --------- */
    scene_open(5, "a refusal that does not take the map down with it");

    scene_problem(
        "Runtime rewiring runs the same cycle check that phase 4 runs "
        "at load time, and here it has to behave differently on "
        "failure. A bad map at load time can reasonably stop the "
        "program: nothing is running yet. A bad edit to a live map "
        "cannot, because the thing asking may be a person experimenting "
        "on a system other people are using. So the check returns a "
        "refusal, names both stations, changes nothing, and leaves "
        "every value already in flight untouched.");

    scene_imagine(
        "a machine with a lever that cannot be moved into one "
        "particular position, because that position would drive two "
        "gears into each other. Pushing it breaks nothing, stops "
        "nothing, and summons nobody: the lever simply does not go "
        "there, and a light says why.");

    scene_stands_for("a rewire request", "pushing the lever",
                     "each is an instruction that may or may not be "
                     "carried out, and issuing one is not the same as "
                     "having the right to have it obeyed");
    scene_stands_for("a gather cycle", "the forbidden position",
                     "each would destroy the machine rather than "
                     "misconfigure it, which is why neither can be "
                     "allowed to be reached and then regretted");
    scene_stands_for("the cycle walk", "the interlock",
                     "both refuse in advance by inspecting the whole "
                     "arrangement, rather than detecting the damage "
                     "afterwards when there is nothing left to detect "
                     "it with");
    scene_stands_for("a refusal that returns", "the light saying why",
                     "both inform without punishing: the operator learns "
                     "what was wrong and everything else carries on, "
                     "which is the only version of this that is safe to "
                     "leave switched on");

    scene_blank();
    scene_line("attempt: make one gatherer pull from its own consumer");
    int refused = map_rewire_gather(m, S_GA, 0, S_GB);
    scene_blank();

    scene_measured("the engine answered",
                   refused == 0 ? "YES?!" : "no",
                   "reason printed above, both named");

    long drain_before = m->stations[S_DRAIN].runs;
    int still = 0;
    map_deliver_value(m, S_PUMP, 0, &still);
    while (m->stations[S_DRAIN].runs == drain_before)
        usleep(5000);

    scene_measured("the next delivery",
                   "flowed through", "the plant never stopped");

    scene_finding(
        "A refusal returns; it does not kill. That is the difference "
        "between a check that protects a program being loaded and a "
        "check that protects a program already running — the second one "
        "has to survive being wrong, because the thing asking may be a "
        "person experimenting on a system other people are using.");

    scene_blank();
    scene_line("and the buffer report, one last time:");
    scene_blank();
    map_report_buffers(m, scene_screen_stream());
    map_report_buffers(m, scene_report_stream());

    pool_submitter_unregister(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    /* Not demo_close(): scene six belongs to the driving script, which
     * closes the report after it. */
    return 0;
}
