/*
 * 093-test-stopping.c — every way a program ends except the happy one
 * (issue 106).
 *
 * What this is: the proof for the three signals a program answers and
 * for the policy that an invalid operation ends it. Running out of
 * work is proven elsewhere; this is the rest.
 *
 * How it does it, in general terms: **each scene runs in a child
 * process**, because what is being tested is how a process dies, and
 * a test that could observe that from inside would be testing
 * something else. The parent starts a child, waits until the child
 * says it is ready, sends it a signal, and then reads two things: the
 * manner of the child's death, and the report the child left behind
 * in the RAM-backed scratch tier.
 *
 * The handshake is a pipe rather than a sleep. A sleep long enough to
 * be reliable on a loaded machine is long enough to make the suite
 * unpleasant, and one short enough to be pleasant is a race that
 * fails on somebody else's laptop and nowhere else.
 */
#include "018-station.h"
#include "026-registry.h"
#include "049-observe.h"
#include "091-stopping.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

/*
 * A ceiling on every scene. What is being tested is how a process
 * dies, and the failure mode of getting it wrong is a process that
 * does not — which without this would hang the whole suite instead of
 * failing one test. Generous, because it is a backstop and not a
 * measurement.
 */
#define SCENE_SECONDS 20

static int failures = 0;

/* {{{ static void check() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "  FAIL: %s\n", what);
        failures++;
    }
}
/* }}} */

static char work_dir[256];

/* {{{ static char *slurp() */
static char *slurp(const char *path)
{
    static char buf[8192];
    buf[0] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return buf;
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    buf[n > 0 ? n : 0] = '\0';
    return buf;
}
/* }}} */

/* {{{ static map_t *a_busy_program() */
/*
 * Two stations in a loop, fed from outside: a doubler whose result
 * comes back round through a station that keeps it. It never runs out
 * of work on its own, which is what a scene about being *told* to
 * stop needs — otherwise the program might end by itself first and
 * prove nothing.
 *
 * The loop is bounded only by the values put in, so the program winds
 * down when the entrance is shut and the values in flight are used
 * up.
 */
static map_t *a_busy_program(void)
{
    map_t *m = map_create_empty();

    int gate = map_add_station(m);
    map_place_box(m, gate, "keep", STATION_PLAIN);
    map_name_station(m, gate, "gate");
    map_designate_input(m, gate);

    int work = map_add_station(m);
    map_place_box(m, work, "slow_double", STATION_PLAIN);
    map_name_station(m, work, "work");

    int out = map_add_station(m);
    map_place_box(m, out, "keep", STATION_PLAIN);
    map_name_station(m, out, "out");
    map_designate_output(m, out);

    map_wire(m, gate, 0, work, 0);
    map_wire(m, work, 0, out, 0);
    return m;
}
/* }}} */

/* {{{ the feeder */
/*
 * **Something outside the program, holding a standing promise that
 * more work may arrive**, which is the shape every real caller has: a
 * control socket, a shell runner, a workbench. It keeps delivering
 * until the entrance refuses it, and then drops the promise.
 *
 * The feeder is what makes the polite path *finish*. A supervisor
 * asking a program to wind down shuts the entrance, but a promise
 * still held means the pool idles rather than terminating — correctly,
 * because a promise says more may still come. So somebody has to
 * notice the door shut and let go, and the somebody is whoever made
 * the promise. Delivering and being refused is how they find out;
 * there is no separate notification and none is wanted.
 */
typedef struct {
    map_t *m;
    int    cap;
    int    pace_us;   /* 0 floods; anything else keeps it going a while */
    int    started;   /* raised once the first values are in flight */
} feeder_t;

static void *feed(void *arg)
{
    feeder_t *f = arg;
    for (int i = 0; i < f->cap; i++) {
        int v = i;
        if (map_deliver_argument(f->m, 0, 0, &v, sizeof v) != NULL)
            break;    /* the door shut; the promise is no longer honest */
        if (i == 4)
            atomic_store((_Atomic int *)&f->started, 1);
        if (f->pace_us)
            usleep((unsigned)f->pace_us);
    }
    atomic_store((_Atomic int *)&f->started, 1);
    pool_submitter_unregister(f->m->pool);
    return NULL;
}
/* }}} */

