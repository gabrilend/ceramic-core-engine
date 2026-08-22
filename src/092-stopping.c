/*
 * 092-stopping.c — every way a program ends except the happy one.
 *
 * What this is: issue 106, from inside. Interface and reasoning in
 * 091-stopping.h.
 *
 * How it does it, in general terms: **nothing here is a signal
 * handler.** The three signals are blocked in every thread and the
 * thread that started the program calls sigwait, which hands back a
 * signal number as an ordinary value to a thread running ordinary
 * code. Every restriction on what a handler may call stops applying,
 * which is why the reports below are free to take locks and format
 * text — except the one that deliberately does neither.
 *
 * The gathering happens on the waiting thread itself rather than as a
 * task somebody hopes gets scheduled. That is what lets it work on a
 * program whose every worker is wedged: the queue guarantees nothing
 * to anybody, and the one piece of work that must happen cannot be
 * the one piece of work standing in line.
 */
#include "091-stopping.h"
#include "049-observe.h"
#include "073-latebox.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * The signal the pool raises when the work runs out.
 *
 * A real-time signal rather than one of the two conventional
 * user-defined ones, because those two are exactly what a box someone
 * brings to this engine is most likely to be using already. Nothing
 * in this project sends SIGRTMIN for any other reason, and a program
 * that does can say so — the number is read from a variable, not
 * baked into the comparisons below.
 */
static int finished_signal;

/* Where a report goes. Opened during preparation and never after,
 * because a dying program cannot answer for a failed open. */
static int  report_fd = -1;
static char report_where[512];

/* How many interrupts have arrived. The second one is an escape
 * hatch and takes no other path with it. */
static int interrupts;

/* {{{ static void escape_now() */
/*
 * The only signal handler in this file, installed for the length of
 * one gather and doing the one thing a handler is unarguably allowed
 * to do. See the second-interrupt case in sora_wait for why it has to
 * exist at all.
 */
static void escape_now(int sig)
{
    (void)sig;
    _exit(SORA_EXIT_INTERRUPTED);
}
/* }}} */

/* {{{ static void say() */
/*
 * One line into the report. Formatted into a stack buffer and written
 * with one call, because **write is the only file operation available
 * on every path in this file** — including the one forbidden to take
 * a lock, and a buffered stream takes one.
 */
static void say(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void say(const char *fmt, ...)
{
    if (report_fd < 0)
        return;
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof line - 1)
        n = (int)sizeof line - 1;
    ssize_t wrote = write(report_fd, line, (size_t)n);
    (void)wrote;   /* a dying program cannot do anything about a short write */
}
/* }}} */

/* {{{ sora_prepare() */
void sora_prepare(const char *report_path)
{
    finished_signal = SIGRTMIN;

    /*
     * Blocked in this thread, and therefore in every thread made
     * after it, because a thread inherits the mask of whoever created
     * it. That is what makes "every thread" true without visiting any
     * of them — and it is why this must be called before the pool
     * exists rather than after.
     */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, finished_signal);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        fprintf(stderr, "stopping: could not block the signals this "
                        "program answers\n");
        exit(SORA_EXIT_NO_RESOURCE);
    }

    if (report_path && *report_path) {
        snprintf(report_where, sizeof report_where, "%s", report_path);
    } else {
        /*
         * The project's RAM-backed scratch tier, which a reboot
         * empties. That is right for something read while debugging
         * and useless as a post-mortem after the machine came back;
         * the core dump is what covers the second case.
         */
        mkdir(SORA_RAM_SHARED, 0777);
        snprintf(report_where, sizeof report_where,
                 "%s/stopping-%d.txt", SORA_RAM_SHARED, (int)getpid());
    }

    report_fd = open(report_where, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (report_fd < 0) {
        /*
         * Loud, and not fatal. A program that cannot open its report
         * can still run and can still stop; what it cannot do is
         * explain itself afterwards, and somebody should be told that
         * now rather than discovering an empty file later.
         */
        fprintf(stderr, "stopping: cannot open %s for the report (%s) — "
                        "this program will stop without explaining itself\n",
                report_where, strerror(errno));
    }
}
/* }}} */

/* {{{ sora_report_path() */
const char *sora_report_path(void)
{
    return report_where;
}
/* }}} */

/* {{{ static void report_without_locks() */
/*
 * **Maximum evidence, no cooperation** — the report for a program
 * that may be holding a lock nobody will ever release.
 *
 * Nothing here takes a lock, walks a list, or follows a pointer that
 * another thread could be freeing. What it reads is atomic counters
 * that are always on, and one integer per worker saying which station
 * that worker was inside. Stations are named by index rather than by
 * name, because the names are an array somebody may be growing right
 * now and reading it would be the crash this report exists to
 * explain.
 *
 * A shelf pointer is safe to follow: the table grows by adding
 * shelves and nothing already placed ever moves, so the shelf array
 * only ever gains entries. Reading a count that is one behind means
 * missing the newest station, which is a smaller wrong than not
 * reporting at all.
 */
