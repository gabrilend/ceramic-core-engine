/*
 * 114-test-error-handler.c — a host hears why the engine stopped.
 *
 * What this proves: an installed handler is called with the engine's
 * own message and the code the process is about to exit with; it is
 * called exactly once; the process still ends, with that same code;
 * and with no handler installed nothing changes.
 *
 * How it does it: every case runs in a forked child, because every case
 * ends in a dead process. The child triggers a refusal and the parent
 * reads what the handler said down a pipe and checks the wait status.
 * A test that asserted from inside the dying process would be asserting
 * in a process that is about to stop regardless of the answer.
 */
#include "cera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int      calls = 0;
static int      seen_code = 0;
static char     seen_message[512];
static int      report_pipe = -1;

/* {{{ static void remember(const char *message, int exit_code) */
static void remember(const char *message, int exit_code)
{
    calls++;
    seen_code = exit_code;
    snprintf(seen_message, sizeof seen_message, "%s", message);

    /* Written from inside the handler, because after it returns the
     * process is gone and nothing else of ours runs. */
    dprintf(report_pipe, "%d\n%d\n%s\n", calls, exit_code, message);
}
/* }}} */

/* {{{ static void never_called(const char *message, int exit_code) */
static void never_called(const char *message, int exit_code)
{
    (void)message; (void)exit_code;
    dprintf(report_pipe, "THE REMOVED HANDLER RAN\n");
}
/* }}} */

/* {{{ static void trigger_a_refusal(void) */
/* A submitter unregistered more times than it was registered: an
 * invalid call, needing nothing but a pool to reach. */
static void trigger_a_refusal(void)
{
    cera_pool_t *p = cera_pool_create(1, NULL, NULL);
    cera_pool_submitter_unregister(p);
    dprintf(report_pipe, "STILL RUNNING AFTER THE REFUSAL\n");
}
/* }}} */

/* {{{ struct outcome */
struct outcome {
    int  exit_code;      /* what the process exited with */
    int  signalled;      /* non-zero if it died on a signal instead */
    char said[1024];     /* everything the child wrote down the pipe */
};
/* }}} */

/* {{{ static struct outcome run_child(int install, int then_remove) */
static struct outcome run_child(int install, int then_remove)
{
    int fds[2];
    if (pipe(fds) != 0) {
        fprintf(stderr, "  pipe failed\n");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        report_pipe = fds[1];
        /* stderr would otherwise mix the engine's own message into the
         * test's output; the pipe is what this test reads. */
        freopen("/dev/null", "w", stderr);
        /* The removal case installs a handler that shouts if it runs,
         * so "not called" is proven rather than inferred from silence. */
        if (then_remove) {
            cera_on_error(never_called);
            cera_on_error(NULL);
        } else if (install) {
            cera_on_error(remember);
        }
        trigger_a_refusal();
        _exit(99);
    }

    close(fds[1]);
    struct outcome o;
    memset(&o, 0, sizeof o);
    ssize_t got = read(fds[0], o.said, sizeof o.said - 1);
    if (got > 0) o.said[got] = '\0';
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))        o.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) o.signalled = WTERMSIG(status);
    return o;
}
/* }}} */

/* {{{ int main(void) */
int main(void)
{
    int failures = 0;

    /* With a handler: it is told, once, and the process still dies. */
    struct outcome with = run_child(1, 0);
    if (strstr(with.said, "STILL RUNNING") != NULL) {
        fprintf(stderr, "  the engine kept going after a refusal\n");
        failures++;
    }
    if (with.said[0] == '\0') {
        fprintf(stderr, "  the handler was never called\n");
        failures++;
    } else {
        int n = 0, code = 0;
        char msg[512] = {0};
        sscanf(with.said, "%d\n%d\n%511[^\n]", &n, &code, msg);
        if (n != 1) {
            fprintf(stderr, "  the handler ran %d times, not once\n", n);
            failures++;
        }
        if (code != CERA_EXIT_BAD_CALL) {
            fprintf(stderr, "  the handler was told code %d, not %d\n",
                    code, CERA_EXIT_BAD_CALL);
            failures++;
        }
        if (strstr(msg, "submitter") == NULL) {
            fprintf(stderr, "  the message did not name the fault: '%s'\n", msg);
            failures++;
        }
        if (strchr(msg, '\n') != NULL) {
            fprintf(stderr, "  the message carried a line break\n");
            failures++;
        }
    }
    if (with.exit_code != CERA_EXIT_BAD_CALL) {
        fprintf(stderr, "  exited %d (signal %d), not %d\n",
                with.exit_code, with.signalled, CERA_EXIT_BAD_CALL);
        failures++;
    }
    if (!failures)
        printf("  a host is told what happened, once, and the engine still stops\n");

    /* Without one: the same ending, and nothing extra. */
    struct outcome without = run_child(0, 0);
    if (without.exit_code != CERA_EXIT_BAD_CALL || without.said[0] != '\0') {
        fprintf(stderr, "  with no handler: exited %d, said '%s'\n",
                without.exit_code, without.said);
        failures++;
    } else {
        printf("  with no handler installed, nothing changes\n");
    }

    /* Removed again: not called. */
    struct outcome removed = run_child(1, 1);
    if (strstr(removed.said, "REMOVED HANDLER RAN") != NULL
        || removed.said[0] != '\0') {
        fprintf(stderr, "  a removed handler still ran: '%s'\n", removed.said);
        failures++;
    } else if (removed.exit_code != CERA_EXIT_BAD_CALL) {
        fprintf(stderr, "  after removal: exited %d\n", removed.exit_code);
        failures++;
    } else {
        printf("  a handler passed NULL is gone\n");
    }

    return failures ? 1 : 0;
}
/* }}} */