/* {{{ static pid_t start_child() */
/*
 * Fork a child that prepares for stopping, builds a program, sets a
 * feeder going, says "ready" down a pipe, and then waits for a
 * signal. The parent gets the child's pid and the read end of the
 * pipe.
 */
static pid_t start_child(const char *report_path, int n_values,
                         int pace_us, int workers, int *ready_fd)
{
    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);

    pid_t pid = fork();
    if (pid != 0) {
        close(pipefd[1]);
        *ready_fd = pipefd[0];
        return pid;
    }

    /* --- the child --- */
    close(pipefd[0]);

    /* Before any thread exists, which is what makes "blocked in every
     * thread" true without visiting any of them — a thread inherits
     * the mask of whoever created it. */
    sora_prepare(report_path);

    map_t *m = a_busy_program();
    map_start(m, workers);
    map_bring_up(m);

    /*
     * **The promise is made before the gate opens, not after.**
     *
     * This program seeds nothing — every station waits for a value
     * that arrives through the entrance — so between releasing the
     * workers and the feeder's first delivery the queue is empty and
     * nobody is promising anything. The last sleeper rule fires in
     * that window and the program is over before it began.
     *
     * That is the rule working exactly as issue 104 wrote it, and the
     * fix is the clause 104 provided for it: register the standing
     * promise first. Found by three scenes that passed for the wrong
     * reason — a program that had already finished exits zero, which
     * is what the polite path was asserting.
     */
    pool_submitter_register(m->pool);
    pool_release(m->pool);

    static feeder_t f;
    f.m = m;
    f.cap = n_values;
    f.pace_us = pace_us;
    f.started = 0;
    pthread_t feeder;
    pthread_create(&feeder, NULL, feed, &f);

    /* Ready means values are actually moving, not merely that the
     * program exists. A signal arriving before anything has run would
     * be testing an idle program. */
    while (!atomic_load((_Atomic int *)&f.started))
        usleep(200);

    ssize_t wrote = write(pipefd[1], "r", 1);
    (void)wrote;

    _exit(sora_wait(m));
}
/* }}} */

/* {{{ static void wait_for_ready() */
static void wait_for_ready(int fd)
{
    char c = 0;
    ssize_t n = read(fd, &c, 1);
    (void)n;
    close(fd);
}
/* }}} */

/* {{{ static int reap() */
/*
 * Collect a child, giving up after the ceiling rather than waiting
 * forever. A test that hangs reports nothing; a test that fails
 * reports which promise was not kept.
 */
static int reap(pid_t pid, const char *what)
{
    for (int tenths = 0; tenths < SCENE_SECONDS * 10; tenths++) {
        int status = 0;
        pid_t got = waitpid(pid, &status, WNOHANG);
        if (got == pid)
            return status;
        usleep(100000);
    }
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    fprintf(stderr, "  FAIL: %s — the child never stopped, so it was "
                    "killed after %d seconds\n", what, SCENE_SECONDS);
    failures++;
    return status;
}
/* }}} */

/* {{{ static void the_polite_shutdown_writes_nothing() */
/*
 * **The ordinary ending, triggered early.** A supervisor asks the
 * program to wind down; the entrance shuts, the values already in
 * flight finish travelling, the queue empties, the workers park, and
 * the last-sleeper rule fires exactly as it would have.
 *
 * Exit zero, and **no report**, because nobody asked for one — a
 * supervisor stopping a healthy program does not want a file it did
 * not request. The absence is the assertion.
 */
static void the_polite_shutdown_writes_nothing(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/polite.txt", work_dir);
    unlink(report);

    int ready = -1;
    /*
     * **Paced, and still feeding when the signal arrives**, which is
     * what makes this the polite path rather than a program that
     * happened to finish. An unpaced feeder empties its whole batch
     * in less time than it takes the parent to raise a signal, and
     * then the scene proves only that a finished program exits zero —
     * which is issue 104's claim, already proven elsewhere.
     */
    pid_t pid = start_child(report, 1000000, 300, 3, &ready);
    wait_for_ready(ready);

    kill(pid, SIGTERM);

    int status = reap(pid, "a polite shutdown");

    check(WIFEXITED(status) && WEXITSTATUS(status) == SORA_EXIT_FINISHED,
          "a politely stopped program exits zero, the same as one that "
          "ran out of work");

    char *said = slurp(report);
    check(said[0] == '\0',
          "and writes no diagnostics, because nobody asked for any");

    printf("  a polite shutdown ended it the ordinary way and said "
           "nothing\n");
}
/* }}} */

