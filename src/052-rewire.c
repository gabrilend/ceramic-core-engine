/*
 * 052-rewire.c — changing the shape while it runs.
 *
 * What this is: issue 704, the feature the whole design has been
 * quietly preparing for. Wires hold station indices rather than
 * addresses; stations never move; a destination list is an immutable
 * array published by one write, so a delivery walk reads it without a
 * lock. Each of those was chosen partly for this moment, and this
 * file is the debt being redeemed.
 *
 * The third of those used to read "delivery snapshots destination
 * lists under the station's mutex", which is what it did before issue
 * 214 — a lock and a copy proportional to fan-out on every value the
 * engine moved. The preparation survived the mechanism being
 * replaced by a better one.
 *
 * A fourth preparation was here and is gone: the gather cycle check
 * ran when a connection was made rather than when it was traversed,
 * and repointing a gather wire at runtime was this file's third
 * operation. The pull path was removed in issue 210, so what remains
 * is connecting and disconnecting.
 *
 * How it does it, in general terms: one rewiring lock makes edge
 * validation and list mutation a single operation — two threads each
 * adding an individually legal edge can produce an illegal pair, so
 * the check and the insertion are never separated. List surgery
 * additionally happens under the owning station's mutex, the same
 * lock delivery snapshots under, so no walker can be left holding a
 * freed node.
 *
 * Refusal behaviour, decided rather than defaulted: a refusal is
 * handed back and the caller decides. A loader that dies serves its
 * author, but a running engine that dies because a control surface
 * sent one bad instruction takes the plant down with it.
 *
 * There are three faces on that now (issue 212). One returns the
 * reason, which is what lets somebody reading a file collect every
 * mistake and present them together; one prints it and returns -1,
 * which is what live editing's callers already expected; one stops
 * the program, which is what construction wants. The cost of the
 * middle one — a caller can ignore the -1 — is weighed in the
 * first-pass report, and the first face is the answer to it.
 */
#include "049-observe.h"
#include "026-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ refuse() */
static int refuse(const char *what)
{
    fprintf(stderr, "rewire: refused: %s\n", what);
    return -1;
}
/* }}} */

/* {{{ said() */
/*
 * A refusal that travels upward instead of being printed where it
 * happened (issue 212). The caller decides what to do with it —
 * collect it with the others in a file, hand it to a control surface,
 * or stop the program.
 *
 * Per thread, because two threads may be editing two different maps
 * and a shared buffer would let one overwrite the other's complaint.
 * Valid until this thread's next refusal.
 */
static const char *said(const char *what)
{
    /* Wide enough for the longest refusal below, which names a
     * station, a port and a kind. A message that arrived
     * truncated would be one somebody had to guess the end of. */
    static _Thread_local char kept[256];
    snprintf(kept, sizeof kept, "%s", what);
    return kept;
}
/* }}} */

/* {{{ station_kind_out_port_limit() */
static int station_kind_out_port_limit(unsigned char kind)
{
    static const int limits[STATION_KIND_COUNT] = {
        [STATION_PLAIN] = 1, [STATION_COMPARATOR] = 3, [STATION_ITERATOR] = 0,
    };
    return kind < STATION_KIND_COUNT ? limits[kind] : 1;
}
/* }}} */

/* {{{ map_wire() */
/*
 * **Draw a wire, at any moment** (issue 212) — while a program is
 * being assembled, or on a running one with workers in flight. There
 * is one implementation and it applies every rule, because the rules
 * were never about *when*: a sink has nothing to wire from whether or
 * not the pool has started, and a destination that is not a buffer
 * has nowhere to put a value either way.
 *
 * There used to be two. Construction had its own, which checked less
 * — it did not ask whether the destination was a buffer, and it did
 * not check the widths — and stopped the program when refused;
 * runtime editing had another, which checked everything and printed.
 * Two sets of rules that were supposed to agree, and one of them
 * quietly weaker, is how a program becomes buildable from a file and
 * unbuildable by hand.
 *
 * Returns NULL when the wire was drawn, or a sentence saying why not.
 * The string is valid until this thread's next refusal.
 */
