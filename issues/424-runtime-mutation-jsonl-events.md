# 424 — Runtime-mutation JSONL events

## Status
open · phase 4 · sub-issue of [419](419-runtime-graph-mutation.md).
Mechanical once [420](420-utility-box-kind.md) lands; needs the
event-payload shapes finalised once the utility-box's behaviour
is decided.

## Intended behavior

The mutator path (inside the target's worker after popping a
RING_RECONFIGURE cell) emits one of three JSONL events each
time it completes work:

- `box_reconfigure` — fires after a successful graph-slot swap
  to a non-NULL replacement. Carries box id, post-apply spec
  (re-serialised from the new struct so the transcript shows
  what the box now does), task id, worker idx, monotonic
  timestamp.
- `reconfigure_discard` — fires once per abandoned port that
  still held queued values at reconfigure time. Carries box id,
  port name, count of dropped values, sample of the first value
  (up to 256 bytes), task id, worker idx, timestamp.
- `box_delete` — fires when the graph-slot transitions to NULL
  via a delete operation. Carries box id, task id, worker idx,
  timestamp.

Plus a fourth addition on the existing `push` event family:

- The `result` field grows a new value `target-gone`, emitted
  by `push_one_connection` when the slot lookup returns NULL
  for a previously-existing target id.

All three new events are always emitted (no opt-in verbosity
gate) — runtime mutations are rare and load-bearing for
debugging. The push-target-gone result piggybacks on
SORAMECH_LOG_SLOTS like every other push variant.

## Why these three

The phase-3 attempt used `box_create` and `wire_add` events.
The phase-4 design collapses create / reconfigure / delete into
one operation, so the event shape collapses too: a single
`box_reconfigure` event covers all transitions to a populated
state, and `box_delete` covers transitions to NULL. The
`reconfigure_discard` event is new — it captures values that
get dropped on the floor when the new port shape doesn't
include an old port, which the phase-3 attempt never had to
think about (it didn't support reconfigure).

## Suggested implementation steps

1. Add three `jsonl_emit_*` writers in
   `src/013-jsonl-events.c` and declarations in the header.
2. Add three thin `event_queue_*` wrappers in
   `src/014-event-queue.{c,h}` that funnel through `q->writer`
   like the existing always-on events.
3. Extend the `push` event's `result` field documentation in
   `docs/004-runtime.md` to include `target-gone`.
4. Document the three new events in `docs/004-runtime.md`'s
   event table.

## Open questions

- **Post-apply spec serialisation.** The
  `box_reconfigure` event's spec field re-serialises the new
  struct. This needs a `serialise_box_to_json(box_t *) ->
  string` helper that doesn't exist today. Add to the
  graph-loader or to a new helper module.
- **Sample size on `reconfigure_discard`.** 256 bytes is a
  reasonable default; the existing `task_input` /
  `task_output` events use 4 KB. Lean 256 because discards are
  rare and the sample is for spot-checking, not full
  diagnostics.

## Relevant files

- `src/013-jsonl-events.{c,h}` — three new writers + the new
  `target-gone` push-result value.
- `src/014-event-queue.{c,h}` — three new thin wrappers.
- `docs/004-runtime.md` — event documentation.
