/*
 * 052-rewire.c — changing the shape while it runs.
 *
 * What this is: issue 704, the feature the whole design has been
 * quietly preparing for. Wires hold station indices rather than
 * addresses; stations never move; delivery snapshots destination
 * lists under the station's mutex. Each of those was chosen partly
 * for this moment, and this file is the debt being redeemed.
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
 * Refusal behaviour, decided rather than defaulted: refusals return
 * -1 with the reason on stderr. A loader that dies serves its
 * author, but a running engine that dies because a control surface
 * sent one bad instruction takes the plant down with it. The cost —
 * a caller can ignore the -1 — is weighed in the first-pass report.
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

/* {{{ station_kind_port_limit() */
static int station_kind_port_limit(unsigned char kind)
{
    static const int limits[STATION_KIND_COUNT] = {
        [STATION_PLAIN] = 1, [STATION_COMPARATOR] = 3, [STATION_ITERATOR] = 0,
    };
    return kind < STATION_KIND_COUNT ? limits[kind] : 1;
}
/* }}} */

/* {{{ map_rewire_connect() */
int map_rewire_connect(map_t *m, int from_station, int port,
                       int to_station, int to_slot)
{
    pthread_mutex_lock(&m->rewire_mutex);

    /* Every load-time rule, per edge (issue 604 made callable). */
    if (from_station < 0 || from_station >= m->n_stations
        || to_station < 0 || to_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("a station index outside the table");
    }
    station_t *from = &m->stations[from_station];
    station_t *to = &m->stations[to_station];
    if (from->out_size == 0) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("wiring from a sink — nothing comes out of it");
    }
    int limit = station_kind_port_limit(from->kind);
    if (port < 0 || (limit > 0 && port >= limit)) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("a port index beyond what this station kind can mean");
    }
    if (to_slot < 0 || to_slot >= to->n_slots) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("a destination slot the box does not have");
    }
    slot_t *dest = &to->slots[to_slot];
    if (dest->kind != SLOT_RING) {
        char message[192];
        snprintf(message, sizeof message,
                 "the destination port is %s, not a buffer — the value would "
                 "have nowhere to go", slot_kind_name(dest->kind));
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse(message);
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
     * slot takes stats" does not say why those disagree.
     */
    if (from->slots && to->slots) {
        const box_info_t *b =
            registry_find(registry_box_name_for_shim(from->call));
        if (b && b->return_size != dest->elem_size) {
            char message[192];
            snprintf(message, sizeof message,
                     "box returns %s (%d bytes), slot takes %s (%d bytes)",
                     b->return_type, b->return_size,
                     dest->type_name ? dest->type_name : "?", dest->elem_size);
            pthread_mutex_unlock(&m->rewire_mutex);
            return refuse(message);
        }
    }

    /* The surgery, under the owning station's mutex so no delivery
     * snapshot is mid-walk. Ports are created empty up to the index,
     * exactly as the loader would. */
    pthread_mutex_lock(&from->mutex);
    while (from->n_ports <= port) {
        port_t *fresh = calloc(1, sizeof *fresh);
        if (!fresh) {
            pthread_mutex_unlock(&from->mutex);
            pthread_mutex_unlock(&m->rewire_mutex);
            return refuse("out of memory for a port");
        }
        port_t **link = &from->ports;
        while (*link)
            link = &(*link)->next;
        *link = fresh;
        from->n_ports++;
    }
    port_t *p = station_port(from, port);
    destination_t *d = calloc(1, sizeof *d);
    if (!d) {
        pthread_mutex_unlock(&from->mutex);
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("out of memory for a destination");
    }
    d->station = to_station;
    d->slot = to_slot;
    d->next = NULL;
    destination_t **link = &p->destinations;
    while (*link)
        link = &(*link)->next;
    *link = d;
    pthread_mutex_unlock(&from->mutex);

    pthread_mutex_unlock(&m->rewire_mutex);
    return 0;
}
/* }}} */

/* {{{ map_rewire_disconnect() */
int map_rewire_disconnect(map_t *m, int from_station, int port,
                          int to_station, int to_slot)
{
    pthread_mutex_lock(&m->rewire_mutex);
    if (from_station < 0 || from_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return refuse("a station index outside the table");
    }
    station_t *from = &m->stations[from_station];

    pthread_mutex_lock(&from->mutex);
    port_t *p = station_port(from, port);
    destination_t *removed = NULL;
    if (p) {
        destination_t **link = &p->destinations;
        while (*link) {
            if ((*link)->station == to_station && (*link)->slot == to_slot) {
                removed = *link;
                *link = removed->next;
                break;
            }
            link = &(*link)->next;
        }
    }
    pthread_mutex_unlock(&from->mutex);
    pthread_mutex_unlock(&m->rewire_mutex);

    if (!removed)
        return refuse("no such wire to remove");

    /* Freed only after both locks are gone: delivery snapshots the
     * list under the station mutex, so nothing can still hold this
     * node. A value already snapshotted before the removal will be
     * delivered down the old wire — indistinguishable from having
     * been delivered a moment earlier, which is fine (issue 704). */
    free(removed);
    return 0;
}
/* }}} */

