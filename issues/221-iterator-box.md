# 221 — Iterator box: editor surface for round-robin output routing

## Status
open (editor surface only)

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