static void report_without_locks(map_t *m)
{
    say("== the program stopped on demand, taking no locks ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    int n = m->n_stations;
    say("stations: %d\n", n);
    for (int i = 0; i < n; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        say("  station %d: %ld run, %ld produced\n", i,
            (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
            (long)atomic_load_explicit(&s->produced, memory_order_relaxed));
    }

    if (m->pool) {
        int workers = pool_worker_count(m->pool);
        say("workers: %d\n", workers);
        for (int i = 0; i < workers; i++) {
            int at = pool_worker_station(m->pool, i);
            if (at < 0)
                say("  worker %d: between tasks\n", i);
            else
                say("  worker %d: inside station %d\n", i, at);
        }
    }
    say("== a core dump follows; every thread's stack is in it, "
        "including the box that is not returning ==\n");
}
/* }}} */

/* {{{ static void report_everything() */
/*
 * **The full picture**, gathered on the thread that received the
 * signal. Somebody is standing there and wants to know what happened,
 * so this is free to take every lock it likes — the thread holding
 * this number is running ordinary code, not a handler.
 *
 * It reports the two backlogs separately, because they mean opposite
 * things: values stuck on a station's inputs mean one branch of the
 * graph outran another, while tasks stuck in the queue mean the
 * consumers are slower than the producers.
 */
static void report_everything(map_t *m)
{
    say("== the program was interrupted ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    if (m->pool)
        say("tasks queued and never run: %d\n", pool_queued(m->pool));

    say("\n-- stations --\n");
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        char who[64];
        if (m->station_names && i < m->n_named && m->station_names[i])
            snprintf(who, sizeof who, "%s", m->station_names[i]);
        else
            snprintf(who, sizeof who, "%d", i);
        say("  %s: %ld run, %ld produced\n", who,
            (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
            (long)atomic_load_explicit(&s->produced, memory_order_relaxed));
        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];
            if (atomic_load_explicit(&sl->kind, memory_order_relaxed)
                != IN_PORT_RING)
                continue;
            say("    port %d: %d waiting, %d deepest, grown %d times\n",
                j, map_in_port_depth(m, i, j), sl->high_water, sl->growths);
        }
    }

    if (m->pool) {
        say("\n-- workers --\n");
        int workers = pool_worker_count(m->pool);
        for (int i = 0; i < workers; i++) {
            int at = pool_worker_station(m->pool, i);
            if (at < 0) {
                say("  worker %d: between tasks\n", i);
                continue;
            }
            char who[64];
            if (m->station_names && at < m->n_named && m->station_names[at])
                snprintf(who, sizeof who, "%s", m->station_names[at]);
            else
                snprintf(who, sizeof who, "%d", at);
            say("  worker %d: inside %s\n", i, who);
        }
    }

    /*
     * And the program itself, written as a map file. Somebody
     * diagnosing a program that was edited while it ran needs the
     * shape it had at the end, not the shape the file on disk
     * describes — those stopped being the same thing the moment a
     * program could be built while running.
     */
    say("\n-- the program as it stands --\n");
    FILE *f = fdopen(dup(report_fd), "a");
    if (f) {
        map_dump(m, f);
        fclose(f);
    }
}
/* }}} */

/* {{{ sora_wait() */
int sora_wait(map_t *m)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, finished_signal);

    if (m && m->pool)
        pool_signal_when_finished(m->pool, finished_signal);

    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) != 0)
            continue;

        if (sig == finished_signal) {
            /* The ordinary ending. The last sleeper already broadcast
             * shutdown; this only collects the threads. */
            if (m && m->pool)
                pool_join(m->pool);
            return SORA_EXIT_FINISHED;
        }

        if (sig == SIGTERM) {
            /*
             * **Wind down.** Shut the one door the outside can push
             * work through and go back to waiting, so the program
             * ends exactly the way it would have ended on its own —
             * the last-sleeper rule, unmodified, triggered early.
             *
             * No diagnostics. Nobody asked for any, and a supervisor
             * stopping a healthy program does not want a report it
             * did not request.
             */
            if (m)
                atomic_store_explicit(&m->closing, 1, memory_order_release);
            continue;
        }

        if (sig == SIGINT) {
            if (++interrupts > 1)
                _exit(SORA_EXIT_INTERRUPTED);

            /*
             * **A second one skips everything, and making that true
             * needs the only handler in this file.**
             *
             * Ctrl+C twice has to always work. Everywhere else here,
             * the signals stay blocked and this thread asks for them
             * — which is what frees the reports below to take locks
             * and format text. But a thread that is *gathering* is
             * not asking, so a second interrupt arriving during the
             * gather would sit pending until the gather finished,
             * and the gather is exactly the thing that may never
             * finish: it takes a station's mutex, and the reason
             * somebody is pressing ctrl+C twice may be that a mutex
             * is held by something that will never release it.
             *
             * So for the length of the gather, and only then, SIGINT
             * is unblocked with a handler that does the one thing a
             * handler is unarguably allowed to do. **_exit is on the
             * short list of async-signal-safe calls**; it runs
             * nobody's cleanup, which is precisely what is being
             * escaped.
             *
             * Without this the escape hatch is nominal: it would work
             * only when the report was going to succeed anyway, which
             * is when nobody needs it.
             */
            struct sigaction escape;
            memset(&escape, 0, sizeof escape);
            escape.sa_handler = escape_now;
            sigemptyset(&escape.sa_mask);
            sigaction(SIGINT, &escape, NULL);
            sigset_t just_int;
            sigemptyset(&just_int);
            sigaddset(&just_int, SIGINT);
            pthread_sigmask(SIG_UNBLOCK, &just_int, NULL);

            /*
             * Stop starting new things — a worker inside a box
             * finishes it, because there is no safe way to interrupt
             * executing C — and then gather everything, here, on this
             * thread. Not as a task: a program whose every worker is
             * wedged has no thread free to run one, which is exactly
             * when this report matters most.
             */
            if (m && m->pool)
                pool_stop(m->pool);
            report_everything(m);
            return SORA_EXIT_INTERRUPTED;
        }

        if (sig == SIGQUIT) {
            /*
             * **No cooperation at all.** Do not stop the pool, do not
             * wait for any thread, and above all do not take a single
             * lock — the reason this signal arrived may be that a
             * lock is held by something that will never release it.
             * Then abort, which leaves a core, so a debugger sees
             * every thread's stack including the box that is not
             * returning.
             */
            report_without_locks(m);
            abort();
        }
    }
}
/* }}} */

