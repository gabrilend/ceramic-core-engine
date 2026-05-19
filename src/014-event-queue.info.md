# 014-event-queue.c — public surface

Phase 3 run-log event queue. Multi-producer, single-consumer.
Producers (worker threads, main thread) call emit functions that
append small event records under a brief mutex; a dedicated
writer thread drains in batches and writes JSONL through the
issue-311 emit helpers.

## Lifecycle

- `event_queue_t *event_queue_create(const char *path)` — open
  the file, spawn the writer thread, return ready-to-use.
- `void event_queue_destroy(event_queue_t *q)` — signal shutdown,
  drain remaining events, join the writer thread, close the file,
  free. Safe on NULL.

## Emit

- `event_queue_run_start  (q, ts, map, n_workers)`
- `event_queue_task_submit(q, ts, task_id, box_id, worker_idx)`
- `event_queue_task_start (q, ts, task_id, worker_idx)`
- `event_queue_task_end   (q, ts, task_id, worker_idx, duration_us, output_size)`
- `event_queue_run_end    (q, ts, duration_us, n_tasks)`

Producer side is non-blocking (modulo brief append-mutex
contention). String arguments are clone-copied into a bounded
buffer inside the event record (128 bytes for `map`, 64 for
`box_id`); callers' pointers don't need to outlive the call.

## Diagnostics

- `int event_queue_pending(const event_queue_t *q)` — best-effort
  count of events queued between produce and drain.

## What's NOT here (deferred)

- **Vyukov-style lock-free MPSC ring**. The architecture doc's
  preferred design. Current implementation uses a singly-linked
  queue under one mutex; the writer thread snapshots `head` and
  drains outside the lock, so encoding + fwrite happens with zero
  producer contention. Producer's critical section is one node
  alloc plus two pointer updates — minimal. Upgrade once
  profiling justifies it.
- **Verbosity gates** (`SORAMECH_LOG_SLOTS=1`,
  `SORAMECH_LOG_VALUES=1`). Slot-allocator and per-task value
  events are opt-in per the issue; not exposed yet.
- **Backpressure on a full ring**. The current linked-list queue
  has no hard bound; a slow writer just grows the queue.

## Related

- Issue 311 — design.
- Issue 313-jsonl-events — per-event serializers this thread calls.
- Issue 314 — bounded JSON writer underneath.
- Issue 304 — dispatch action emits task_start / task_end.
- Issue 301 — pool runner emits run_start / run_end.