const char *map_wire(map_t *m, int from_station, int port,
                     int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);

    /* Every load-time rule, per edge (issue 604 made callable). */
    if (from_station < 0 || from_station >= m->n_stations
        || to_station < 0 || to_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a station index outside the table");
    }
    station_t *from = map_station(m, from_station);
    station_t *to = map_station(m, to_station);
    /* Only construction used to ask this, and it is the one rule the
     * runtime path was missing rather than the other way round: an
     * empty place in the table has no ports to wire and no size to
     * check against, so every question below it would be asked of
     * nothing. */
    if (!from->call || !to->call) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("wiring a station that has no box placed yet — place, "
                    "then wire");
    }
    if (from->out_size == 0) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("wiring from a sink — nothing comes out of it");
    }
    int limit = station_kind_out_port_limit(from->kind);
    if (port < 0 || (limit > 0 && port >= limit)) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a port index beyond what this station kind can mean");
    }
    if (to_port < 0 || to_port >= to->n_in_ports) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a destination port the box does not have");
    }
    in_port_t *dest = &to->in_ports[to_port];
    if (dest->kind != IN_PORT_RING) {
        /* Named by station and port, because "the destination port"
         * is not something anybody can go and look at. The whole-map
         * check said it this way and this refusal now arrives first,
         * so it had better say as much. */
        char who[64];
        if (m->station_names && to_station < m->n_named
            && m->station_names[to_station])
            snprintf(who, sizeof who, "%s", m->station_names[to_station]);
        else
            snprintf(who, sizeof who, "%d", to_station);
        char message[224];
        snprintf(message, sizeof message,
                 "an arrow lands on %s.%d, but that port is %s, not a buffer "
                 "— the value would have nowhere to go",
                 who, to_port, in_port_kind_name(dest->kind));
        pthread_mutex_unlock(&m->rewire_mutex);
        return said(message);
    }
    /*
     * The wire check, by **width** rather than by type name (issue
     * 309). Identical layouts under different names now wire, which
     * is the capability this buys; same-width types of different
     * layouts also wire, which is the accepted cost, stated as a
     * non-guarantee in 058.
     *
     * The names still ride along in the message, because "4 bytes
     * against 4 bytes" is not a sentence anybody can act on — and
     * both widths ride along beside them, because "box returns vec4,
     * port takes stats" does not say why those disagree.
     */
    if (from->in_ports && to->in_ports) {
        /*
         * **The width decides, and only the width** (issue 309). Two
         * boxes may spell one shape differently and mean the same
         * data, so a wire is legal when both sides count the same
         * bytes — comparing the names would refuse a connection that
         * is perfectly sound, which is the whole reason names stopped
         * being what a wire is checked against.
         */
        if (from->out_size != dest->elem_size) {
            /*
             * The spellings, fetched **here and only here**, because
             * this is the one place they are wanted: "4 bytes against
             * 8 bytes" names no fix, while "box returns int, port
             * takes double" names two.
             *
             * A lookup on the refusal path rather than a field on
             * every station, and the difference matters more than the
             * few bytes: a type name stored on a station is a type
             * name the engine *carries*, and carrying them is what
             * this line of work is removing. A refusal is rare enough
             * to pay for its own message.
             *
             * When the box records go, this reads the spelling out of
             * the box source the binary carries (issue 311c) — the
             * exact text that was compiled, so a name reported can
             * never come from a source that has since changed on disk.
             */
            const box_info_t *b = from->box_name
                                ? registry_find(from->box_name) : NULL;
            char message[192];
            snprintf(message, sizeof message,
                     "box returns %s (%d bytes), port takes %s (%d bytes)",
                     b ? b->return_type : "?", from->out_size,
                     dest->type_name ? dest->type_name : "?", dest->elem_size);
            pthread_mutex_unlock(&m->rewire_mutex);
            return said(message);
        }
    }

    /* The surgery, under the owning station's mutex so no delivery
     * snapshot is mid-walk. Ports are created empty up to the index,
     * exactly as the loader would. */
    pthread_mutex_lock(&from->mutex);
    while (from->n_out_ports <= port) {
        out_port_t *fresh = calloc(1, sizeof *fresh);
        if (!fresh) {
            pthread_mutex_unlock(&from->mutex);
            pthread_mutex_unlock(&m->rewire_mutex);
            return said("out of memory for a port");
        }
        out_port_t **link = &from->out_ports;
        while (*link)
            link = &(*link)->next;
        *link = fresh;
        from->n_out_ports++;
    }
    out_port_t *p = station_out_port(from, port);

    /*
     * A whole new set, published by one write (issue 214). Walkers
     * already inside the old one keep walking it and are not
     * disturbed; the old set is filed rather than freed, because one
     * of them may be in it right now.
     */
    dest_set_t *old = out_port_dests(p);
    dest_set_t *fresh_set = dest_set_build(old, to_station, to_port, -1, -1);
    atomic_store_explicit(&p->dests, fresh_set, memory_order_release);
    pthread_mutex_unlock(&from->mutex);
    map_retire(m, old, free);

    pthread_mutex_unlock(&m->rewire_mutex);
    return NULL;
}
/* }}} */

