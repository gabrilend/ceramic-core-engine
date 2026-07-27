# 014-event-queue.c — public surface

The run-log event queue. Multi-producer, single-consumer. Worker
threads and the main thread emit event records; one dedicated
writer thread drains them and writes JSONL through the
`013-jsonl-events` serializers.

The point of the split is that **no worker ever waits on disk**.
A worker's emit is a ring publish; encoding and `fwrite` happen on
the writer thread.

## Lifecycle

- `event_queue_t *event_queue_create(const char *path)` — open the
  file, spawn the writer thread, return ready to use.
- `void event_queue_destroy(event_queue_t *q)` — signal shutdown,
  drain what's left, join the writer, close the file, free. Safe
  on NULL.

## Emit — always on

- `event_queue_run_start  (q, ts, map, n_workers)`
- `event_queue_task_submit(q, ts, task_id, box_id, worker_idx)`
- `event_queue_task_start (q, ts, task_id, worker_idx)`
- `event_queue_task_end   (q, ts, task_id, worker_idx, duration_us, output_size)`
- `event_queue_run_end    (q, ts, duration_us, n_tasks)`

## Emit — opt-in

Gated by the runner's environment variables; the queue itself
always accepts them, the callers decide whether to emit.

- `event_queue_task_input (q, ts, ...)` — payload bytes in.
- `event_queue_task_output(q, ts, ...)` — payload bytes out.
  Both under `SORAMECH_LOG_VALUES=1`.
- `event_queue_slot_alloc (q, ts, ...)` — the startup slot-layout
  enumeration.
- `event_queue_push       (q, ts, ...)` — one per wire push,
  carrying `"ok"` or a short skip reason. Both under
  `SORAMECH_LOG_SLOTS=1`.

String arguments are clone-copied into a bounded buffer inside the
event record, so a caller's pointer does not need to outlive the
call.

## Diagnostics

- `int event_queue_pending(const event_queue_t *q)` — best-effort
  count of events queued between produce and drain.

## The ring

A Vyukov-style **bounded** MPSC ring: each slot carries a sequence
number, a producer claims a slot with one compare-exchange on the
enqueue position and publishes with a release store on the
sequence. The capacity is a power of two, sized to absorb dispatch
bursts rather than to bound total events.

**A full ring blocks the producer; it never drops.** That is a
deliberate trade — dropping events would silently destroy the
audit trail, and a transcript with invisible holes is worse than a
brief stall, because you cannot tell the difference between "this
did not happen" and "this was not recorded."

## What's NOT here (genuinely deferred)

- **Per-event filtering inside the queue.** Verbosity is decided
  by the callers before they emit; the queue has no notion of
  levels.
- **Rotation or size capping of the output file.** A long-running
  map writes an unbounded transcript.

## Related

- Issue 311 — design.
- `src/013-jsonl-events.c` — the per-event serializers the writer
  thread calls.
- `libs/json` — the bounded JSON writer underneath.
- `src/012-dispatch.c` — emits the task and push events.
- `src/008-pool-runner.c` — emits run start / end and owns the
  verbosity environment variables.
- `docs/004-runtime.md` — the transcript's event table and the
  sample output.
