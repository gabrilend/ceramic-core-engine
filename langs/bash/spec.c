/* langs/bash/spec.c — Bash language spec for SoraMech.
 *
 * Persistent-subprocess model: each worker thread spawns one bash
 * process at init() and keeps it alive for the run, communicating
 * over a Unix-domain socketpair. Every invoke writes a
 * line-oriented request to the bash side and reads a framed
 * response — orders of magnitude faster than fork+exec per call.
 *
 * Designed in issue 308. The previous iteration's fork+exec path
 * lives only in git history now; this file replaces it wholesale.
 *
 * Protocol details, including the bash-side loop, are in
 * `langs/bash/bash-server.sh`. The C side here is just enough
 * machinery to spawn the subprocess and read/write the framing.
 *
 * Limitations:
 *  - Arguments containing literal newlines are not supported in
 *    this iteration (line-oriented protocol). Most bash boxes
 *    don't need them; binary data should be base64-encoded
 *    upstream.
 *  - The protocol is single-request/single-response; a slow box
 *    serializes calls behind it on the same worker. Multi-worker
 *    pools dispatch in parallel anyway, so this is rarely
 *    the bottleneck.
 */

#define _GNU_SOURCE
#include "lang-spec.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SERVER_PATH "langs/bash/bash-server.sh"

/* {{{ Per-worker handle */
typedef struct {
    int     fd;      /* parent-side socketpair fd */
    pid_t   pid;     /* bash subprocess */
} bash_handle_t;
/* }}} */

/* {{{ server_path() — locate bash-server.sh
 *
 * Precedence:
 *   1. SORAMECH_BASH_SERVER env override.
 *   2. bash-server.sh sitting next to *this* spec.so file. We use
 *      dladdr on a local symbol to find our own .so path, then take
 *      its dirname. This is what makes the compiled/ artifact from
 *      issue 309's compile pipeline portable: copying compiled/langs/
 *      brings spec.so and bash-server.sh together, and the spec
 *      finds the script regardless of cwd.
 *   3. Fall back to the legacy cwd-relative default. The fallback is
 *      logged so a stray dependency on cwd surfaces in the output
 *      rather than mysteriously failing.
 *
 * The resolved path lives in a static buffer; it's set once on first
 * call and reused. */
static const char *server_path(void)
{
    static char  cached[2048];
    static int   resolved = 0;
    if (resolved) return cached;

    const char *env = getenv("SORAMECH_BASH_SERVER");
    if (env && *env) {
        snprintf(cached, sizeof cached, "%s", env);
        resolved = 1;
        return cached;
    }

    /* dladdr on the address of any function defined in this .so
     * returns dli_fname = path the .so was loaded from. */
    Dl_info info;
    if (dladdr((void *)&server_path, &info) != 0 && info.dli_fname) {
        char copy[2048];
        snprintf(copy, sizeof copy, "%s", info.dli_fname);
        char *dir = dirname(copy);
        if (dir) {
            char candidate[2048];
            int w = snprintf(candidate, sizeof candidate,
                             "%s/bash-server.sh", dir);
            if (w > 0 && (size_t)w < sizeof candidate) {
                struct stat st;
                if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) {
                    snprintf(cached, sizeof cached, "%s", candidate);
                    resolved = 1;
                    return cached;
                }
            }
        }
    }

    fprintf(stderr, "bash spec: WARNING — falling back to cwd-relative "
                    "'%s'; set SORAMECH_BASH_SERVER to silence this\n",
            DEFAULT_SERVER_PATH);
    snprintf(cached, sizeof cached, "%s", DEFAULT_SERVER_PATH);
    resolved = 1;
    return cached;
}
/* }}} */

/* {{{ write_all() — write n bytes, retrying on partial writes */
static int write_all(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        p    += w;
        left -= (size_t)w;
    }
    return 0;
}
/* }}} */

/* {{{ read_exact() — read exactly n bytes from fd */
static int read_exact(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        p    += r;
        left -= (size_t)r;
    }
    return 0;
}
/* }}} */

/* {{{ read_line() — read up to cap bytes including a trailing \n */
/* Returns the number of bytes read (including the newline), or -1
 * on error / EOF. The newline is overwritten with '\0' so the
 * line is a NUL-terminated C string of length (return-1). */
static int read_line(int fd, char *buf, int cap)
{
    int n = 0;
    while (n < cap - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        if (c == '\n') {
            buf[n] = '\0';
            return n + 1;
        }
        buf[n++] = c;
    }
    buf[n] = '\0';
    return -1;     /* line longer than buffer */
}
/* }}} */

