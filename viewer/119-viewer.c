/*
 * 119-viewer.c — the forwarding reader: a window onto a running program.
 *
 * What this is: a small server that maps a watched program's trail and
 * pushes what it finds to a browser, plus the static files the page is
 * made of. It is the only long-running program in this project.
 *
 * How it does it, in general terms: one thread, one poll loop. The ring
 * is opened read-only with the engine's own reader, so the shape of an
 * event is defined in one place and read in one place. Each connected
 * page gets a reader of its own, which is why two people watching do
 * not interfere: neither has anything the other needs.
 *
 * **It can only forward, and that is the whole of its design.** There
 * is no request it answers by writing anything, anywhere. The program
 * being watched cannot be reached through it, and neither can the map
 * file — a layout the viewer offers to save is downloaded by the
 * browser, never written here. A window, not a door.
 *
 * The events go out as server-sent events: plain HTTP, one `data:` line
 * per event, which a browser reads with EventSource and reconnects on
 * its own. No framing to implement and no library to depend on.
 *
 * usage: viewer --trail=<ring> --map=<file> [--port=N] [--root=<dir>]
 */
#include "cera.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENTS 64

/* {{{ struct client */
/*
 * One connected browser. A client is either streaming events — in which
 * case it holds a reader with a cursor of its own — or it was a one-shot
 * request that has already been answered and is waiting to be closed.
 */
struct client {
    int   fd;
    int   streaming;
    cera_watch_reader_t *reader;
    uint64_t lost_total;
    int   told_gone;
};
/* }}} */

/* {{{ static struct settings */
static struct {
    const char *trail;
    const char *map;
    const char *root;
    int         port;
} settings = { NULL, NULL, ".", 8723 };
/* }}} */

/* {{{ static void say_usage(const char *why) */
static void say_usage(const char *why)
{
    if (why)
        fprintf(stderr, "viewer: %s\n", why);
    fprintf(stderr,
        "usage: viewer --trail=<ring> --map=<file> [--port=N] [--root=<dir>]\n"
        "\n"
        "  --trail   the ring a watched program is writing\n"
        "  --map     the map file describing that program's shape\n"
        "  --port    which port to listen on (default 8723)\n"
        "  --root    where this program's own page files live\n");
    exit(2);
}
/* }}} */

/* {{{ static char *read_whole(const char *path, size_t *len) */
static char *read_whole(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (len) *len = got;
    return buf;
}
/* }}} */

/* {{{ static void send_all(int fd, const char *data, size_t n) */
/*
 * Best effort, and deliberately so. A page that has stopped reading is
 * a page that stops receiving; blocking here would let a slow browser
 * hold up every other watcher, which is the same failure the ring
 * exists to prevent one level down.
 */
static void send_all(int fd, const char *data, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(fd, data + sent, n - sent, MSG_NOSIGNAL);
        if (w <= 0)
            return;
        sent += (size_t)w;
    }
}
/* }}} */

/* {{{ static void serve_file(int fd, const char *path, const char *type) */
static void serve_file(int fd, const char *path, const char *type)
{
    size_t n = 0;
    char *body = read_whole(path, &n);
    if (!body) {
        const char *miss = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
        send_all(fd, miss, strlen(miss));
        return;
    }
    char head[512];
    int h = snprintf(head, sizeof head,
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                     "Content-Length: %zu\r\nCache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n", type, n);
    send_all(fd, head, (size_t)h);
    send_all(fd, body, n);
    free(body);
}
/* }}} */

/* {{{ static void open_stream(struct client *c) */
/*
 * The headers that turn a connection into an event stream, and the
 * reader that will feed it. A reader per client is what lets two
 * browsers watch the same program without either seeing the other.
 */
static void open_stream(struct client *c)
{
    const char *head =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: keep-alive\r\n\r\n";
    send_all(c->fd, head, strlen(head));

    c->reader = cera_watch_attach(settings.trail);
    c->streaming = 1;
    c->lost_total = 0;
    c->told_gone = 0;

    if (!c->reader) {
        const char *none =
            "event: notrail\ndata: {\"why\":\"the trail could not be opened\"}\n\n";
        send_all(c->fd, none, strlen(none));
    }
}
/* }}} */

/* {{{ static void pump(struct client *c) */
/*
 * Everything waiting in this client's reader, as one `data:` line each.
 *
 * `lost` rides on the event that follows the gap rather than being
 * announced separately, so a page cannot draw a picture that is missing
 * events without also being handed the number it is missing.
 */
static void pump(struct client *c)
{
    if (!c->reader)
        return;

    cera_watch_event_t e;
    uint64_t lost = 0;
    char line[512];

    for (int budget = 0; budget < 4096; budget++) {
        if (!cera_watch_next(c->reader, &e, &lost))
            break;
        c->lost_total += lost;
        int n = snprintf(line, sizeof line,
                         "data: {\"seq\":%llu,\"ns\":%llu,\"kind\":\"%s\","
                         "\"a\":%u,\"b\":%u,\"c\":%u,\"d\":%u,\"lost\":%llu}\n\n",
                         (unsigned long long)e.seq, (unsigned long long)e.ns,
                         cera_watch_kind_name((int)e.kind),
                         e.a, e.b, e.c, e.d, (unsigned long long)lost);
        send_all(c->fd, line, (size_t)n);
    }

    /* Said once, so a page can stop waiting on a stream that will never
     * move again rather than looking like it is merely quiet. */
    if (!c->told_gone && !cera_watch_writer_alive(c->reader)) {
        const char *gone = "event: ended\ndata: {}\n\n";
        send_all(c->fd, gone, strlen(gone));
        c->told_gone = 1;
    }
}
/* }}} */

