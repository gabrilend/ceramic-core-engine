/*
 * 050-observe.c — the engine, saying out loud what it already knew.
 *
 * What this is: the reporting half of phase 7 (issues 701, 702).
 * Growth counts and high-water marks have been kept since phase 2;
 * run counts ride the delivery path; timing, when compiled in, comes
 * from the shims and the delivery path. This file only reads and
 * formats — nothing here decides anything.
 *
 * How it does it, in general terms: walks the station table under no
 * lock but the slots' own for depths (stale-by-a-moment numbers are
 * the nature of observing a live machine), sorts through a dispatch
 * table of orderings, and optionally emits on a timer from a small
 * thread that is not a worker and pushes nothing, so termination
 * stays sound.
 */
#include "049-observe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Growth past this many doublings at shutdown is shouted, per the
 * project's rule that a warning is an error nobody has decided
 * about yet. Reported by the diagnostics, not documented as a
 * constant anywhere else — this is where the number lives. */
#define GROWTH_SHOUT_THRESHOLD 4

/* {{{ station_label() */
static const char *station_label(map_t *m, int i, char *fallback, size_t n)
{
    if (m->station_names && m->station_names[i])
        return m->station_names[i];
    snprintf(fallback, n, "station %d", i);
    return fallback;
}
/* }}} */

/* {{{ map_report_buffers() */
void map_report_buffers(map_t *m, FILE *out)
{
    fprintf(out, "buffers:\n");
    int spoke = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        for (int j = 0; j < s->n_slots; j++) {
            slot_t *sl = &s->slots[j];
            if (sl->kind != SLOT_RING)
                continue;
            if (sl->growths == 0 && sl->high_water <= 1)
                continue;
            char fallback[32];
            fprintf(out,
                    "  %s.%d: grew %d time%s to %d cells, high water %d\n",
                    station_label(m, i, fallback, sizeof fallback), j,
                    sl->growths, sl->growths == 1 ? "" : "s",
                    sl->capacity, sl->high_water);
            spoke = 1;
        }
    }
    if (!spoke)
        fprintf(out, "  every slot stayed shallow; no imbalance to report\n");

    if (m->pool) {
        int capacity, high_water, growths;
        pool_queue_stats(m->pool, &capacity, &high_water, &growths);
        fprintf(out,
                "  the task ring: grew %d time%s to %d entries, high water %d\n"
                "  (slot piles mean uneven inputs; ring piles mean consumers\n"
                "   slower than producers — two different diagnoses)\n",
                growths, growths == 1 ? "" : "s", capacity, high_water);
    }
}
/* }}} */

/* {{{ the three orderings — a dispatch table of comparators */
static map_t *sorting_map;   /* qsort has no context argument */

/* Time attributable to a station's own work. This used to add the
 * gather time charged to it as a puller, because a station that
 * pulled paid for its upstream's run on its own thread and hiding
 * that would have mis-ranked it (issue 702). Nothing pulls now
 * (issue 210), so the box's own time is the whole of it. */
static long station_time(const station_t *s)
{
    return s->box_ns;
}

static int by_time(const void *a, const void *b)
{
    const station_t *sa = &sorting_map->stations[*(const int *)a];
    const station_t *sb = &sorting_map->stations[*(const int *)b];
    return (station_time(sb) > station_time(sa))
         - (station_time(sb) < station_time(sa));
}

static int by_contention(const void *a, const void *b)
{
    const station_t *sa = &sorting_map->stations[*(const int *)a];
    const station_t *sb = &sorting_map->stations[*(const int *)b];
    return (sb->mutex_wait_ns > sa->mutex_wait_ns)
         - (sb->mutex_wait_ns < sa->mutex_wait_ns);
}

static int by_count(const void *a, const void *b)
{
    const station_t *sa = &sorting_map->stations[*(const int *)a];
    const station_t *sb = &sorting_map->stations[*(const int *)b];
    return (sb->runs > sa->runs) - (sb->runs < sa->runs);
}

static int (*const orderings[REPORT_ORDER_COUNT])(const void *, const void *) = {
    [REPORT_BY_TIME]       = by_time,
    [REPORT_BY_CONTENTION] = by_contention,
    [REPORT_BY_COUNT]      = by_count,
};
/* }}} */

