# 013-jsonl-events.c — public surface

Phase 3 run-log event writer. Emits one JSON object per event,
one event per line, to a writable file.

## Lifecycle

- `jsonl_writer_t *jsonl_writer_open(const char *path)` — opens
  `path` for writing (truncates). NULL on failure.
- `void jsonl_writer_close(jsonl_writer_t *w)` — fflush+fclose,
  free.

## Event emitters

Each returns 0 on success, -1 on write or encode failure.
All emitters are thread-safe via a per-writer mutex.

Always emitted:

- `jsonl_emit_run_start  (w, ts, map, n_workers)`
- `jsonl_emit_task_submit(w, ts, task_id, box_id, worker_idx)`
- `jsonl_emit_task_start (w, ts, task_id, worker_idx)`
- `jsonl_emit_task_end   (w, ts, task_id, worker_idx, duration_us, output_size)`
- `jsonl_emit_run_end    (w, ts, duration_us, n_tasks)`

Emitted when the runner's verbosity gates are on:

- `jsonl_emit_task_input (w, ts, ...)` — payload bytes in
- `jsonl_emit_task_output(w, ts, ...)` — payload bytes out
  (both under `SORAMECH_LOG_VALUES=1`)
- `jsonl_emit_slot_alloc (w, ts, ...)` — startup slot layout
- `jsonl_emit_push       (w, ts, ...)` — one per wire push,
  carrying `"ok"` or a skip reason
  (both under `SORAMECH_LOG_SLOTS=1`)

Timestamps are Unix epoch seconds as doubles.

## Who calls these

Not the workers. `014-event-queue.c` owns a lock-free bounded MPSC
ring; workers publish records into it and a single dedicated
writer thread drains the ring and calls the emitters above. So the
per-writer mutex here is uncontended in practice — one thread
touches it — and no worker ever blocks on `fwrite`.

Call these directly only from single-threaded contexts (tests,
tools). Anything inside a run goes through the event queue.

## What's NOT here (deferred)

- **Reading the transcript back.** These are write-only emitters;
  parsing a transcript is `scripts/normalise-jsonl.lua` and the
  test harness's business.
- **Rotation or size capping.** A long-running map writes an
  unbounded file.

## Related

- Issue 311 — design.
- Issue 314 — bounded JSON writer this builds on.
- Issue 304 — dispatch produces task_start / task_end.
- Issue 301 — pool runner produces run_start / run_end.
