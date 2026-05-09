# 221 — Iterator box: editor surface for round-robin output routing

## Status
complete (editor surface only — runtime in 304)

## Implementation notes

The editor side of iterator boxes is in. Phase 3's dispatch layer
(304) will read `iterator_outputs` and rotate the counter.

Touches:
- `src/001-schema.lua` — accept `iterator_outputs` (array of
  strings); when present, `ref` is no longer required
- `assets/js/002-boxes.js::box_height`,`port_positions` — N output
  rows when `iterator_outputs` is set; box grows vertically; each
  slot's name lands on the dot exactly like a comparator branch
- `assets/js/004-inspector.js` — `is_iterator`, `make_iterator`,
  `unmake_iterator`, `add_iterator_slot`, `remove_iterator_slot`,
  `rename_iterator_slot`, `auto_grow_iterator_after_connect`;
  iterator toggle button next to kind dropdown; when active, the
  ref/fn/comparator UI is replaced by an editable slot list with
  per-slot remove and a "+ slot" button at the end
- `assets/js/006-wires.js::create_connection` — calls the iterator
  auto-grow helper after a wire from any branch is created (it's a
  no-op for non-iterators or non-last-slot connections)

### Behavior decisions

- Toggling on snips outgoing wires, strips ref/fn/comparand, seeds
  one empty slot named `output_0`.
- Toggling off snips outgoing wires, deletes `iterator_outputs`,
  restores empty `ref`/`fn` so the schema (which requires ref on
  non-iterator call boxes) accepts the save. User re-picks via
  browse to populate.
- Slot names are user-renamable — placeholder `output_N` only
  applied on auto-grow / `+ slot`. Existing slots keep their names
  on add/remove (no compaction; slot names are free-form, not
  numeric indices like the variadic input pattern).
- Removing a slot snips outgoing wires whose `from_branch` matched
  the removed name. Last slot can't be removed — toggle iterator
  off instead.
- Renaming a slot rewrites `from_branch` on every outgoing wire on
  both endpoints (source and destination boxes), so existing wires
  follow the rename rather than going stale.

### Edge cases verified by inspection

- Self-loop wires (issue 228) on an iterator slot draw with the
  looped control points: a self-feeding accumulator iterator is
  the motivating use case.
- The `data` kind also accepts `iterator_outputs` schema-wise. The
  editor doesn't constrain by kind; if the user puts iterator on a
  data box, the runtime will treat it as a routing primitive.
  Phase 3's loader can refine if it matters.

## Scope

This issue covers the **editor side** of the iterator box: schema
field, inspector toggle, wires auto-grow, canvas rendering. The
runtime semantics (counter on `dispatch_task_t`, successor task
re-spawn, copy-input-to-output without invoking a language spec) are
specified in issue 304 and implemented as part of phase 3.

The phase 2 synchronous executor (`src/004-executor.lua`) does not
get an interim iterator implementation — the executor is being
retired by the phase 3 pool runner, so adding throwaway support
here is wasted work. Maps with iterator boxes can be authored in
phase 2 and run in phase 3.

## Current behavior

No mechanism exists for a box to distribute output across multiple
downstream paths in sequence. The comparator (issue 210) routes
based on a value threshold; the iterator routes based on a counter
that advances each call.

## Intended behavior (editor)

### What it is

An iterator box has no `ref` / `fn`. It declares an ordered list of
output names in `iterator_outputs`. The runtime (issue 304) routes
input → one output per call, advancing a per-instance counter mod
`#iterator_outputs`. Pure routing primitive: there is no language
function being invoked.

### Data model

Box JSON has an `iterator_outputs` field — an ordered array of
user-defined output path names. Iterator boxes do not carry `ref` or
`fn`; those fields are absent (or, if present, ignored):

```json
{
  "id": "router",
  "kind": "call",
  "inputs": [{"name": "data", "type": "any"}],
  "iterator_outputs": ["path_a", "path_b", "path_c"],
  "connections": [
    {
      "from_box": "router", "from_branch": "path_a",
      "to_box": "worker-a", "to_input": "data"
    },
    {
      "from_box": "router", "from_branch": "path_b",
      "to_box": "worker-b", "to_input": "data"
    },
    {
      "from_box": "router", "from_branch": "path_c",
      "to_box": "worker-c", "to_input": "data"
    }
  ]
}
```

`from_branch` is reused for iterator output names — the same field
that holds `lt` / `eq` / `gt` for comparators. The dispatch layer
(issue 304) checks whether the box has `iterator_outputs`; if so, it
routes by counter.

### Inspector

- An "iterator" toggle per box (mirrors the variadic toggle on
  inputs).
- When toggled on: the output side of the box renders named slots
  instead of a single dot; a `+` row at the bottom adds a new slot.
- Each slot name is editable inline.
- The `ref` / `fn` / comparator controls hide while the iterator
  toggle is on (mutually exclusive with a function-backed box).
- Toggling iterator off restores `ref` / `fn` controls and snips all
  output wires (same all-wires-snipped behavior as the variadic
  toggle, per the stale-cache lesson from issue 217).

### Auto-grow output slots

Connecting a wire to the last output slot appends a new empty named
slot — identical to variadic input auto-grow (issue 217).
Connecting to the last slot generates a placeholder name (`output_N`)
and grows the list immediately, no prompt. Disconnecting removes the
slot and compacts (rename-and-compact mirror of variadic input
logic).

### Canvas rendering

When `iterator_outputs` is set, the box renders N named output dots
down the right edge instead of a single dot. Box height grows to
accommodate. Wire dragging starts from the dot whose slot the cursor
is over.

## Suggested implementation sequence

1. `src/001-schema.lua` — accept `iterator_outputs` (array of
   strings) on call boxes; when present, `ref` and `fn` are not
   required.
2. `assets/js/004-inspector.js` — iterator toggle; editable output
   slot list; mutual-exclusion with ref/fn/comparator controls;
   snip-all-wires behavior on toggle (both directions).
3. `assets/js/002-boxes.js` — render N named output dots when
   `iterator_outputs` is set; box height accommodates the slot
   count; `port_positions` returns N output positions.
4. `assets/js/006-wires.js` — auto-grow output slots on connect;
   auto-shrink + rename on disconnect (mirror of variadic input).

## Runtime

See issue 304 (task dispatch layer) for the full runtime flow:
counter on the dispatch task, copy-input-to-output, fire the
routing connection, increment counter, re-spawn successor task. No
language driver, no function call, no executor-state table.

## Open questions

- Counter exposed as a readable data port: no. Adds complexity most
  users won't need.
- Auto-grow naming: placeholder (`output_N`) on connect, user
  renames afterward if they care. No prompt.

## Relevant files

- `src/001-schema.lua` — box validator (`iterator_outputs` field)
- `assets/js/004-inspector.js` — iterator toggle, slot editor
- `assets/js/006-wires.js` — auto-grow/shrink for output slots
- `assets/js/002-boxes.js` — box rendering, port positions
- `issues/completed/217-concat-box-and-dynamic-inputs.md` —
  variadic input model that auto-grow logic mirrors
- `issues/completed/108-branch-box-and-predicate-routing.md` —
  comparator model that `from_branch` and routing pattern extend
- `issues/304-task-dispatch-layer.md` — phase 3 runtime semantics
- `issues/228-self-loop-wires-routed-around-box.md` — self-loop
  routing that pairs naturally with iterators