/* {{{ static void answer(struct client *c, const char *request) */
static void answer(struct client *c, const char *request)
{
    char path[256] = "/";
    sscanf(request, "%*s %255s", path);

    char full[512];
    if (strcmp(path, "/events") == 0) {
        open_stream(c);
        return;
    }
    if (strcmp(path, "/mapname") == 0) {
        /* Which map is being drawn, so the page can say so and can name
         * a layout after it. */
        const char *slash = strrchr(settings.map, '/');
        const char *base = slash ? slash + 1 : settings.map;
        char head[512];
        int h = snprintf(head, sizeof head,
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                         "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                         strlen(base), base);
        send_all(c->fd, head, (size_t)h);
        return;
    }
    if (strcmp(path, "/map") == 0) {
        serve_file(c->fd, settings.map, "text/plain; charset=utf-8");
        return;
    }
    if (strcmp(path, "/layout") == 0) {
        /* A layout saved beside the map, if somebody put one there.
         * Read only: this program writes nothing, ever. */
        snprintf(full, sizeof full, "%s.lay", settings.map);
        serve_file(c->fd, full, "application/json");
        return;
    }
    if (strcmp(path, "/") == 0) {
        snprintf(full, sizeof full, "%s/120-viewer.html", settings.root);
        serve_file(c->fd, full, "text/html; charset=utf-8");
        return;
    }
    if (strcmp(path, "/viewer.css") == 0) {
        snprintf(full, sizeof full, "%s/121-viewer.css", settings.root);
        serve_file(c->fd, full, "text/css");
        return;
    }
    if (strcmp(path, "/viewer.js") == 0) {
        snprintf(full, sizeof full, "%s/122-viewer.js", settings.root);
        serve_file(c->fd, full, "text/javascript");
        return;
    }

    const char *miss = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                       "Connection: close\r\n\r\n";
    send_all(c->fd, miss, strlen(miss));
}
/* }}} */

/* {{{ static void drop(struct client *c) */
static void drop(struct client *c)
{
    if (c->reader)
        cera_watch_detach(c->reader);
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->reader = NULL;
    c->streaming = 0;
}
/* }}} */

/* {{{ static volatile sig_atomic_t stopping */
static volatile sig_atomic_t stopping = 0;
static void note_stop(int signo) { (void)signo; stopping = 1; }
/* }}} */

/* {{{ int main(int argc, char **argv) */
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--trail=", 8) == 0)      settings.trail = argv[i] + 8;
        else if (strncmp(argv[i], "--map=", 6) == 0)   settings.map   = argv[i] + 6;
        else if (strncmp(argv[i], "--root=", 7) == 0)  settings.root  = argv[i] + 7;
        else if (strncmp(argv[i], "--port=", 7) == 0)  settings.port  = atoi(argv[i] + 7);
        else say_usage("unrecognised argument");
    }
    if (!settings.trail) say_usage("no --trail: there is nothing to watch");
    if (!settings.map)   say_usage("no --map: without it there is no graph to draw");

    /* A broken pipe is an ordinary event here — a page closed its tab —
     * and killing the server for it would make watching fragile. */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, note_stop);
    signal(SIGTERM, note_stop);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        fprintf(stderr, "viewer: no socket: %s\n", strerror(errno));
        return 1;
    }
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in where;
    memset(&where, 0, sizeof where);
    where.sin_family = AF_INET;
    where.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    where.sin_port = htons((uint16_t)settings.port);

    if (bind(listener, (struct sockaddr *)&where, sizeof where) != 0) {
        fprintf(stderr, "viewer: cannot listen on port %d: %s\n",
                settings.port, strerror(errno));
        return 1;
    }
    listen(listener, 16);

    printf("viewer: watching %s\n", settings.trail);
    printf("viewer: drawing %s\n", settings.map);
    printf("viewer: open http://localhost:%d/\n", settings.port);
    fflush(stdout);

    struct client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].reader = NULL;
        clients[i].streaming = 0;
    }

    while (!stopping) {
        struct pollfd fds[MAX_CLIENTS + 1];
        int map_back[MAX_CLIENTS + 1];
        int n = 0;

        fds[n].fd = listener;
        fds[n].events = POLLIN;
        map_back[n] = -1;
        n++;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd < 0) continue;
            fds[n].fd = clients[i].fd;
            fds[n].events = clients[i].streaming ? 0 : POLLIN;
            map_back[n] = i;
            n++;
        }

        /* A short wait rather than none: streaming clients are fed on
         * the timeout, so the loop costs nothing while nothing moves
         * and still forwards promptly when something does. */
        if (poll(fds, (nfds_t)n, 40) < 0 && errno != EINTR)
            break;

        if (fds[0].revents & POLLIN) {
            int fresh = accept(listener, NULL, NULL);
            if (fresh >= 0) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (clients[i].fd < 0) { slot = i; break; }
                if (slot < 0) close(fresh);
                else {
                    clients[slot].fd = fresh;
                    clients[slot].streaming = 0;
                    clients[slot].reader = NULL;
                }
            }
        }

        for (int i = 1; i < n; i++) {
            struct client *c = &clients[map_back[i]];
            if (c->fd < 0) continue;
            if (fds[i].revents & (POLLHUP | POLLERR)) { drop(c); continue; }
            if (!(fds[i].revents & POLLIN)) continue;

            char request[2048];
            ssize_t got = recv(c->fd, request, sizeof request - 1, 0);
            if (got <= 0) { drop(c); continue; }
            request[got] = '\0';
            answer(c, request);
            if (!c->streaming)
                drop(c);
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd >= 0 && clients[i].streaming)
                pump(&clients[i]);
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd >= 0)
            drop(&clients[i]);
    close(listener);
    printf("\nviewer: stopped\n");
    return 0;
}
/* }}} */