/* {{{ map_report_stations() */
void map_report_stations(map_t *m, FILE *out, int order)
{
    if (order < 0 || order >= REPORT_ORDER_COUNT) {
        fprintf(stderr, "observe: no such report ordering\n");
        abort();
    }

    static const char *const order_names[REPORT_ORDER_COUNT] = {
        "by time inside boxes", "by mutex contention", "by run count",
    };
    fprintf(out, "stations, %s:\n", order_names[order]);

    int indices[m->n_stations > 0 ? m->n_stations : 1];
    for (int i = 0; i < m->n_stations; i++)
        indices[i] = i;
    sorting_map = m;
    qsort(indices, (size_t)m->n_stations, sizeof indices[0], orderings[order]);

    for (int rank = 0; rank < m->n_stations; rank++) {
        int i = indices[rank];
        station_t *s = &m->stations[i];
        char fallback[32];
        fprintf(out, "  %-12s runs %-7ld produced %-7ld",
                station_label(m, i, fallback, sizeof fallback),
                (long)s->runs, (long)s->produced);
#ifdef SORA_STATS
        fprintf(out, " box %8.2fms  waited %8.2fms",
                s->box_ns / 1e6, s->mutex_wait_ns / 1e6);
#endif
        fprintf(out, "\n");
    }
#ifndef SORA_STATS
    fprintf(out, "  (times compiled out; build with -DSORA_STATS to see them)\n");
#endif
}
/* }}} */

/* {{{ sora_stats_box_time() */
/* Called by every generated shim when SORA_STATS is compiled in;
 * reaches the active map the same way a box's statics write does. */
void sora_stats_box_time(int32_t station, long ns)
{
    map_t *m = sora_active_map;
    if (!m || station < 0 || station >= m->n_stations)
        return;
    m->stations[station].box_ns += ns;
}
/* }}} */

/* {{{ the observer thread */
static void *observer_main(void *arg)
{
    map_t *m = arg;
    while (__atomic_load_n(&m->observer_running, __ATOMIC_ACQUIRE)) {
        FILE *out = fopen(m->observer_path, "a");
        if (out) {
            fprintf(out, "--- observation ---\n");
            map_report_buffers(m, out);
            map_report_stations(m, out, REPORT_BY_COUNT);
            fclose(out);
        }
        usleep((useconds_t)m->observer_interval_ms * 1000);
    }
    return NULL;
}

void map_observe_start(map_t *m, const char *path, int interval_ms)
{
    if (interval_ms <= 0) {
        /* Refuse rather than default: an engine writing diagnostics
         * nobody reads is a background thread doing nothing useful
         * (issue 701). Asking for zero means you did not want it. */
        fprintf(stderr, "observe: a non-positive interval — if you do not "
                        "want observation, do not start it\n");
        abort();
    }
    if (m->observer_running) {
        fprintf(stderr, "observe: already observing\n");
        abort();
    }
    m->observer_path = strdup(path);
    m->observer_interval_ms = interval_ms;
    m->observer_running = 1;
    pthread_create(&m->observer, NULL, observer_main, m);
}

void map_observe_stop(map_t *m)
{
    if (!m->observer_running)
        return;
    __atomic_store_n(&m->observer_running, 0, __ATOMIC_RELEASE);
    pthread_join(m->observer, NULL);
    free(m->observer_path);
    m->observer_path = NULL;
}
/* }}} */

/* {{{ map_report_shutdown() */
/*
 * The loud parting word (issue 701): any slot that grew past the
 * threshold gets named at teardown, because a map that works while
 * one buffer quietly absorbs a mismatch forever is a map with a
 * design problem nothing else will surface.
 */
void map_report_shutdown(map_t *m)
{
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        for (int j = 0; j < s->n_slots; j++) {
            slot_t *sl = &s->slots[j];
            if (sl->kind == SLOT_RING && sl->growths >= GROWTH_SHOUT_THRESHOLD) {
                char fallback[32];
                fprintf(stderr,
                        "observe: %s.%d grew %d times (to %d cells, high water "
                        "%d) — a producer outran a sibling input the whole "
                        "run; memory absorbed it, and someone should decide\n",
                        station_label(m, i, fallback, sizeof fallback), j,
                        sl->growths, sl->capacity, sl->high_water);
            }
        }
    }
}
/* }}} */