/* {{{ static int write_capture() */
static int write_capture(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "capture: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    map_dump(m, f);
    if (fclose(f) != 0) {
        fprintf(stderr, "capture: cannot finish writing %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ sora_capture() */
int sora_capture(map_t *m, const char *path)
{
    if (!m || !path || !*path) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }

    /*
     * **Shut the door first, then let it drain.** The entrance is the
     * only way anything outside pushes work in, so closing it is the
     * whole of what "stop accepting new work" can mean here — and with
     * nothing new arriving, the last-sleeper rule ends the program the
     * way it would have ended on its own. Nothing is told to hurry and
     * no task is discarded.
     */
    atomic_store_explicit(&m->closing, 1, memory_order_release);

    if (m->pool) {
        /* Released in case nobody has: a pool whose workers are still
         * parked at the starting gate never drains, and both of these
         * are safe to call again. */
        pool_release(m->pool);
        pool_join(m->pool);
    }

    return write_capture(m, path);
}
/* }}} */

/* {{{ sora_capture_whole() */
int sora_capture_whole(map_t *m, const char *dir)
{
    if (!m || !dir || !*dir) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }

    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "capture: cannot create %s: %s\n",
                dir, strerror(errno));
        return -1;
    }

    /*
     * The sources first, then the description. A reader meeting a
     * half-written capture should find a directory that is missing its
     * description rather than one whose description names sources that
     * are not there — the first is obviously incomplete and the second
     * looks whole and is not.
     */
    if (late_spill_sources(dir) < 0)
        return -1;

    char path[1024];
    if (snprintf(path, sizeof path, "%s/program.map", dir)
        >= (int)sizeof path) {
        fprintf(stderr, "capture: path too long: %s\n", dir);
        return -1;
    }
    return sora_capture(m, path);
}
/* }}} */

/* {{{ sora_capture_now() */
int sora_capture_now(map_t *m, const char *path)
{
    if (!m || !path || !*path) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }
    /* Nothing is shut and nothing is waited for. Whatever a worker is
     * inside stays there, and the artifact says which stations those
     * are. */
    return write_capture(m, path);
}
/* }}} */

/* {{{ sora_stop_now() */
void sora_stop_now(map_t *m, int exit_code, const char *why)
{
    fprintf(stderr, "%s\n", why ? why : "an invalid operation");
    fflush(stderr);

    /*
     * Say everything that can be said on the way out. This is a
     * program being told to do something it refuses, which means
     * somebody is editing it and will want to know what state it was
     * in — the same report an interrupt writes, for the same reason.
     */
    if (m && m->pool)
        pool_stop(m->pool);
    say("== an invalid operation ended this program ==\n%s\n",
        why ? why : "an invalid operation");
    report_everything(m);

    _exit(exit_code);
}
/* }}} */
