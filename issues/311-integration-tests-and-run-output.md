# 311 — Integration tests & run output (`last-run.json`)

## Status
open

## Current behavior

Phase 2 writes a `last-run.json` after every map run, recording per-
box invocations, inputs, outputs, and any errors. The editor reads
it to display a run summary. Phase 2 testing is ad-hoc: a few maps
in `maps/` (`hello`, `branch-test`, `classify-demo`, `driver-test`)
that the developer runs by hand to verify the synchronous executor
still works.

Phase 3 changes the shape of the problem: many concurrent
invocations, possibly thousands per run when iterators are in play,
written from multiple worker threads. The phase 2 "build a single
table at end of run" approach does not survive that. And the
existing maps cover only a small slice of the new surface (no
queued inputs, no large iterator counts, no fan-in).

This issue covers two things that go together: the format and
writer for the run output, and the integration test plan that
exercises the phase 3 runtime end-to-end.

## Part A — Run output

### Decoupling from the editor

The editor (phase 2) is being scoped down to a pure map editor — it
does not know how to run maps and does not display run results.
`last-run.json` is therefore not a UI feed. It exists for:

- LLM consumption — agents reading the file to understand what a
  run did
- CLI-driven inspection — a future viewer utility (out of scope
  here) that pretty-prints the file
- Test harness consumption — the integration tests below diff the
  written file against expected output

Decoupling means the writer's design is driven by these consumers,
not by editor rendering needs.

### File format: JSON Lines

The phase 2 single-document format does not work when the runner is
streaming events from many threads. Use JSON Lines instead — one
JSON object per line, each line is one event, the file grows
monotonically as the run proceeds.

Event types:

```jsonl
{"event":"run_start", "ts":1234567890.123, "map":"maps/classify-demo", "n_workers":16}
{"event":"task_submit", "ts":..., "task_id":42, "box_id":"classify", "worker_idx":-1}
{"event":"task_start", "ts":..., "task_id":42, "worker_idx":3}
{"event":"task_end", "ts":..., "task_id":42, "worker_idx":3, "duration_us":127, "output_size":12}
{"event":"slot_alloc", "ts":..., "slot_id":5, "size":256, "n_cells":1}
{"event":"slot_free", "ts":..., "slot_id":5}
{"event":"run_end", "ts":..., "duration_us":1850, "n_tasks":7}
```

JSONL is append-only, which makes thread-safe writing tractable: a
single mutex around the write call serializes the lines, and each
line is self-contained so a partial line on crash is recoverable
(reader skips the broken trailing line).

The reader (LLM, viewer, test harness) parses one line at a time;
total file size doesn't have to fit in memory.

### What gets logged

Per-task: submission, start, end, duration, output size. Inputs
and outputs are NOT logged by default — for iterator boxes
producing thousands of invocations, logging every value bloats
the file unbounded. There is a verbosity switch:

- `SORAMECH_LOG_VALUES=1` — log inputs and outputs (truncated past
  some byte limit, e.g. 4 KB) per task. For debugging.
- Default: log task lifecycle but not values.

Slot allocator events (alloc, free, coalesce, grow) at a separate
verbosity level. Off by default; on under `SORAMECH_LOG_SLOTS=1`.
For diagnosing memory issues.

### Writer mechanism

A single dedicated writer thread inside the runner consumes events
from a multi-producer single-consumer ring buffer. Worker threads
push event records (small fixed-size structs) into the ring; the
writer pulls them, serializes to JSON, and writes lines to the
file. This avoids per-event mutex contention — the ring buffer is
the only shared state, and a well-written MPSC ring is lock-free
on the producer side.

Alternative considered: each worker writes its own log file, merge
at end. Simpler concurrency, but readers would have to merge by
timestamp — and any cross-event reasoning (was task A still
running when task B ended?) requires the merge anyway.

The writer thread is started after the pool is created and joined
before `pool_destroy`. It also handles `run_end` as the very last
event, including the final summary.

### File location

`<map-dir>/last-run.jsonl` (note: `.jsonl` not `.json` — the
extension reflects the format change). Overwritten on every run.

A rotated archive (`last-run.jsonl.1`, `.2`, …) is out of scope.
Users who want history copy the file before re-running.

## Part B — Integration tests

### Scope

End-to-end tests of the pool runner against real map directories.
Each test is:

1. A map directory under `tests/maps/<name>/`
2. An expected `last-run.jsonl` (or a comparison spec) under
   `tests/expected/<name>.jsonl`
3. A test driver script that runs the map and diffs

The test driver is `scripts/run-tests.sh`. It iterates
`tests/maps/`, runs each, diffs against expected (with a normalizer
that drops timestamps, durations, worker indices — anything non-
deterministic), and reports pass/fail.

### Test maps

Coverage targets:

