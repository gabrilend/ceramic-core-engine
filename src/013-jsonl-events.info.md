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

- `jsonl_emit_run_start  (w, ts, map, n_workers)`
- `jsonl_emit_task_submit(w, ts, task_id, box_id, worker_idx)`
- `jsonl_emit_task_start (w, ts, task_id, worker_idx)`
- `jsonl_emit_task_end   (w, ts, task_id, worker_idx, duration_us, output_size)`
- `jsonl_emit_run_end    (w, ts, duration_us, n_tasks)`

Timestamps are Unix epoch seconds as doubles, matching the
architecture doc.

## What's NOT here (deferred)

- **Slot allocator events** (`slot_alloc` / `slot_free`) under
  `SORAMECH_LOG_SLOTS=1`. Adds when 302's allocator is
  instrumented.
- **Per-task input/output bytes** under `SORAMECH_LOG_VALUES=1`.
- **MPSC ring buffer + dedicated writer thread.** Current
  implementation locks a mutex around `fwrite`. The proper
  pipeline is many producers pushing fixed-size event records
  onto a lock-free ring while one writer thread drains and
  serializes. Lands once dispatch is producing events at rate.

## Related

- Issue 311 — design.
- Issue 314 — bounded JSON writer this builds on.
- Issue 304 — dispatch produces task_start / task_end.
- Issue 301 — pool runner produces run_start / run_end.
