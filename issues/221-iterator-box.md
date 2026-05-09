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
increments each call and wraps back to 0 after the last output. The
function itself is a dumb passthrough — it does not make the routing
decision. The executor does, based on the counter.

Useful for distributing work across parallel paths, cycling through a
list of consumers, or producing round-robin load balancing in the graph.

### Counter model
The counter is per-box-instance, stored in the executor's in-memory
state (keyed by box ID). It resets to 0 at the start of each run.
Persistence across runs is not in scope; that is a separate issue.

In the compiler output (issue 219), the counter becomes a local variable
in a generated closure — same reset-per-program-start behavior.

Wrap-around: `counter = (counter + 1) % #iterator_outputs`

### The iterator function
Each language ships a standard iterator function in `libs/`:

```
libs/iterator.lua  — M.iterate(counter, data) → data
libs/iterator.sh   — function iterate() { local counter="$1"; echo "$2"; }
libs/iterator.c    — reads argv[2] (counter), argv[3] (data); prints data
```

The counter is the FIRST argument (position 0 in the driver's arg list,
before the declared inputs). This is intentional: last-arg convention is
for variadic tails where the count is unknown; the counter is always
exactly one value and is always findable at position 0 without counting.
The function returns `data` unchanged. The routing decision is entirely
in the executor.

The user points the box's `ref` and `fn` at the appropriate language's
iterator lib file. The `iterator_outputs` field in the box JSON is what
flags the executor to treat this box as an iterator rather than a plain
call box.

### Data model
Box JSON gains an `iterator_outputs` field — an ordered array of
user-defined output path names:

```json
{
  "id": "router",
  "kind": "call",
  "ref": "libs/iterator.lua",
  "fn": "iterate",
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

`from_branch` is reused for iterator output names — the same field that
holds "lt"/"eq"/"gt" for comparators. The executor checks whether the
box has `iterator_outputs`; if so, it fires only the connection whose
`from_branch` matches `iterator_outputs[counter]`.

### Auto-grow output slots
Connecting a wire to the last slot of the iterator's output side appends
a new empty named slot — identical to the auto-grow behavior for variadic
inputs (issue 217). The user names each slot in the inspector. Connecting
to the last slot prompts for a name and grows the list. Disconnecting
removes the slot and compacts (same rename-and-compact logic as variadic
inputs).

### Executor changes
In `fire_connections`, a new branch handles iterator boxes:

```
if box.iterator_outputs and #box.iterator_outputs > 0:
    counter = executor_state[box.id .. ".counter"] or 0
    target_branch = box.iterator_outputs[counter + 1]  -- 1-indexed in Lua
    fire only connections where from_branch == target_branch
    executor_state[box.id .. ".counter"] = (counter + 1) % #box.iterator_outputs
```

The counter state table lives alongside the existing value store in
the executor.

### Driver contract addition
The executor injects the counter as the first argument when invoking the
driver for an iterator box, before any declared input values:

  `<driver> <file> <fn> <arg_count+1> <counter> [data_args...]`

The counter is always at position 0 in the arg list — no counting required
to find it. Non-iterator call boxes are unaffected.

### Inspector changes
The inspector shows:
- An "iterator" toggle per box (like the variadic toggle on inputs)
- When toggled on: the output side of the box renders named slots instead
  of a single dot; a "+" row at the bottom adds a new slot
- Each slot name is editable inline
- The current counter value is shown as a read-only display (useful for
  debugging live runs)

### Compiler output (issue 219 integration)
For an iterator box, the compiler generates a closure with a captured
counter variable:

```lua
local router_counter = 0
local function router(data)
  local idx = router_counter
  router_counter = (router_counter + 1) % 3
  if     idx == 0 then worker_a(data)
  elseif idx == 1 then worker_b(data)
  else                  worker_c(data)
  end
end
```

The iterator lib files are bundled alongside other box source files.

### Threading model integration (3d-rts thread pool)

In the pthreads task pool, each box execution is a task struct containing:
- A function pointer to the language-specific invocation wrapper
- A pointer to shared memory holding the input arguments and return value
- A pointer list of dependency tasks (upstream boxes whose output this box
  needs before it can run)
- For iterator boxes: a `counter` field (integer, part of the task struct)

**Blocking:** when a task cannot run because its dependencies haven't
completed, it is placed in the blocking task's waiting list. No memory is
allocated — the task struct stays alive, only a pointer to it is moved.
When the blocking task completes and writes its return value to shared
memory, it walks its waiting list and re-adds each waiting task to the
main queue (again, pointer move only — no reallocation or struct copying).

**Iterator self-re-add:** when an iterator task completes, it does not
terminate. Instead:
1. It increments its own `counter` field (or wraps to 0).
2. It checks if there is another pending input value in its input queue.
3. If yes: it updates its input pointer to the next pending value, then
   re-adds itself to the main task queue. No new allocation.
4. If no: it adds itself to the waiting list of its upstream dependency,
   to be re-added when the next input value arrives.

This means the iterator task struct is allocated once and lives for the
duration of the run, cycling through the queue as many times as there are
input values to process. Input values are processed one-at-a-time in
arrival order — if 10 values are queued, the iterator visits the queue 10
times, each time firing a different output path and advancing the counter.

**C calling Lua:** the per-thread invocation wrapper is a C function
(pthreads requires a C entry point). Each worker thread initializes its
own `lua_State` at startup — Lua states are not thread-safe but one per
thread is fine. The wrapper dispatches on language tag: if Lua, calls
through the thread's `lua_State` via `lua_pcall`; if native C, calls the
function pointer directly (no interpreter overhead); if bash, spawns a
subprocess (the one case that doesn't map cleanly to pthreads — see
docs/004-ipc-and-threading.md for options).

## Open questions
- Should the counter be exposed as a readable data port (wirable to
  another box as an input) in addition to being shown in the inspector?
- Should auto-grow prompt for the output name immediately on connect, or
  generate a placeholder (`out_N`) that the user renames afterward?

## Suggested implementation sequence
1. `libs/iterator.lua` — trivial passthrough function.
2. `libs/iterator.sh`, `libs/iterator.c` — same for bash and C.
3. `src/001-schema.lua` — add `iterator_outputs` to the box validator.
4. `src/004-executor.lua` — add counter state table; extend
   `fire_connections` with the iterator branch; inject counter arg in
   `run_task` when box has `iterator_outputs`.
5. `assets/js/004-inspector.js` — iterator toggle; editable output slots.
6. `assets/js/006-wires.js` — auto-grow output slots on connect;
   auto-shrink + rename on disconnect (mirrors variadic input logic).
7. `assets/js/002-boxes.js` — render N named output dots when
   `iterator_outputs` is set (replacing the single-wire dot).

## Relevant files
- `src/004-executor.lua` — fire_connections, run_task
- `src/001-schema.lua` — box validator
- `assets/js/004-inspector.js` — iterator toggle, slot editor
- `assets/js/006-wires.js` — auto-grow/shrink for output slots
- `assets/js/002-boxes.js` — box rendering
- `issues/217-concat-box-and-dynamic-inputs.md` — variadic input model
  (auto-grow/shrink logic mirrors this)
- `issues/completed/108-branch-box-and-predicate-routing.md` — comparator
  model that `from_branch` and the routing pattern extend from
- `issues/219-map-compiler.md` — compiler integration notes above
