# 213 — Queued inputs and synchronous task model

## Status

won't implement (in phase 2 — superseded by phase 3 issue 302)

## Resolution

The queue-per-port model described here is the right model. The
phase 2 implementation in `src/004-executor.lua` would be throwaway
work because phase 3 (issues 301–311) replaces the synchronous
runtime wholesale: the C pool runner uses ring-buffer slots in
shared memory (issue 302) for exactly the queue semantics this
issue describes.

Phase 3 already incorporates this issue's design — fan-in,
queue-drain, optional ports, and the "many wires into one input
port" model. See:
- `issues/302-wire-value-slot-store.md` — slots are ring buffers;
  N-cell rings handle queued inputs natively.
- `issues/304-task-dispatch-layer.md` — the dispatch action drains
  one value per input from each port queue.

Closing this issue without implementing it in the phase 2 Lua
runtime. Maps that rely on fan-in (multiple wires to one input
port) will produce undefined behavior under the phase 2 runner —
last-wire-wins, with no queue. Once phase 3 ships, fan-in works
correctly.

This means: don't ship a map that depends on fan-in until phase 3
lands. The editor allows constructing such maps; they just don't
run correctly yet.

## Current behavior

Each input port in the executor store holds exactly one value. When an upstream
box fires a connection, it overwrites whatever was previously in that slot. A box
only runs once (the `visited` guard prevents re-execution). This means:

- A box with two upstream feeders only processes the last-arriving value.
- Fan-in patterns (many outputs converging on one input) silently drop values.
- There is no way to process a stream of values through a box.

## Intended behavior

Each input port has a **queue** (ordered list) of waiting values instead of a
single slot. A box becomes "ready" when all of its required input ports have at
least one value in their queue. When a box runs, it pops one value from the
front of each required input queue, then runs the task with those values. After
the task completes, the box is checked again — if all required queues are still
non-empty, it runs again immediately, consuming the next set of inputs.

Optional ports (marked with `"optional": true` in the input object) do not gate
readiness. If a value is present in an optional port's queue when the task runs,
it is consumed; otherwise the task receives nil for that port.

This model enables:
- Fan-in: A collects 10 values, B collects 1. Each new value to B creates one
  task that consumes one A-value and the new B-value.
- Pipelines: output of one box feeds into the next, which re-runs for each
  arriving value.
- Broadcast: one box fans out to two boxes, both of which process that one value.

The executor stays **synchronous** in this phase. Coroutines are not required
for the synchronous model — the simple approach is: after each box run, re-check
all boxes (not just directly connected ones) for readiness and enqueue any that
are ready. Run until the queue is empty or an error occurs.

## Data model changes

**Store**: `store["box_id.port_name"]` changes from a single value to an array
of values (a queue). All reads pop from the front; all writes push to the back.

**Inputs satisfied**: `inputs_satisfied(box, store)` — a port is satisfied when
`#store[key] > 0`. Optional ports are always considered satisfied.

**Collect inputs**: `collect_inputs(box, store)` — shifts (pops front) one value
from each port's queue and returns the map of port-name → value.

**Visited guard**: remove the `visited` flag. Instead, enqueue a box whenever its
inputs become satisfied (even if it has run before). This is safe because
satisfaction requires queue entries, and collect_inputs consumes them.

**Literal values**: pre-seeded literal values are still pushed once into the
queue at startup. They are consumed on first run. A wire can push additional
values on top of the literal.

## Optional port convention

In the box file:
```json
{ "name": "temperature", "type": "any", "optional": true, "value": "0.7" }
```

`optional: true` means this port does not block readiness. The literal `value`
(if present) is still pre-seeded into the queue as a default. Wires can push
additional values.

## Suggested implementation steps

1. **`src/004-executor.lua`** — change store values to arrays. Update
   `inputs_satisfied` (check array length, skip optional ports). Update
   `collect_inputs` (table.remove from front). Remove `visited` table.
   After each `fire_connections`, re-scan all boxes for satisfaction and
   enqueue the newly ready ones.

2. **`src/001-schema.lua`** — add optional `"optional"` boolean to port objects.

3. **`src/004-executor.lua`** — pre-seeding: push literal values into arrays
   (`store[key] = { parsed_val }`) instead of assigning directly.

4. **`assets/js/004-inspector.js`** — add an "optional" checkbox per input port
   row. On change, update `port.optional` and save.

## Future: thread pool

When the synchronous model is replaced by a thread pool, the queue-per-port
design maps directly: a worker thread pops one value from each input queue,
runs the task, and pushes outputs to the connected ports' queues. The
satisfaction check becomes a condition variable or semaphore per port. The
synchronous executor described here is the reference implementation.

## Related documents

- src/004-executor.lua — execute, inputs_satisfied, collect_inputs, fire_connections
- src/001-schema.lua — port schema, optional field
- assets/js/004-inspector.js — optional port checkbox
- docs/007-architecture.md — store model, box.inputs schema