/* {{{ static void the_polite_shutdown_shuts_the_door() */
/*
 * The one mechanism the polite path does add, and the reason it works
 * at all: the entrance stops accepting. Without it a caller could
 * keep feeding a program that has been asked to wind down, and the
 * queue would never empty.
 */
static void the_polite_shutdown_shuts_the_door(void)
{
    map_t *m = a_busy_program();
    map_start(m, 2);
    map_bring_up(m);

    int v = 1;
    check(map_deliver_argument(m, 0, 0, &v, sizeof v) == NULL,
          "an open program takes an argument");

    atomic_store(&m->closing, 1);
    const char *no = map_deliver_argument(m, 0, 0, &v, sizeof v);
    check(no != NULL && strstr(no, "winding down") != NULL,
          "and a closing one refuses, saying why");

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    printf("  the entrance shuts when a program is asked to wind down\n");
}
/* }}} */

/* {{{ static void the_interrupt_gathers_everything() */
/*
 * **Somebody is standing there and wants to know what happened.** The
 * queue stops handing out work, and the report is written from the
 * waiting thread itself — not enqueued as a task somebody has to hope
 * gets scheduled, because the whole point is that it works on a
 * program whose workers are all busy.
 *
 * A deliberate backlog: many values, few workers, and a box that
 * takes its time. So there is something in the queue to report on.
 */
static void the_interrupt_gathers_everything(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/interrupt.txt", work_dir);
    unlink(report);

    int ready = -1;
    /*
     * **A deliberate backlog that lasts.** The feeder is paced faster
     * than two workers can get through a box that takes its time, so
     * values pile up and keep piling up — which is both what gives
     * the report something to say and what keeps the program alive
     * until the signal arrives. An unpaced feeder empties its batch
     * and the program finishes on its own, proving nothing about
     * interruption.
     */
    pid_t pid = start_child(report, 1000000, 50, 2, &ready);
    wait_for_ready(ready);

    kill(pid, SIGINT);

    int status = reap(pid, "an interrupt");

    check(WIFEXITED(status) && WEXITSTATUS(status) == SORA_EXIT_INTERRUPTED,
          "an interrupted program exits 130, which every shell already "
          "understands");

    char *said = slurp(report);
    check(strstr(said, "interrupted") != NULL,
          "the report says what happened");
    check(strstr(said, "-- stations --") != NULL
          && strstr(said, "work") != NULL,
          "and names the stations by the names their author gave them");
    check(strstr(said, "-- workers --") != NULL,
          "and says where each worker was");
    check(strstr(said, "-- the program as it stands --") != NULL
          && strstr(said, "slow_double") != NULL,
          "and writes the program out as a map file, which is the shape "
          "it had at the end rather than the shape on disk");

    printf("  an interrupt stopped it, exited 130, and left a full "
           "report\n");
}
/* }}} */

/* {{{ static void the_quit_takes_no_locks() */
/*
 * **Maximum evidence, no cooperation.** This is the path that works
 * when the other two cannot: a program whose every worker is wedged
 * has no thread free to run a diagnostics task and no queue that will
 * ever drain, so it can only be examined from outside.
 *
 * What is asserted is that the report appeared and that the process
 * left a body. Stations are named by index rather than by name here,
 * on purpose — the names are an array somebody may be growing at this
 * exact moment, and reading it would be the crash this report exists
 * to explain.
 */
static void the_quit_takes_no_locks(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/quit.txt", work_dir);
    unlink(report);

    int ready = -1;
    pid_t pid = start_child(report, 1000000, 50, 2, &ready);
    wait_for_ready(ready);

    kill(pid, SIGQUIT);

    int status = reap(pid, "a quit");

    check(WIFSIGNALED(status),
          "the quit path leaves a body rather than exiting tidily");

    char *said = slurp(report);
    check(strstr(said, "taking no locks") != NULL,
          "the report says what it did not do");
    check(strstr(said, "station 1:") != NULL,
          "and names stations by index, never by a name it would have "
          "had to read an array to find");
    check(strstr(said, "worker 0:") != NULL,
          "and says where each worker was, from a number it can read "
          "without touching anything");
    check(strstr(said, "core dump") != NULL,
          "and points at the core, which is where the wedged box's "
          "stack is");

    printf("  a quit wrote what needs no lock and left a core\n");
}
/* }}} */

