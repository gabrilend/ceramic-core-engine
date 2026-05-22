# 244 — Data boxes as round-robin default providers, pulled on demand

## Status
complete — pull-on-demand model ships end-to-end for both the
single-predecessor case (covered by the read-literal and hello
fixtures) and the round-robin case (covered by
`test_read_predecessor_rotation` in `tests/012-dispatch-test.c`,
which builds a three-read-box fixture, serial-spawns the
consumer three times, and verifies each spawn picks the next
predecessor in the loader's list order).

## Current behavior

Read boxes are pull-on-demand value sources. They never enter the
pool's task queue. At graph load every read box's bytes are read
once and cached on its box record (inline `value` is strdup'd;
`path` is fopen+fread). Every call/write box gets a per-input-port
list of its read-box predecessors. When a consumer attempts to
fire and finds its slot empty, the dispatch's `read_inputs` copies
the next predecessor's cached bytes into the consumer's input
buffer — single predecessor: always index 0; multiple predecessors:
the port's atomic-counter slot rotates the choice across parallel
attempts. A port satisfied by a read predecessor counts as ready
in `box_is_ready`, so consumers wired only to read boxes fire
immediately at startup without their predecessors having to push.

`do_read_box` is gone; the BOX_READ case in dispatch_action is now
a BUG path that fires only if a read box accidentally reaches the
pool.

This breaks down in two cases:

1. **Multiple read boxes feeding the same input slot.** The slot
   ends up holding one value (whichever read box's startup-push
   landed first) and silently ignores the others. The user's
   intent — "rotate among these values" — is not honored.
2. **A read box feeding a slot whose other producers fire many
   times.** Once the read box's startup push is consumed, the slot
   sits empty until a call-box producer fills it. If the consumer
   fires N times and the call-box producer also fires N times,
   the read box's value is consumed at most once. The user's
   intent — "this is a default that's always available" — is not
   honored.

## Intended behavior

A read box is an **inexhaustible source**, not a one-shot producer.
Its declared value is always available, on every consumer's
request. Consumers pull from read boxes when they need a value and
no value is currently waiting in the slot.

### Pull, not push

Read boxes do not push at startup, do not appear in the pool's task
queue, and do not have task structs of their own. They are
**callable sources** whose state lives in the box's runtime record:

- For an inline literal (`value` field set), the value sits in the
  box's record as bytes ready to copy.
- For a file path (`path` field set), the bytes are read once at
  graph load and cached in the box's record. The user editing the
  file mid-run is not supported; the cached bytes are the source of
  truth for the run.

The attempt-task model from the architecture doc makes the pull
happen naturally. When an attempt task examines a consumer's input
slots, the check is now:

```
for each required input port:
   if the slot has a value waiting (ring buffer non-empty):
      take that value
   elif the port has any read-box predecessor:
      round-robin pull from the next read-box predecessor
   else:
      not ready — return from the attempt
```

The pull copies the read box's value (or its `$ref` handle, if
the value is variable-size) into the consumer's local snapshot.
The read box's record is unchanged.

### Round-robin across multiple read-box predecessors

A consumer input port with N read-box predecessors carries a small
piece of state on the consumer box:

```
input_port_state {
   data_box_ids: [box_id, box_id, ...],   // declared order
   rr_index:     0                        // next to pull from
}
```

The attempt task uses `data_box_ids[rr_index]`, increments
`rr_index` mod `len(data_box_ids)`, and proceeds. Multiple attempt
tasks running in parallel for the same consumer use the same
counter via the atomic-increment slot operation from 302 — distinct
attempts get distinct indices, no race.

### Mixed predecessors: read boxes as defaults

A consumer port may have a mix of call-box predecessors AND
read-box predecessors. The attempt rule (slot-first, read-box-
fallback) handles this naturally:

- If a call-box producer has pushed a value, the slot is non-empty
  and the attempt consumes from there.
- If the slot is empty, the attempt pulls from the next read-box
  predecessor.

So a read box behaves as a **default value** for the slot — used
only when no call-box producer has supplied an actual value. This
matches the typical use: "supply a fallback host address, but if a
call box computes a custom one, use the custom one instead."