| Test map                  | What it exercises                                    |
|---------------------------|------------------------------------------------------|
| `hello`                   | Single Lua box, one input, one output                |
| `data-only`               | Data box → call box. No inputs, just stored values   |
| `fan-out-3`               | One output → three consumer boxes (peek, refcount)   |
| `fan-in-iterator`         | Three producers → iterator with queued input         |
| `comparator-routing`      | Comparator with all three branches (lt/eq/gt)        |
| `iterator-100`            | Iterator with 100 input values, round-robin to 3 paths |
| `multilang`               | One Lua box, one C box, one Bash box, all wired      |
| `large-output`            | Box producing a 1 MB string (variable-size heap)     |
| `cycle-detector-fail`     | Non-iterator cycle — load must abort                 |
| `missing-input-fail`      | Connection references a nonexistent box — load aborts|
| `box-error-fail`          | Box function returns nonzero — runner aborts         |

The `*-fail` tests verify error behavior — the runner is expected
to exit nonzero with a precise error message. The driver matches
exit code and stderr against expected.

### Test driver

```bash
# scripts/run-tests.sh
for test_dir in tests/maps/*/; do
    name=$(basename "${test_dir}")
    expected="tests/expected/${name}.jsonl"

    actual=$( "${SOURCE_DIR}/soramech-pool" "${test_dir}" )
    rc=$?

    if [ -f "${expected}.exit" ]; then
        # *-fail tests: assert exit code and stderr substring
        check_failure "${name}" "${rc}" "${test_dir}/last-run.jsonl"
    else
        # success tests: diff normalized last-run.jsonl
        check_success "${name}" "${test_dir}/last-run.jsonl" "${expected}"
    fi
done
```

`normalize_jsonl` strips timestamps, durations, worker indices,
slot ids, and task ids (all non-deterministic) and sorts events by
event type. The result should be deterministic across runs.

### Phase 3 demo map

A single map under `issues/completed/demos/phase-3/` that
demonstrates everything working together:

- Multiple entry-point data boxes
- Lua, C, and Bash boxes all wired together
- An iterator with a queued input
- A comparator routing into different downstream paths
- A large-output box (variable-size heap)
- Non-trivial graph topology (≥ 10 boxes)

Run with `./run.sh` from the demo directory. Prints a short summary
of what happened (parsing the `last-run.jsonl`) so the demo is
visible without a viewer utility.

This satisfies the standing project rule (CLAUDE.md) that each
phase produces a runnable demo in `issues/completed/demos/`.

### Test infrastructure constraints

- Tests must not depend on the editor or any HTTP server
- Tests must not depend on internet access
- Tests must run in under 60 seconds total on a developer machine
- Tests must be deterministic when normalized (any
  non-determinism is normalized out, not papered over with retries)

## Open questions

- Whether to log inputs/outputs by default. Useful for development,
  noisy for production. Current decision: opt-in via env var,
  default off.
- Slot-event logging: also opt-in. Useful for diagnosing memory
  issues. Off by default.
- File format: JSON Lines is the choice. An alternative was a
  protobuf or msgpack binary log, which is faster to write but
  harder to inspect by hand. JSONL wins on inspectability.
- Whether to keep timestamps as Unix epoch floats or ISO 8601
  strings. Epoch floats are smaller and easier to diff. Stick with
  floats.

## Suggested implementation sequence

### Run output (Part A)
1. Define the event struct and JSONL line format. Start with
   `run_start`, `task_start`, `task_end`, `run_end`.
2. Implement the MPSC ring buffer for events.
3. Implement the writer thread: pulls from ring, serializes,
   appends a line to the output file.
4. Wire event emission into the pool runner: `run_start` at
   startup, `task_start` and `task_end` in the dispatch action,
   `run_end` after quiescence.
5. Add the verbosity env vars and the optional event types.

### Tests (Part B)
6. Create `tests/maps/` and `tests/expected/` directory structure.
7. Write the `hello` test as the first end-to-end smoke check.
8. Write the test driver `scripts/run-tests.sh` and the JSONL
   normalizer.
9. Add tests one at a time as each runtime feature lands. The
   `*-fail` tests can land before their corresponding successful
   tests if the failure path is implemented first.
10. Build the phase 3 demo map under `issues/completed/demos/phase-3/`
    once the rest of phase 3 is implemented.

## Relevant files

- `scripts/run-tests.sh` — test driver (to be written)
- `tests/maps/`, `tests/expected/` — test fixtures (to be created)
- `issues/completed/demos/phase-3/` — phase 3 demo (to be built)
- `src/008-pool-runner.c` — emits run_start / run_end
- `src/dispatch.c` — emits task_start / task_end
- `src/slot-store.c` — emits slot_alloc / slot_free (verbose mode)
- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
- `issues/302-wire-value-slot-store.md` — slot events
- `issues/304-task-dispatch-layer.md` — task lifecycle events
- `issues/305-c-graph-loader.md` — load-time errors
- `issues/309-build-system.md` — `make test` invokes this
- `docs/004-ipc-and-threading.md` — blocking semantics relevant to
  test design