/* {{{ static void an_ignored_refusal_cannot_half_build() */
/*
 * **The property the fatal policy buys**, and the reason for it.
 *
 * Rewiring used to hand back a code a caller could ignore, chosen
 * deliberately: a running engine that dies because a control surface
 * sent one bad instruction takes the plant down with it. The cost was
 * written down at the time as a debt — *a caller can ignore a return
 * value* — and an ignored refusal leaves a program running that
 * somebody believes they just edited successfully.
 *
 * The child below is that caller. It asks for a wire that cannot
 * exist, ignores the answer completely, and carries on as though the
 * edit had happened. It must not get that far.
 */
static void an_ignored_refusal_cannot_half_build(void)
{
    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        freopen("/dev/null", "w", stderr);
        sora_prepare("/dev/null");

        map_t *m = map_create_empty();
        int a = map_add_station(m);
        map_place_box(m, a, "seven", STATION_PLAIN);   /* -> int, 4 bytes */
        int b = map_add_station(m);
        map_place_box(m, b, "mix", STATION_PLAIN);     /* port 1: double  */

        /* Four bytes into an eight-byte port, through the face that
         * stops rather than the one that hands the reason back. */
        map_connect(m, a, 0, b, 1);

        /* Never reached. If it is, the program was left half built by
         * an instruction it refused, which is the whole thing being
         * prevented. */
        ssize_t wrote = write(pipefd[1], "survived", 8);
        (void)wrote;
        _exit(0);
    }

    close(pipefd[1]);
    char words[16] = { 0 };
    ssize_t got = read(pipefd[0], words, sizeof words - 1);
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    check(got <= 0, "the program did not carry on past a refused edit");
    check(WIFEXITED(status) && WEXITSTATUS(status) == SORA_EXIT_BAD_CALL,
          "and ended with the code meaning the calling code was wrong, "
          "which a shell can tell from a malformed file");

    printf("  an ignored refusal ended the program instead of half "
           "building it\n");
}
/* }}} */

/* {{{ static void a_wedged_program_still_reports() */
/*
 * **The situation every one of these paths was designed for.**
 *
 * Every worker is inside a box that will never return. There is no
 * thread free to run a diagnostics task, and the queue will never
 * drain, so a design that gathered by enqueueing work would go silent
 * exactly when the observation mattered. Gathering on the thread that
 * received the signal is what makes this scene possible at all.
 *
 * The program cannot be *stopped* — there is no safe way to interrupt
 * executing C, so "halt everything" honestly means stop starting new
 * things. What it can do is explain itself on the way past, which is
 * what is asserted.
 */
static void a_wedged_program_still_reports(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/wedged.txt", work_dir);
    unlink(report);

    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        sora_prepare(report);

        map_t *m = map_create_empty();
        int gate = map_add_station(m);
        map_place_box(m, gate, "keep", STATION_PLAIN);
        map_name_station(m, gate, "gate");
        map_designate_input(m, gate);
        int stuck = map_add_station(m);
        map_place_box(m, stuck, "wedge", STATION_PLAIN);
        map_name_station(m, stuck, "stuck");
        map_designate_output(m, stuck);
        map_wire(m, gate, 0, stuck, 0);

        map_start(m, 2);
        map_bring_up(m);
        pool_submitter_register(m->pool);
        pool_release(m->pool);

        /* Two workers, four values: both end up inside the wedge and
         * the rest sit in the queue forever. */
        for (int i = 0; i < 4; i++) {
            int v = i;
            map_deliver_argument(m, gate, 0, &v, sizeof v);
        }
        /* Give both workers time to get inside. */
        usleep(200000);
        ssize_t wrote = write(pipefd[1], "r", 1);
        (void)wrote;
        _exit(sora_wait(m));
    }

    close(pipefd[1]);
    wait_for_ready(pipefd[0]);
    kill(pid, SIGINT);
    int status = reap(pid, "an interrupt on a wedged program");

    check(WIFEXITED(status) && WEXITSTATUS(status) == SORA_EXIT_INTERRUPTED,
          "a program whose every worker is wedged still stops when a "
          "person asks it to");

    char *said = slurp(report);
    check(strstr(said, "-- workers --") != NULL
          && strstr(said, "inside stuck") != NULL,
          "and the report names the station the workers are stuck in, "
          "which is the whole reason somebody pressed the key");

    printf("  a program with every worker wedged still reported, and "
           "named where they were stuck\n");
}
/* }}} */

