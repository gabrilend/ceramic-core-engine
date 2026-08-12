# 049-observe.h — seeing inside, from outside

Phase 7's surface, implemented across the observe (050), dump (051),
and rewire (052) files.

## Reports

**map_report_buffers(map, stream)** — every slot that grew or held a
backlog: doublings, capacity, high water, named by station. Plus the
task ring's own story, with the reminder that slot piles and ring
piles are different diagnoses.

**map_report_stations(map, stream, order)** — runs and
tasks-produced per station (always on), plus box time and mutex wait
when built with `-DSORA_STATS` (clock reads compile out otherwise).
Orders: by time, by contention, by count — a dispatch table of
comparators.

**map_observe_start(map, path, interval ms) / map_observe_stop** —
periodic emission from a non-worker thread that pushes nothing.
A non-positive interval refuses: unwanted observation is refused,
not defaulted.

**map_report_shutdown(map)** — called by teardown; slots grown past
the shout threshold are named on stderr.

## The dump

**map_dump(map, stream)** — the live table in map format, derived
facts as comments (types, sizes, indices). Loaded maps
only (names required). Load → dump → load → dump is byte-identical;
statics render their load-time text, with runtime byte-writes noted
as not re-serialized.

## Rewiring

**map_rewire_connect / map_rewire_disconnect** —
change the shape while it runs. All load-time rules per edge, check
and mutation under one rewiring lock, list surgery additionally
under the owning station's mutex (delivery snapshots under the
same). A third operation repointed a gather wire and carried the
cycle walk; it went with the pull path. Refusals return -1 with the
reason on stderr — a running
engine is not killed by one bad instruction; the trade is weighed in
the first-pass report.
