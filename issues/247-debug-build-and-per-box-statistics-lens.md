# 247 — Debug build alternative and per-box statistics lens

## Status
open

## Current behavior

Compiling a map produces exactly one kind of artifact: the release
build that the compile pipeline (`scripts/soramech-compile.sh`)
writes into the map's `compiled/` directory. There is no debug mode
and no way to choose a build mode; the editor's compile toolbar
button today only pre-flights input bindings and reports that the
pipeline isn't wired to it yet. Every run appends an event
transcript to the map's `tmp/last-run.jsonl`, and the task-end
events already carry per-fire duration and output size; richer
payload and slot events exist behind opt-in environment variables,
but nothing in the editor turns them on. On the canvas, boxes are
colored by kind only — seeing which boxes are slow or busy means
leaving the editor and reading the transcript by hand.

## Intended behavior

The compile button gains a small dropdown of build modes: release
stays the default, a debug build is the first alternative, and the
chosen mode persists per map. The debug build switches on the
runner's opt-in event toggles for the next run. After a debug run,
the editor aggregates the transcript per box (run count, mean /
p50 / p95 duration, output-size mean and variance), caches the
result beside the transcript, and offers a lens panel: the user
picks one statistic and the canvas tints every box along a
normalized gradient, with a legend showing the run's actual min
and max. Boxes that never fired get a distinct no-data styling,
and picking "(none)" restores the default box styling.

## Concept

The Compile button currently produces one artifact: a release
build of the map. Box authors who want to know which boxes are
slow, which fire often, or which produce variably-sized outputs
have no editor-side way to see it — the run-output JSONL exists
(`tmp/last-run.jsonl`) but reading it requires leaving the
editor.

This issue adds two things that work together:

1. **Compile-button alternatives.** A small dropdown (right-click
   the button, or a tiny arrow next to it) opens a menu of build
   modes. The default is the existing release build. The first
   alternative is a **debug build** that turns on per-box
   instrumentation. Other modes can land here later (strict
   build, profile build, etc.) — the dropdown is the entry point.
2. **Per-box statistics lens.** After a debug-build run, the
   editor reads `tmp/last-run.jsonl`, aggregates per-box stats
   (run count, mean / p50 / p95 duration, mean output size,
   output-size variance), and colors each box on the canvas
   according to a user-chosen filter. The filter is a small
   panel in the editor — "color by: run count / speed /
   consistency / cumulative time" — and the canvas re-tints as
   the user picks.

## What "debug build" turns on

