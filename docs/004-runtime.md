# SoraMech — Runtime

This doc covers what happens when you run a map: how the runner
loads the graph, fires boxes, threads values along wires, writes
the JSONL transcript, and tears down. Plus the compile pipeline
and the reference-counted-artifact system that lets long-running
processes hold onto a specific build of a map across rebuilds.

## Running a map

```bash
./soramech-pool <map-dir>
```

The runner reads `<map-dir>/meta.json` + `<map-dir>/boxes/*.json`,
builds the in-memory graph, attaches the slot store (one slot
per input port), starts a worker pool (one worker per CPU by
default; override with `SORAMECH_WORKERS=N`), and submits the
entry boxes as initial tasks. Workers pick tasks off the pool
queue, run the box's spec function (Lua, C, or Bash), and push
the output along the producer's outgoing connections.

When all consumers are satisfied and no tasks remain, the run
hits quiescence and the runner exits. The stderr summary prints
each box's most recent output.

```
soramech-pool: 'hello' — 2 box(es), entry seed, 14 worker(s)
soramech-pool: run log → /tmp/soramech-last-run.jsonl
soramech-pool: run complete (2 task(s) in 1245 µs).
soramech-pool: outputs:
  seed → world
  greet → Hello, world!
```

## The JSONL transcript

Every run writes a JSON-Lines transcript to
`<map-dir>/tmp/last-run.jsonl` (or
`/tmp/soramech-last-run.jsonl` when the map directory has no
`tmp/` symlink). One line per event, append-only as the run
proceeds. The file is the authoritative record of what happened
— the stderr summary is a quick glance; the transcript is the
audit trail.

Default events (always emitted):

| Event           | When |
|-----------------|------|
| `run_start`     | runner starts |
| `task_submit`   | a box gets queued to fire |
| `task_start`    | a worker picks it up |
| `task_end`      | the box's function returns; carries duration_us, output_size |
| `run_end`       | every task drained, no more queued |

For runtime graph mutations (issue 319 self-construction):

| Event         | When |
|---------------|------|
| `box_create`  | `create_box` succeeds; records the new box's id + kind + lang + ref + fn |
| `wire_add`    | `connect` succeeds; records the from→to wire |
| `slot_alloc`  | a runtime-created box allocates an input slot |

Opt-in verbosity, via env vars:

- `SORAMECH_LOG_VALUES=1` — adds `task_input` / `task_output`
  events with the byte payloads (truncated to 4 KB per record).
  Useful for inspecting what flowed where.
- `SORAMECH_LOG_SLOTS=1` — adds the startup slot-layout
  enumeration AND a per-push event (`push`) for every
  dispatch-side wire push. The `push` event's `result` field
  carries either `"ok"` or a short skip reason
  (`"input-slots-null"`, `"to-input-out-of-range"`, etc.) — the
  load-bearing diagnostic when a box fires but the value
  doesn't seem to reach its consumer.

Sample (a runtime-create run with `SORAMECH_LOG_SLOTS=1`):

```jsonl
{"event":"slot_alloc","ts":...,"slot_id":0,"cell_capacity":4,...,"box":"trigger","port":"input"}
{"event":"run_start","ts":...,"map":"runtime-planner","n_workers":14}
{"event":"task_submit","ts":...,"task_id":0,"box_id":"trigger","worker_idx":-1}
{"event":"task_start","ts":...,"task_id":0,"worker_idx":3}
{"event":"slot_alloc","ts":...,"slot_id":1,"box":"auto_00000000","port":"x"}
{"event":"box_create","ts":...,"box_id":"auto_00000000","kind":"call","lang":"lua",...}
{"event":"wire_add","ts":...,"from_box":"trigger","to_box":"auto_00000000","to_input":"x"}
{"event":"push","ts":...,"from_box":"trigger","to_box":"auto_00000000","slot_id":1,"result":"ok"}
{"event":"task_end","ts":...,"task_id":0,"duration_us":418,"output_size":46}
{"event":"task_start","ts":...,"task_id":1,"worker_idx":11}
{"event":"task_end","ts":...,"task_id":1,"duration_us":78,"output_size":64}
{"event":"run_end","ts":...,"duration_us":3820,"n_tasks":2}
```