/* {{{ map_rewire_connect() */
/*
 * The same operation, for a caller that wants the refusal printed and
 * a code back. Kept because that is what live editing's callers
 * already expect; what changed is that it is a face on one
 * implementation rather than a second implementation.
 */
int map_rewire_connect(map_t *m, int from_station, int port,
                       int to_station, int to_port)
{
    const char *no = map_wire(m, from_station, port, to_station, to_port);
    return no ? refuse(no) : 0;
}
/* }}} */

/* {{{ map_rewire_disconnect() */
int map_rewire_disconnect(map_t *m, int from_station, int port,
                          int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);
    if (from_station < 0 || from_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("a station index outside the table");
    }
    station_t *from = map_station(m, from_station);

    pthread_mutex_lock(&from->mutex);
    out_port_t *p = station_out_port(from, port);
    dest_set_t *old = out_port_dests(p);
    dest_set_t *fresh_set = NULL;
    int found = 0;
    for (int i = 0; old && i < old->n; i++)
        if (old->items[i].station == to_station
            && old->items[i].port == to_port) {
            found = 1;
            break;
        }
    if (found) {
        fresh_set = dest_set_build(old, -1, -1, to_station, to_port);
        atomic_store_explicit(&p->dests, fresh_set, memory_order_release);
    }
    pthread_mutex_unlock(&from->mutex);
    pthread_mutex_unlock(&m->rewire_mutex);

    if (!found)
        return refuse("no such wire to remove");

    /* Filed, not freed: a walker may be inside the old set right now
     * (issue 214). A value already on its way down the removed wire
     * is delivered, which is indistinguishable from having been
     * delivered a moment earlier and is fine (issue 704). */
    map_retire(m, old, free);

    return 0;
}
/* }}} */


/* {{{ removed_parts_t / reclaim_station() */
/*
 * A removed station's parts, reclaimed by the scrapyard once nobody
 * can still be inside a task built from it.
 *
 * The station record itself stays in the table — it is what an index
 * means, and indices are what wires are made of. What goes is
 * everything hanging off it, and the shim, whose absence is what says
 * the place is free for the next station.
 */
typedef struct removed_parts {
    station_t  *station;
    out_port_t *out_ports;
    in_port_t  *in_ports;
    int         n_in_ports;
    char       *name;
} removed_parts_t;