Most of the data already exists in `tmp/last-run.jsonl`. The
runner's event types include `task_end` with `duration_us` and
`output_size` already (`docs/004-runtime.md`, "The JSONL
transcript"). What the debug build does is **enable the optional
event toggles**:

- `SORAMECH_LOG_VALUES=1` — input/output payloads per task (truncated
  at 4 KB). Useful for the consistency lens.
- `SORAMECH_LOG_SLOTS=1` — slot allocator events. Useful for the
  memory lens.

In the future the debug build can layer on actual instrumentation
(per-spec-invoke timing, custom-shim timing, etc.). The first cut
just toggles the env vars the existing runner already supports.

## Lenses (color filters)

Each lens maps `box_id → some_number → color`. The editor
normalizes the number across all boxes in the run, then maps to a
gradient.

| Lens | Per-box value | Gradient (low → high) |
|------|---------------|------------------------|
| **Run count** | how many times the box's task fired during the run | cool → warm |
| **Mean duration** | mean `duration_us` over all firings | green → red |
| **Cumulative time** | sum of `duration_us` over all firings | green → red |
| **p95 duration** | 95th-percentile single-firing time | green → red |
| **Output size variance** | stdev / mean of `output_size`; high variance = "unpredictable size" | calm → loud |
| **Output size mean** | mean output bytes | small → large |

The lens panel is a dropdown of these options plus a small
legend (gradient strip + min/max values for the current run).

Selecting "(none)" returns boxes to their default styling.

## Where the data lives

```
maps/<name>/tmp/
    last-run.jsonl         ← runner's event stream (existing)
    last-run-stats.json    ← editor-aggregated per-box stats (new)
```

The editor reads `last-run.jsonl`, aggregates by `box_id`, and
caches the result in `last-run-stats.json` so subsequent lens
switches don't re-parse the JSONL. The cache file is
regenerated each time the user clicks Compile-debug-and-run
(or just Run, if a debug build is loaded).

The aggregation is dumb: walk events, accumulate per-box
counters and arrays of durations / sizes, compute the summary
statistics. Runs in the editor's HTTP backend (Lua), since the
file is already accessible there.

## UI: Compile button dropdown

The Compile button grows a small right-arrow affordance (▾) on
its right edge. Clicking the button proper still runs the
default release build (unchanged behaviour). Clicking the arrow
opens a small menu:

- **Release build** (current default)
- **Debug build** (this issue)
- **Strict build** (`make STRICT=1` equivalent — added later)
- **Profile build** (future, with detailed timing)

The chosen mode persists per-map (saved in the map's local UI
state) so re-clicking the button main face reruns the last mode.
The button's label / icon changes subtly to indicate which mode
is active.

## UI: Lens panel

A small panel in the editor sidebar (or a popover near the
canvas-zoom indicator):

```
┌─ Box statistics ─────────────────┐
│ Color by:                         │
│   ( ) None                        │
│   (•) Run count                   │
│   ( ) Mean duration               │
│   ( ) Cumulative time             │
│   ( ) p95 duration                │
│   ( ) Output size variance        │
│   ( ) Output size mean            │
│                                   │
│ Legend: [▮▮▮▮▮▮]                  │
│         1            18,300        │
└───────────────────────────────────┘
```

The legend's min/max reflect the current run's actual values.
Boxes that didn't fire in the run get a "no data" styling
(dimmed or outlined-only).

## Relevant files

- `assets/index.html` — compile button + lens panel UI.
- `assets/js/005-app.js` — compile-button click handlers; mode
  state.
- `assets/js/002-boxes.js` — box rendering; lens color override.
- `src/006-server-main.lua` — aggregation route, reads
  `last-run.jsonl`, produces `last-run-stats.json`.
- `Makefile` / `scripts/soramech-compile.sh` — debug build mode
  (sets the env vars; possibly compiles with `-O0 -g`).
- `docs/004-runtime.md` — event format reference, in "The JSONL
  transcript" (was cited as `docs/001-architecture.md:638-666`, a
  file that no longer exists; line-numbered citations into docs go
  stale on the next edit, so this one names the section instead).

## Suggested implementation steps

The original planning sequence below is the concrete step list.

### Suggested implementation sequence

1. **Compile-button dropdown UI.** Arrow affordance, menu, mode
   state persisted per-map.
2. **Debug build mode** in the compile script. Sets
   `SORAMECH_LOG_VALUES=1` and `SORAMECH_LOG_SLOTS=1` for the
   subsequent run. Optionally adds `-O0 -g` to per-box compiles
   so stack traces are meaningful.
3. **Aggregation route** in the HTTP backend. Reads
   `last-run.jsonl`, computes per-box stats, writes
   `last-run-stats.json`.
4. **Lens panel** in the editor. Reads
   `last-run-stats.json`, applies the selected color filter to
   the boxes on the canvas.
5. **Legend** with current-run min/max.
6. **No-data styling** for boxes that didn't fire.

## Open questions

- **Where the lens panel docks**: sidebar vs. floating popover.
  Whatever fits the existing editor layout.
- **Color gradient choice**: a perceptually-uniform palette
  (viridis, magma) reads better than a linear hue gradient.
  Pick once; reuse across lenses.
- **Live updates during a run**: the first cut reads stats after
  the run finishes. A streaming version (tail the JSONL, update
  the lens as boxes fire) is a follow-on if useful.
- **Lens combination**: only one lens active at a time in the
  first cut. Layered lenses (size of dot + color of dot from two
  filters) are a follow-on.
- **History / comparisons**: comparing two runs side by side (was
  this box slower before my last edit?) is interesting but out
  of scope here; would be a separate ticket.