/* {{{ bash_init() — spawn the subprocess */
static void *bash_init(int worker_idx)
{
    (void)worker_idx;

    /* SOCK_CLOEXEC atomically marks both fds close-on-exec at
     * creation. This matters because multiple worker threads
     * concurrently fork their own bash subprocesses; without
     * CLOEXEC, child A inherits child B's parent-side fd, the
     * bash on the other end never sees EOF when its real parent
     * exits, and we leak orphaned bash processes that hang on
     * read forever. */
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]); close(sv[1]);
        return NULL;
    }
    if (pid == 0) {
        /* Child: route the child-side socket fd to stdin+stdout.
         * dup2'd fds clear CLOEXEC, so bash's stdin/stdout survive
         * across exec. The original sv[1] (still CLOEXEC) closes
         * automatically. */
        close(sv[0]);
        if (dup2(sv[1], STDIN_FILENO)  < 0) _exit(126);
        if (dup2(sv[1], STDOUT_FILENO) < 0) _exit(126);
        close(sv[1]);

        /* Ignore SIGPIPE so the parent dying doesn't immediately
         * kill us — let the read on stdin return EOF and exit cleanly. */
        signal(SIGPIPE, SIG_IGN);

        execlp("bash", "bash", server_path(), (char *)NULL);
        _exit(127);
    }
    /* Parent. */
    close(sv[1]);
    bash_handle_t *h = malloc(sizeof *h);
    if (!h) { close(sv[0]); return NULL; }
    h->fd  = sv[0];
    h->pid = pid;
    return h;
}
/* }}} */

/* {{{ bash_teardown() — graceful shutdown */
static void bash_teardown(void *handle)
{
    if (!handle) return;
    bash_handle_t *h = (bash_handle_t *)handle;
    /* Send "QUIT" so the bash loop exits cleanly; then close the
     * socket and wait for the child. */
    if (h->fd >= 0) {
        (void)write_all(h->fd, "QUIT\n", 5);
        close(h->fd);
    }
    if (h->pid > 0) {
        int status = 0;
        /* Give the child a chance to exit on its own; if it doesn't,
         * SIGTERM it. The waitpid loop handles EINTR. */
        for (int tries = 0; tries < 200; tries++) {
            pid_t r = waitpid(h->pid, &status, WNOHANG);
            if (r == h->pid) goto done;
            if (r < 0 && errno != EINTR) break;
            struct timespec ts = { 0, 5 * 1000 * 1000 };  /* 5 ms */
            nanosleep(&ts, NULL);
        }
        kill(h->pid, SIGTERM);
        waitpid(h->pid, &status, 0);
    }
done:
    free(h);
}
/* }}} */

/* {{{ send_request() — write the request frame */
static int send_request(int fd, const char *file_path, const char *fn_name,
                        const void **input_data, const int *input_sizes,
                        int n_inputs)
{
    char header[64];
    int n = snprintf(header, sizeof header, "%d\n", n_inputs);
    if (n < 0 || n >= (int)sizeof header) return -1;
    if (write_all(fd, header, (size_t)n)              < 0) return -1;
    if (write_all(fd, file_path, strlen(file_path))   < 0) return -1;
    if (write_all(fd, "\n", 1)                        < 0) return -1;
    if (write_all(fd, fn_name, strlen(fn_name))       < 0) return -1;
    if (write_all(fd, "\n", 1)                        < 0) return -1;
    for (int i = 0; i < n_inputs; i++) {
        if (write_all(fd, input_data[i], (size_t)input_sizes[i]) < 0) return -1;
        if (write_all(fd, "\n", 1) < 0) return -1;
    }
    return 0;
}
/* }}} */

/* {{{ bash_invoke() */
static int bash_invoke(void *handle,
                       const char  *file_path,
                       const char  *fn_name,
                       const void **input_data,
                       const int   *input_sizes,
                       int          n_inputs,
                       void        *out_buf,
                       int          out_capacity,
                       int         *out_size)
{
    if (!handle || !file_path || !fn_name) return -1;
    bash_handle_t *h = (bash_handle_t *)handle;

    if (send_request(h->fd, file_path, fn_name,
                     input_data, input_sizes, n_inputs) != 0) {
        return -1;
    }

    /* Response: status\n out_len\n out_bytes. */
    char line[64];
    if (read_line(h->fd, line, sizeof line) < 0) return -1;
    int status = atoi(line);

    if (read_line(h->fd, line, sizeof line) < 0) return -1;
    int n = atoi(line);

    if (n > out_capacity) {
        /* Drain the output to keep the protocol synced, then fail. */
        char drop[1024];
        int left = n;
        while (left > 0) {
            int chunk = left < (int)sizeof drop ? left : (int)sizeof drop;
            if (read_exact(h->fd, drop, (size_t)chunk) < 0) return -1;
            left -= chunk;
        }
        fprintf(stderr, "bash spec: %s:%s returned %d bytes; buffer is %d\n",
                file_path, fn_name, n, out_capacity);
        return -1;
    }
    if (n > 0) {
        if (read_exact(h->fd, out_buf, (size_t)n) < 0) return -1;
    }
    if (out_size) *out_size = n;

    if (status != 0) {
        fprintf(stderr, "bash spec: %s:%s exited with status %d\n",
                file_path, fn_name, status);
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "bash",
    .file_ext = ".sh",
    .init     = bash_init,
    .teardown = bash_teardown,
    .compile  = 0,
    .invoke   = bash_invoke,
};
/* }}} */
