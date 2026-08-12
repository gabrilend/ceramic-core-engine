/*
 * 049-observe.h — seeing inside a running map.
 *
 * What this is: phase 7's surface. None of it is required for
 * correctness; all of it is required for confidence. Buffer growth
 * reported instead of silently absorbed, per-station statistics that
 * locate a bottleneck instead of letting it be guessed at, a dump of
 * the loaded map that reads back as a map, and rewiring while it
 * runs.
 *
 * How it does it, in general terms: the counters were left in place
 * by the phases that built the structures (growth counts in phase 2,
 * high water alongside); this phase reads them out loud. Timing is
 * heavier than counting, so it compiles out entirely unless
 * SORA_STATS is defined — a measurement apparatus whose cost is
 * unmeasured is a rumour, and one that cannot be removed is a tax.
 */
#ifndef SORA_OBSERVE_H
#define SORA_OBSERVE_H

#include <stdio.h>

#include "018-station.h"

/* ------------------------------------------------------------------ */
/* Reporting (issues 701, 702).                                       */
/* ------------------------------------------------------------------ */

/* {{{ map_report_buffers() — issue 701 */
/*
 * Every slot's growth story: doublings, current capacity, high-water
 * occupancy — plus the pool ring's own, because the two piles form
 * in different places (a lesson from phase 2). High water matters
 * more than capacity: a thousand-cell buffer that held two values
 * had one bad moment; one that held nine hundred is a bottleneck.
 */
void map_report_buffers(map_t *m, FILE *out);
/* }}} */

/* {{{ map_report_stations() — issue 702 */
/*
 * Per station: runs, tasks produced for others, and — when compiled
 * with SORA_STATS — time inside the box and time spent waiting on the
 * station's mutex. Three orderings of the same data, because the
 * interesting station is a different one under each.
 *
 * A third time was reported until issue 210: gather time, charged to
 * the station that pulled rather than to the one that ran, because
 * that is where a pulled value was actually paid for. Nothing pulls
 * now, so a station's own box time is the whole of its cost.
 */
enum report_order {
    REPORT_BY_TIME = 0,
    REPORT_BY_CONTENTION,
    REPORT_BY_COUNT,
    REPORT_ORDER_COUNT
};
void map_report_stations(map_t *m, FILE *out, int order);
/* }}} */

/* {{{ map_observe_start() / map_observe_stop() — issue 701 */
/*
 * Periodic emission of both reports to a file, from a small thread
 * that is not a worker and pushes nothing (so termination stays
 * sound). An interval of zero refuses to start: diagnostics nobody
 * reads are a background thread doing nothing useful.
 */
void map_observe_start(map_t *m, const char *path, int interval_ms);
void map_observe_stop(map_t *m);
/* }}} */

/* {{{ map_report_shutdown() — issue 701's loud parting word */
/* Called by map_destroy: any slot grown past the shout threshold is
 * named on stderr, so a quietly-absorbing map gets decided about. */
void map_report_shutdown(map_t *m);
/* }}} */

/* {{{ sora_stats hooks — filled by generated shims and the engine */
/*
 * The shims emitted by the generator call the first of these around
 * every box run when SORA_STATS is defined; the engine calls the
 * rest from the delivery path. All of them are cheap atomics on the
 * station's own counters; all of the *timing* callers compile out
 * without the define.
 */
void sora_stats_box_time(int32_t station, long ns);
/* }}} */

/* ------------------------------------------------------------------ */
/* The dump (issue 703). Lives in 050-dump.c.                         */
/* ------------------------------------------------------------------ */

/* {{{ map_dump() */
/*
 * Write the in-memory station table back out in the map file format,
 * derived facts as comments beside the lines that parse. Dumped from
 * the table, never from any remembered text: the table is what
 * exists, and any disagreement with the original file is a loader
 * bug nothing else would catch. Requires a map loaded from a file
 * (names retained); refuses a hand-built map by name.
 */
void map_dump(map_t *m, FILE *out);
/* }}} */

/* ------------------------------------------------------------------ */
/* Rewiring (issue 704). Lives in 051-rewire.c.                       */
/* ------------------------------------------------------------------ */

/* {{{ map_rewire_connect() / map_rewire_disconnect() */
/*
 * Change the shape while it runs. Every load-time rule applies per
 * edge — type compatibility, buffer-slot destinations, port limits —
 * and the check and the change happen under one lock, because two
 * threads each adding an individually legal edge can produce an
 * illegal pair.
 *
 * A third operation lived here: repointing a gather wire, which
 * carried the cycle walk and was the sharpest example of why the
 * check and the change must share a lock. It went with the pull path
 * (issue 210). The joint-illegality argument did not go with it — it
 * still governs these two, and the port limits are where it now
 * shows.
 *
 * Refusal behaviour, decided rather than defaulted: these return -1
 * with the reason on stderr instead of stopping the program. A
 * loader that dies serves its author; a running engine that dies
 * because a control surface sent one bad instruction takes the whole
 * plant down with it. The debt is that a caller can ignore the -1 —
 * the first-pass report weighs the trade, and issue 212 overturns it.
 */
int map_rewire_connect(map_t *m, int from_station, int port,
                       int to_station, int to_slot);
int map_rewire_disconnect(map_t *m, int from_station, int port,
                          int to_station, int to_slot);
/* }}} */

#endif
