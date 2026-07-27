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

### Which boxes start the run

The entry set is **derived from the graph's shape**, not declared.
At load the runner walks every box and takes the ones that could
not possibly be waiting on anything:

- The box is a `call` or `write` box. A `read` box never runs as
  a task at all, so it is never an entry box — no matter what
  `meta.json` says.
- Every non-optional input port on it has **zero non-read
  feeders**. Read-box predecessors don't disqualify a box,
  because a read box is pulled rather than awaited; a box fed
  only by read boxes can fire immediately. Optional ports are
  ignored entirely.

A call box with no inputs at all passes vacuously and qualifies.
Every box that clears both tests is submitted as an initial task,
so a map can have many entry boxes — the count is whatever the
wiring implies.

> **`meta.json`'s `entry_box_id` is being removed.** There is no
> single entry box and there never really was one — the field is
> mandatory in the schema, parsed by the loader, and consumed by
> nothing except the `entry <id>` word in the startup banner.
> It is also commonly pointed at a `read` box, the one kind that
> categorically cannot be an entry box, which makes that banner
> line actively misleading. Issue 206 (entry box designation)
> carries the removal: the schema stops requiring it, the loader
> stops parsing it, and the banner starts reporting the derived
> set. Maps that still carry the field will keep loading — it
> becomes ignored, not an error.

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
| `slot_alloc`    | a slot is allocated for an input port (startup-time enumeration) |

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

Sample (a small run with `SORAMECH_LOG_SLOTS=1`):

```jsonl
{"event":"slot_alloc","ts":...,"slot_id":0,"cell_capacity":4,...,"box":"trigger","port":"input"}
{"event":"run_start","ts":...,"map":"hello","n_workers":14}
{"event":"task_submit","ts":...,"task_id":0,"box_id":"trigger","worker_idx":-1}
{"event":"task_start","ts":...,"task_id":0,"worker_idx":3}
{"event":"push","ts":...,"from_box":"trigger","to_box":"echo","slot_id":1,"result":"ok"}
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

Same-box concurrent fires are **not** gated. There is no
single-spawn invariant and no multi-spawn marker: every box may
be firing on several workers at once, and a box that is still
running can be fired again the moment its inputs are ready
again. The runtime's whole guarantee is two sentences — each
fire owns the input values that were popped for it, and each
fire owns a unique return slot for its output. Nothing else
about a box's execution is serialised.

The cost lands on box authors, who write thread-safe box
functions: atomics for shared counters, no unsynchronised
static mutable state, or pure functions that share nothing at
all. See [`docs/005-writing-boxes.md`](005-writing-boxes.md)
for the per-language contract.

The gate used to exist, and why it went is worth recording.
Gating same-box re-entry means the hot box — the one whose
inputs refill fastest, the one that most wants to run on every
idle worker — is exactly the box the runtime refuses to
parallelise. The gate was then punched through for iterator
routing and everything downstream of it, which cost a load-time
forward walk to decide which boxes were exempt, a second slot
shape for the exempt ones, and a rule for what happens where
the two shapes meet. Deleting the distinction deletes the gate
and all three of its costs at once.

> **Runtime conformance.** The C runtime has not caught up to
> this ruling yet — `soramech-pool` still carries the CAS spawn
> guard and the iterator-seeded marker walk. Issues
> [304](../issues/304-task-dispatch-layer.md) and
> [305](../issues/305-c-graph-loader.md) are reopened to remove
> them.

An input port receives values through one of two **input
methods**, and the difference is whether the value is *consumed
on use* or *referenced on use*. The method is a property of the
port alone — with the box-level marker gone, nothing about the
box its port belongs to enters the decision. A consuming port
gives up one queued delivery per fire, which is what a wire-fed
port wants: each lap of a recursing network delivers fresh. A
referencing port reads its value in place, never spending it. A
typed-in constant whose port has no incoming wire is always
referenced: startup delivers it once and every fire re-reads it.
A port carrying both a constant and a wire consumes — there the
constant is only the seed, and the network re-feeds the port on
every revolution. Note the model has no "loops" in the
traditional sense: a map is a network of boxes that recurse
through themselves, iteratively re-processing data or memory
locations, and the input method on each port is what shapes how
values survive that recursion.

Cross-language wires are atomic at the value level. The slot
store carries values as native bytes (when both sides share a
language) or JSON (when the sides differ); the dispatch picks
the right format per-wire. Each consumer sees a consistent
snapshot of the value, never a partial one.

## Termination: circular vs. quiescent

A run ends one of two ways, and the **long-running way is the
primary one** — it's what the runtime is built around. A map
that loops back on itself stays up and serves indefinitely; a
straight-through pipeline drains and exits. Which fate a map
meets is decided entirely by whether its wiring forms a legal
cycle.

**Circular maps run until a quit signal.** Because a box fires
whenever its inputs arrive, a map can be wired to feed itself so
the graph never runs dry — each turn re-arms the next. This is
the intended shape for most maps: a game's frame tick driving
read → solve → render and back to the tick, an LLM loop feeding
its own next prompt, a server re-arming on each request. Two
constructs form a legal loop:

- **Iterator-routing boxes cut the cycle.** A cycle that passes
  *through* an iterator box is legal: the iterator re-fires as
  long as its input queue has values to drain and terminates
  cleanly when the queue empties. This is how a data-driven loop
  sustains itself — the iterator is the box the validator trusts
  not to deadlock.
- **A re-arming heartbeat drives a wall-clock loop.** The timer
  box (issue 251, currently a design draft — not yet built)
  fires, pushes a tick downstream, and schedules its own next
  fire at `now + rate_ms`. It's the first box whose schedule
  comes from the clock rather than from data arrival, and it
  keeps a loop turning at a fixed rate (a 60 Hz frame tick, a
  cron-like "every N minutes" pipeline) until a quit signal —
  Ctrl-C, or a `rate_ms` of 0 — stops it.

**Cycle validation happens at load, not at run.** The graph is
DFS-walked for back-edges before the first box ever fires. A
back-edge that does *not* pass through an iterator box is
rejected outright — `non-iterator cycle detected` — so a
deadlock-prone loop fails loud at load instead of hanging
mid-run. During the walk the validator skips an iterator's
outgoing edges, which is how it expresses "this edge doesn't
propagate cycle reachability."

**Batch maps hit quiescence and exit.** A map with no feedback
loop eventually drains: the work queue empties, every in-flight
task completes, and no box has queued values waiting to fire.
With nothing left to do, the runner exits on its own — the right
behavior for a one-shot pipeline (seed → transform → report).

Neither is a special case bolted onto the other; they are the
two natural fates of a dataflow graph.

## Running tests

- `make test` — full ship-it suite: builds + runs the C unit
  tests, then runs the integration fixtures.
- `make quicktest` — cheap iteration loop: runs only the
  already-built C unit binaries. Add `ARGS=009` to filter to
  binaries whose names start with `009`.

See [`docs/006-test-coverage-map.md`](006-test-coverage-map.md)
for the test inventory.