## Schema implications

The compile-time check from 230 currently requires every
non-optional input port to be wired, carry a literal, or be marked
optional. Under this issue, **a read-box predecessor on the port
also satisfies the requirement** — the port is guaranteed to be
fed at attempt time, just via pull rather than push.

`src/001-schema.lua::check_input_bindings` extends:

```
state if any of:
   has_wire (any producer-class predecessor)        ← unchanged
   has_literal (port.value set)                     ← unchanged
   has_data_box_predecessor (read-box predecessor)  ← new
   is_optional (port.optional == true)              ← unchanged
```

The editor's compile-or-save validation surfaces the same check;
the user sees an error if a required port has none of the above.

## Suggested implementation steps

1. `src/001-schema.lua::check_input_bindings` — extend to
   recognize read-box-as-predecessor as a satisfying state.
2. `src/010-graph-loader.c` — at graph load, walk every consumer
   box; for each input port, compute the list of read-box
   predecessors and store as part of the port's runtime state.
   Cache each read box's value bytes (or LVH handle) in the box's
   runtime record.
3. `src/012-dispatch.c` — the attempt-task's input check
   incorporates the read-box pull: slot first, else round-robin
   from `data_box_ids[rr_index]`.
4. The startup phase no longer spawns tasks for read boxes (or
   pushes their values eagerly). Read boxes are graph-load data
   only; they do not appear in the pool's queue.
5. Fixture: `tests/maps/data-box-defaults/` — a consumer with one
   call-box and one read-box predecessor, exercising both "value
   available" and "fallback pulled" paths.
6. Fixture: `tests/maps/data-box-rotation/` — a consumer with three
   read-box predecessors, asserting round-robin order over a series
   of attempt invocations.

## Cycle behavior

Pull-on-demand removes a startup-ordering concern: a graph with a
read box feeding two consumers no longer depends on "did the read
box's push land before either consumer fired its first attempt?" —
the attempts pull when they need to, regardless of order. Iterators
downstream of read boxes work cleanly: each iteration's attempt
pulls fresh from the read box (or its rotation).

## Relevant files

- `issues/completed/229-data-box-as-language-agnostic-file-io.md` —
  parent; renamed data→read/write. This issue extends the read-box
  semantics from "one-shot producer" to "inexhaustible pull-source."
- `issues/completed/230-required-inputs-and-optional-flag.md` —
  parent; this issue adds a third satisfying state to its check.
- `src/001-schema.lua` — extended binding check.
- `src/010-graph-loader.c` — per-port read-box-predecessor list
  computed at load.
- `src/012-dispatch.c` — attempt-task input pull.
- `docs/001-architecture.md` — the attempt loop (described in the
  "structural shell" section) is the place where the pull happens.

## Out of scope for this issue

- **Schema validation in `src/001-schema.lua`.** The phase 2 Lua
  schema is the canonical author-side check that runs in the
  editor. Adding the "read-box predecessor satisfies a required
  port" condition there belongs to phase-2 schema work, not to
  this issue's phase-3 dispatch redesign. The C loader's topology
  validation already accepts read-box-fed ports as satisfied
  (otherwise nothing here would work); the editor's static check
  is the part that's still on the old "wire or literal" rule.

## Open questions

- **Read-box value mutation during a run.** A file-backed read box
  whose file changes mid-run: do we re-read on every pull, or stay
  with the load-time cached value? Lean toward "load-time cache"
  (deterministic, no I/O per pull, mirrors the immutability of the
  graph during a run). Re-read on pull is a follow-on if the use
  case appears.
- **Round-robin determinism across parallel attempts.** The atomic
  counter for `rr_index` gives every attempt a distinct index, but
  the order in which attempts complete is non-deterministic. If
  attempts A1 and A2 both pull index 0 and index 1 respectively
  but A2 finishes first, the consumer's overall output sequence
  reflects A2 before A1. This is the same parallel-iterator
  ordering question 302's tagged slots already solve. Apply the
  same tagging: attempts that pull from read boxes carry the
  pulled index as a tag, downstream consumers reorder by tag.