/* {{{ static void a_held_lock_does_not_trap_the_person() */
/*
 * **The escape hatch, against the thing it exists for.**
 *
 * A station's mutex is held by somebody who will never release it.
 * The first interrupt starts gathering the full report, which asks
 * that station how deep its buffers are, which takes that mutex —
 * so the gather never finishes.
 *
 * A person who pressed ctrl+C and got nothing presses it again, and
 * the second one has to work. It does, because for the length of the
 * gather that one signal is unblocked with a handler that does
 * nothing but leave.
 *
 * Without that, the escape would be nominal: the second interrupt
 * would sit pending behind a gather that never returns, and the hatch
 * would open only in the cases where nobody needed it.
 */
static void a_held_lock_does_not_trap_the_person(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/trapped.txt", work_dir);
    unlink(report);

    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        sora_prepare(report);

        map_t *m = a_busy_program();
        map_start(m, 2);
        map_bring_up(m);
        pool_submitter_register(m->pool);
        pool_release(m->pool);

        /* Held and never released, which is the condition being
         * survived rather than a mistake. */
        pthread_mutex_lock(&map_station(m, 1)->mutex);

        ssize_t wrote = write(pipefd[1], "r", 1);
        (void)wrote;
        _exit(sora_wait(m));
    }

    close(pipefd[1]);
    wait_for_ready(pipefd[0]);

    kill(pid, SIGINT);
    usleep(300000);          /* long enough for the gather to be stuck */
    kill(pid, SIGINT);

    int status = reap(pid, "a second interrupt through a held lock");
    check(WIFEXITED(status) && WEXITSTATUS(status) == SORA_EXIT_INTERRUPTED,
          "a second interrupt got out of a report that could not finish");

    printf("  a second interrupt escaped a gather stuck on a lock nobody "
           "would release\n");
}
/* }}} */

/* {{{ static void the_quit_ignores_a_held_lock() */
/*
 * The same held lock, and the path that was built to survive it. The
 * quit report takes no locks at all, so it says what it can and
 * leaves a body, where the full report would have hung.
 *
 * This is the pair worth reading together: the previous scene is what
 * happens when somebody asks for everything, and this is what happens
 * when they ask for whatever can be had without cooperation.
 */
static void the_quit_ignores_a_held_lock(void)
{
    char report[512];
    snprintf(report, sizeof report, "%s/quit-locked.txt", work_dir);
    unlink(report);

    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        sora_prepare(report);

        map_t *m = a_busy_program();
        map_start(m, 2);
        map_bring_up(m);
        pool_submitter_register(m->pool);
        pool_release(m->pool);
        pthread_mutex_lock(&map_station(m, 1)->mutex);

        ssize_t wrote = write(pipefd[1], "r", 1);
        (void)wrote;
        _exit(sora_wait(m));
    }

    close(pipefd[1]);
    wait_for_ready(pipefd[0]);
    kill(pid, SIGQUIT);
    int status = reap(pid, "a quit through a held lock");

    check(WIFSIGNALED(status), "the quit path still left a body");
    char *said = slurp(report);
    check(strstr(said, "taking no locks") != NULL
          && strstr(said, "worker 0:") != NULL,
          "and still wrote its report, through a lock that would have "
          "stopped the full one");

    printf("  a quit reported through a held lock, which is the case it "
           "exists for\n");
}
/* }}} */

/* {{{ main */
int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/stopping-test-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot make %s\n", work_dir);
        return 1;
    }

    the_polite_shutdown_shuts_the_door();
    the_polite_shutdown_writes_nothing();
    the_interrupt_gathers_everything();
    the_quit_takes_no_locks();
    a_wedged_program_still_reports();
    a_held_lock_does_not_trap_the_person();
    the_quit_ignores_a_held_lock();
    an_ignored_refusal_cannot_half_build();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        fprintf(stderr, "could not clean up %s\n", work_dir);

    if (failures) {
        fprintf(stderr, "%d stopping checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