The transcript is line-oriented and JSON-shaped, so any tool
that handles JSON Lines (jq, log aggregators, your own parser)
ingests it directly. The line-per-event shape also means a
crash mid-run leaves a recoverable file — the writer just stops
appending; existing lines are intact.

## The compile pipeline

`scripts/soramech-compile.sh` bundles a map into a portable
artifact:

```bash
./scripts/soramech-compile.sh <map-dir>
```

The artifact lives at `<map-dir>/compiled/` and contains:

- `meta.json`, `boxes/`, `src/`, `data/` — verbatim copies of
  the map source
- `langs/<lang>/spec.so` — the language plugins the map needs
- `bin/<box>.so` — pre-compiled C boxes (informational; the
  runner still lazy-compiles from `src/` if a `.so` is missing)
- `pool-runner` — a copy of `soramech-pool` (a real copy, not a
  symlink, so the binary discovers its sibling `langs/` via
  `/proc/self/exe`)
- `manifest.json` — compile stats and provenance

Run the compiled artifact like the source map:

```bash
./<map-dir>/compiled/pool-runner <map-dir>/compiled
```

The artifact is self-contained — copy or symlink the directory
anywhere on the same machine and it runs without the source
tree, the project's `langs/`, or the project's build environment.

## Reference-counted artifacts

A compiled directory carries its own reference log (`.refs`).
Other programs that hold onto a build of a map across time
(long-running runners, sibling tools that opened the artifact
yesterday) **acquire** a reference on startup and **release** on
shutdown. The compile pipeline checks the log before rebuilding:

- If `compiled/` doesn't exist → build there fresh.
- If `compiled/` exists with zero live references → wipe and
  rebuild in place (the destructive case).
- If `compiled/` exists with **one or more** live references →
  fork to the next `compiled.N` sibling. The original stays
  intact for its holders.

Each fork's manifest carries a `forked_from` field naming its
parent generation, so the lineage is recoverable.

Helper:

```bash
# acquire a reference (prints the assigned id to stdout)
./scripts/soramech-ref.sh acquire <compiled-dir>

# release when you're done
./scripts/soramech-ref.sh release <compiled-dir> --id <id>

# count live references
./scripts/soramech-ref.sh count <compiled-dir>

# inspect — JSON array with per-entry validity
./scripts/soramech-ref.sh list <compiled-dir>

# garbage-collect dead entries (stale PIDs, missing marker files)
./scripts/soramech-ref.sh reap <compiled-dir>
```

The back pointer is PID + `/proc/<pid>/stat` field 22 (start
time in clock ticks). PID alone isn't enough — PIDs recycle —
so the start-time match is what distinguishes "the original
holder is still alive" from "a different process has the same
PID now." An optional marker file path supplies a second
liveness signal.

The reference count is **expected to be unreliable** by design:
a crashing holder doesn't get to release. The back pointer is
what lets `list` and `reap` independently verify whether any
given entry is still meaningful.

## Parallelism

The runner's thread pool fires independent boxes on different
workers. Two boxes whose inputs both become ready at the same
moment will run concurrently — there's no global lock on the
graph.

Same-box concurrent fires are gated. A box that has been
spawned but not yet completed won't get spawned again until it
finishes (single-spawn invariant). The exception is iterator
routing and any box downstream of an iterator: those boxes are
marked multi-spawn, and the dispatch re-fires them while their
input slots have queued values to drain.

Cross-language wires are atomic at the value level. The slot
store carries values as native bytes (when both sides share a
language) or JSON (when the sides differ); the dispatch picks
the right format per-wire. Each consumer sees a consistent
snapshot of the value, never a partial one.

## Quiescence

The runner exits when the work queue is empty AND every
in-flight task has completed AND no boxes have queued values
waiting to fire. Long-running maps (e.g., the timer-box design
in issue 251) prevent quiescence by re-arming after each fire —
the runner stays up until you Ctrl-C.

## Running tests

- `make test` — full ship-it suite: builds + runs the C unit
  tests, then runs the integration fixtures.
- `make quicktest` — cheap iteration loop: runs only the
  already-built C unit binaries. Add `ARGS=009` to filter to
  binaries whose names start with `009`.

See [`docs/006-test-coverage-map.md`](006-test-coverage-map.md)
for the test inventory.
