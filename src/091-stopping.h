/*
 * 091-stopping.h — every way a program ends except the happy one.
 *
 * What this is: issue 106. Running out of work is handled and proven
 * elsewhere — the last worker to fall asleep looks once more, finds
 * nothing, and stops everyone. This is the rest: a program told to
 * stop, and a program that found something it refuses to continue
 * past.
 *
 * How it does it, in general terms: **no handler is installed
 * anywhere.** The three signals are blocked in every thread, and the
 * thread that started the program waits for one to arrive as an
 * ordinary value. That is the whole trick, and it removes the hardest
 * constraint in the design — a signal handler may call almost
 * nothing, while a thread that woke up holding a number may call
 * anything at all. It can take locks, format text, walk the station
 * table.
 *
 * The pool's own ending arrives at that same waiting point, as one
 * more signal. One place to wait, woken for two reasons, told apart
 * by which number came back.
 */
#ifndef SORA_STOPPING_H
#define SORA_STOPPING_H

#include "018-station.h"

/* {{{ exit codes — issue 106 */
/*
 * **Everything below the signal offset belongs to the program**; the
 * offset and above belongs to signals by convention, so no meaning
 * here ever reaches up there.
 *
 * The three middle codes are the ones worth having. They make the
 * distinction this project already draws — a fault the caller can
 * correct and retry, against one it cannot — visible to a shell
 * script rather than only to somebody reading the message.
 */
#define SORA_EXIT_FINISHED     0    /* ran out of work, or was asked to stop */
#define SORA_EXIT_BAD_FILE     65   /* a map file the engine refused        */
#define SORA_EXIT_BAD_CALL     70   /* an invalid construction call         */
#define SORA_EXIT_NO_RESOURCE  71   /* out of memory; no edit fixes it      */
#define SORA_EXIT_INTERRUPTED  130  /* a person interrupted it              */
/* The quit path is not in this list: it aborts, and the shell
 * computes 131 from the signal itself. Naming a number here would be
 * inventing an agreement that already exists. */
/* }}} */

/* {{{ sora_prepare() — issue 106 */
/*
 * **Block the three signals in every thread, and open the report's
 * destination.** Call this before any thread exists, because a thread
 * inherits the mask of the thread that made it — which is what makes
 * "every thread" true without visiting any of them.
 *
 * `report_path` names where a diagnostic report goes; NULL means the
 * project's RAM-backed scratch directory, which is right for something
 * read while debugging and useless as a post-mortem after a reboot —
 * that is what a core dump is for.
 *
 * **The destination is opened now rather than while dying**, and that
 * is not tidiness. The directory lives on a filesystem a reboot
 * empties, so it may be absent, and creating it on a failure path
 * means a system call that can fail for reasons a dying program can do
 * nothing about. A descriptor is an integer, and writing to an integer
 * is the one file operation available on every path here — including
 * the one that is forbidden to take a lock.
 */
void sora_prepare(const char *report_path);

/* Where the report will be written. Valid after sora_prepare. */
const char *sora_report_path(void);
/* }}} */

/* {{{ sora_wait() — issue 106 */
/*
 * **Wait for the program to end, however it ends**, and return the
 * code the process should exit with.
 *
 * The pool must already be released. Three signals are answered, and
 * they form a progression: each needs less cooperation from the
 * program than the one before, and each is the right answer for a
 * program in worse condition than the last.
 *
 * | | polite shutdown | interrupt | quit |
 * |---|---|---|---|
 * | who sends it | a service manager | a person at a terminal | a person who wants evidence |
 * | what it means | wind down, there is time | stop, and tell me why | stop now, leave the body |
 * | diagnostics | none | everything | only what needs no lock |
 * | waits for running boxes | yes | no | no |
 * | how it ends | zero, by the existing rule | 130, explicitly | aborts, leaving a core |
 *
 * **The polite path adds no mechanism at all**, which is the argument
 * for it: it shuts the one door the outside can push work through and
 * then goes back to waiting, so the program ends exactly the way it
 * would have ended on its own. It writes no diagnostics, because
 * nobody asked for any and a supervisor stopping a healthy program
 * does not want a report it did not request.
 *
 * **It borrows a clock rather than inventing one.** On a wedged
 * program this path never completes, which is correct: whatever sent
 * the signal already has a timer and will escalate to the uncatchable
 * kill when it expires. That supervisor's clock is the only clock in
 * the system that knows how long is too long for this deployment, and
 * a guess made here is wrong on a slow machine and wrong differently
 * on a fast one.
 *
 * **A second interrupt skips everything and exits at once.** Ctrl+C
 * twice has to always work; a diagnostic path that can itself get
 * stuck would trap the person it was written for, which is the exact
 * failure it exists to prevent.
 */
int sora_wait(map_t *m);
/* }}} */

/* {{{ sora_capture() / sora_capture_now() — issue 712 */
/*
 * **Put a running program down on disk so it can be picked up again.**
 *
 * `sora_capture` is the polite one. It shuts the entrance — the only
 * way anything outside pushes work in — lets everything already in
 * flight finish and deliver, waits for the workers to go home, and
 * then writes the artifact. What it produces is **complete** by
 * construction: no task was running when it was written, so nothing
 * was lost.
 *
 * **There is no bound on the wait, deliberately.** Quiet is decidable
 * exactly — the pool knows when its queue is empty and every worker
 * is asleep — so the only case that never returns is a box that never
 * returns, and nothing inside the process can tell that from a box
 * that is merely slow. Whoever asked for the capture already has a
 * clock, and theirs is the only one that knows how long is too long
 * for this deployment. This is the same reasoning that keeps a
 * timeout out of `sora_wait`, and it is not weaker here.
 *
 * `sora_capture_now` is for when that clock runs out. It writes
 * immediately, whatever is happening, and the artifact says so: a
 * header naming every station a worker is still inside, and the plain
 * statement that their inputs are lost and their results were never
 * delivered.
 *
 * **The incompleteness is stated, never inferred.** A program reviving
 * such an artifact without being told would be quietly missing work
 * somebody computed, which is the standing rule of this engine wearing
 * a different hat: a fallback is a warning and a warning is an error.
 * Reading one back is refused unless the caller asks for salvage.
 *
 * Both return 0, or -1 with a reason on stderr.
 */
int sora_capture(map_t *m, const char *path);
int sora_capture_now(map_t *m, const char *path);
/* }}} */

/* {{{ sora_stop_now() — issue 106 */
/*
 * **An invalid operation ends the program**, having first said
 * everything it can about what went wrong.
 *
 * Not a return value a caller may discard. The surface still hands a
 * refusal *back* rather than dying where the failure happens, because
 * a caller reading a file collects every mistake in it and presents
 * them together — so a refusal travels, accumulates, and the stop
 * happens once with the whole list. A single instruction arriving
 * alone produces a list of one.
 *
 * What this buys is that **a program cannot be left half-built by
 * ignored refusals**, because there is no surviving path in which a
 * refused instruction leaves a program running that somebody believes
 * they just edited successfully.
 *
 * `m` may be NULL when there is no program to describe yet.
 */
void sora_stop_now(map_t *m, int exit_code, const char *why);
/* }}} */

#endif
