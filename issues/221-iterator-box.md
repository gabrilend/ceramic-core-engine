# 221 — Iterator box: round-robin output routing with per-instance counter

## Status
open

## Current behavior
No mechanism exists for a box to distribute output across multiple
downstream paths in sequence. The comparator routes based on a value
threshold; the iterator routes based on a counter that advances each call.

## Intended behavior

### What it does
An iterator box receives one input value and routes it to one of N named
output wires. Which wire fires is determined by an internal counter that
increments each call and wraps back to 0 after the last output. There is
no function — the dispatch layer copies input directly to output, no
language spec is invoked. The iterator is a pure routing primitive.

Useful for distributing work across parallel paths, cycling through a
list of consumers, or producing round-robin load balancing in the graph.

### Counter model
The counter is per-box-instance, stored in the executor's in-memory
state (keyed by box ID). It resets to 0 at the start of each run.
Persistence across runs is not in scope; that is a separate issue.

In the compiler output (issue 219), the counter becomes a local variable
in a generated closure — same reset-per-program-start behavior.

Wrap-around: `counter = (counter + 1) % #iterator_outputs`

### No function — pure dispatch primitive
There is no iterator function in any language. There are no
`libs/iterator.lua` / `iterator.sh` / `iterator.c` files. The dispatch
layer (issue 304) recognizes iterator boxes by the presence of
`iterator_outputs` in the box JSON and handles routing itself:

1. Read the input value from the input slot.
2. Copy it directly into the output slot.
3. Fire the connection whose `from_branch` matches
   `iterator_outputs[counter]`.
4. Increment counter mod `n_iter_outputs`.
5. Re-spawn a successor dispatch task with the new counter value.

This replaces the earlier "passthrough function" design. The function
was always identity, so there was nothing to invoke — the dispatch
layer does the copy directly and skips the language spec entirely.

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
that holds "lt" / "eq" / "gt" for comparators. The dispatch layer
checks whether the box has `iterator_outputs`; if so, it routes by
counter as described above.

### Auto-grow output slots
Connecting a wire to the last slot of the iterator's output side appends
a new empty named slot — identical to the auto-grow behavior for variadic
inputs (issue 217). Connecting to the last slot generates a placeholder
name (`output_N`) and grows the list immediately — no prompt. The user
can rename the slot in the inspector if they care, but the default
flow does not interrupt them. Disconnecting removes the slot and
compacts (same rename-and-compact logic as variadic inputs).

### Dispatch layer integration
The dispatch layer (issue 304) is where iterator semantics live. The
counter is stored on the `dispatch_task_t` for each iterator
invocation; successor tasks carry the next counter value. There is no
language driver, no function call, no executor-state table — just the
counter on the task struct.

See issue 304 for the full dispatch flow. The iterator-specific path
is short: read input slot, copy to output slot, fire the routing
connection, increment counter, re-spawn.

### Inspector changes
The inspector shows:
- An "iterator" toggle per box (like the variadic toggle on inputs)
- When toggled on: the output side of the box renders named slots instead
  of a single dot; a "+" row at the bottom adds a new slot
- Each slot name is editable inline
- The current counter value is shown as a read-only display (useful for
  debugging live runs)

### Compiler output (issue 219 integration)
For an iterator box, the compiler emits a counter variable and a
routing function — no inner function call, since the iterator does
not invoke anything:

```lua
local router_counter = 0
local function router(data)
  local idx = router_counter
  router_counter = (router_counter + 1) % 3
  if     idx == 0 then worker_a(data)
  elseif idx == 1 then worker_b(data)
  else                 worker_c(data)
  end
end
```

No iterator lib file is bundled — there is none.

### Threading model
Iterator routing in the thread pool runner is fully described in issue
304. Briefly: each invocation is a `dispatch_task_t` with the current
counter value; on completion the dispatch action fires the routing
connection, increments the counter, and submits a successor task. The
input slot is a queued (multi-cell) ring buffer that persists across
re-spawns. See issues 302 (slot store) and 304 (dispatch layer) for
the full picture.

## Open questions

(none currently — earlier questions resolved as follows:)

- Counter exposed as a readable data port: no. Adds complexity most
  users won't need.
- Auto-grow naming: placeholder (`output_N`) on connect, user renames
  afterward if they care. No prompt.

## Suggested implementation sequence
1. `src/001-schema.lua` — add `iterator_outputs` to the box validator;
   when present, `ref` and `fn` are not required.
2. `src/004-executor.lua` (phase 2 path) — add per-box counter state;
   extend `fire_connections` to route by `iterator_outputs[counter]`
   and copy input → output without invoking a driver.
3. `assets/js/004-inspector.js` — iterator toggle; editable output slots.
4. `assets/js/006-wires.js` — auto-grow output slots on connect;
   auto-shrink + rename on disconnect (mirrors variadic input logic).
5. `assets/js/002-boxes.js` — render N named output dots when
   `iterator_outputs` is set (replacing the single-wire dot).
6. (Phase 3) Implement iterator dispatch in `src/008-pool-runner.c` per
   issue 304 — counter on `dispatch_task_t`, successor re-spawn.

## Relevant files
- `src/004-executor.lua` — phase 2 fire_connections / route-by-counter
- `src/001-schema.lua` — box validator (`iterator_outputs` field)
- `assets/js/004-inspector.js` — iterator toggle, slot editor
- `assets/js/006-wires.js` — auto-grow/shrink for output slots
- `assets/js/002-boxes.js` — box rendering
- `issues/217-concat-box-and-dynamic-inputs.md` — variadic input model
  (auto-grow/shrink logic mirrors this)
- `issues/completed/108-branch-box-and-predicate-routing.md` — comparator
  model that `from_branch` and the routing pattern extend from
- `issues/219-map-compiler.md` — compiler integration
- `issues/304-task-dispatch-layer.md` — phase 3 iterator dispatch (no
  function invocation; routing only)