static void reclaim_station(void *p)
{
    removed_parts_t *r = p;

    out_port_t *port = r->out_ports;
    while (port) {
        free(out_port_dests(port));
        out_port_t *next = port->next;
        free(port);
        port = next;
    }
    for (int i = 0; i < r->n_in_ports; i++) {
        in_port_free_pages(&r->in_ports[i]);
        in_port_constant_free(&r->in_ports[i]);
    }
    free(r->in_ports);
    free(r->name);

    /* Last, and this is the moment the place becomes free: everything
     * that reads a station checks the shim first. */
    station_t *s = r->station;
    s->out_ports = NULL;
    s->n_out_ports = 0;
    s->in_ports = NULL;
    s->n_in_ports = 0;
    s->out_size = 0;
    s->compare = NULL;
    s->cursor = 0;
    s->call = NULL;
    atomic_store_explicit(&s->removed, 0, memory_order_release);

    free(r);
}
/* }}} */

/* {{{ map_remove_station() */
int map_remove_station(map_t *m, int station)
{
    pthread_mutex_lock(&m->rewire_mutex);

    if (station < 0 || station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("removing a station outside the table");
    }
    station_t *s = map_station(m, station);
    if (!s->call || atomic_load_explicit(&s->removed, memory_order_acquire)) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("removing a station that is not there");
    }

    /*
     * Marked first, so nothing new starts from it while the wires are
     * being cut. Values already on their way are discarded when they
     * arrive, which is what this engine already does with a value
     * that has nowhere to go.
     */
    pthread_mutex_lock(&s->mutex);
    atomic_store_explicit(&s->removed, 1, memory_order_release);
    pthread_mutex_unlock(&s->mutex);

    /*
     * Every wire that names this station, cut before the station
     * goes. A wire lives only as a destination record on some
     * station's output port, so this walk finds all of them — and
     * because it happens first, nothing stale can survive to be
     * followed afterwards. That is what makes reusing the place safe
     * without a version on every wire.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *other = map_station(m, i);
        if (!other->call)
            continue;
        pthread_mutex_lock(&other->mutex);
        for (out_port_t *p = other->out_ports; p; p = p->next) {
            dest_set_t *old = out_port_dests(p);
            if (!old)
                continue;
            int names_it = 0;
            for (int d = 0; d < old->n; d++)
                if (old->items[d].station == station)
                    names_it = 1;
            if (!names_it)
                continue;
            /* Rebuilt without every wire to this station, in one new
             * set rather than one per wire, so a walker sees the
             * before or the after and never a partial cut. */
            dest_set_t *fresh =
                calloc(1, sizeof *fresh
                          + (size_t)(old->n > 0 ? old->n : 1)
                            * sizeof(destination_t));
            if (!fresh) {
                pthread_mutex_unlock(&other->mutex);
                pthread_mutex_unlock(&m->rewire_mutex);
                return refuse("out of memory rebuilding a destination set");
            }
            int out = 0;
            for (int d = 0; d < old->n; d++)
                if (old->items[d].station != station)
                    fresh->items[out++] = old->items[d];
            fresh->n = out;
            atomic_store_explicit(&p->dests, fresh, memory_order_release);
            map_retire(m, old, free);
        }
        pthread_mutex_unlock(&other->mutex);
    }

    /*
     * Its parts handed to the scrapyard, which frees them and clears
     * the record once nobody can still be inside a task built from
     * this station. Nothing is detached here: a task being built
     * right now reads the port count and the return size, and they
     * have to still be there.
     */
    removed_parts_t *parts = calloc(1, sizeof *parts);
    if (!parts) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("out of memory removing a station");
    }
    parts->station = s;
    parts->out_ports = s->out_ports;
    parts->in_ports = s->in_ports;
    parts->n_in_ports = s->n_in_ports;
    if (m->station_names) {
        parts->name = m->station_names[station];
        m->station_names[station] = NULL;
    }

    map_retire(m, parts, reclaim_station);
    pthread_mutex_unlock(&m->rewire_mutex);
    return 0;
}
/* }}} */
