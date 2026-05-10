# 304 — Task dispatch layer (C, replaces synchronous executor)

## Status
open

## Current behavior
The synchronous executor at `src/004-executor.lua` walks the graph in
dependency order, calling each box's driver, capturing stdout, decoding
JSON, and propagating values to downstream boxes. Single-threaded,
blocking, runs in Lua coroutines.

## Concept

The task dispatch layer is the C component that runs inside each pool
worker. The pool calls a single C action — `dispatch_action` — per
task. That action is the one place in the runtime that:

- Reads from and writes to slots
- Calls `lang_spec->invoke`
- Manages slot refcounts on consumed inputs
- Implements iterator routing and self-rescheduling
- Implements comparator branch routing
- Spawns successor tasks when its box's outputs feed downstream boxes

There is no Lua-level executor in the phase 3 path. `004-executor.lua`
is replaced wholesale.

## Task struct

A task is one ephemeral invocation of a box. Its struct carries
just enough to point the action at the box and the snapshot of its
counter at spawn time:

```c
typedef struct {
    int  box_id;          // index into the graph's box array
    int  counter;         // iterator only; snapshot at spawn time
} dispatch_task_t;
```

There is no `input_slots` array on the task — the box's input
slots are durable per-box state (see issue 302). The action looks
up the input and output slots through `box_runtime_state[box_id]`.

There is no `output_slot` either — output is a routing event, not
a stored value. When the action finishes, it pushes copies of the
return value into the input slots of every downstream box wired
to this output (and only the matching branch for comparators and
iterators).

Tasks are spawned by the dispatch layer's "spawn on input ready"
rule (issue 302). Each spawn allocates a fresh `dispatch_task_t`
on the pool's task allocator, pre-populated with the box ID and
counter value. The action's first read of the input slots is what
drains them — pop one cell per input port.

## The dispatch action

```c
action_result_t dispatch_action(task_ctx_t *ctx, void *arg);
```

Phases, in order:

### 1. Read inputs into byte buffers

Tasks are spawned only when every input port has at least one
value (issue 302's spawn-on-input-ready rule), so the action does
not need to check or block — input availability is the spawn
precondition.

For each input port: `slot_pop` from the box's input slot into a
heap-allocated buffer sized to the slot's `cell_capacity`. Pop
drains one cell from the front of the ring; subsequent values
behind it are still queued and will spawn further tasks.

Variable-size payloads work via the large-value heap (issue 302):
the popped cell holds a `{size, offset}` handle that points into
the heap; the spec follows the handle if it needs the bytes.

Buffers are freed when the action ends.

### 3. Dispatch by box mode
The action now branches on the box's mode:

- **Plain call box**: proceed to phase 4 (invoke) and phase 5 (write
  output) as below.
- **Comparator box**: a call box that *also* carries a `comparand`
  field. The function still runs (phase 4 + 5 happen), so the box
  has a `ref` / `fn` like any other call box. The difference shows
  up at routing time (phase 7): instead of one output wire, three
  branches `lt` / `eq` / `gt` are available, and the dispatch layer
  picks which one to fire by comparing the function's *output value*
  to `comparand`. The function is unchanged from a plain call box's
  perspective; it just produced the value the routing layer compares.
- **Iterator box**: skip invoke entirely. Copy the input value
  straight into the output slot, and proceed to phase 7. Iterators
  have no `ref` / `fn`; the routing-by-counter is the entire point
  of the box.

Iterator semantics live in the dispatch layer (no spec involvement).
Comparator semantics live partly in the dispatch layer (the
output-vs-comparand comparison and branch fire) and partly in the
spec (the function still runs to produce the value being compared).

### 4. Invoke (plain call AND comparator call boxes)
Look up the box's language spec by its `lang` field. Find the
worker's language handle in `current_worker->handles[lang_idx]`.
Call `lang->invoke` with:
- `handle` — the per-worker language runtime state
- `file_path`, `fn_name` — the box's function source
- `input_data[]`, `input_sizes[]` — pointers and sizes for each
  input buffer
- `out_buf`, `out_buf_capacity`, `*out_size` — the output buffer the
  spec writes into

The spec is the bridge between the language's native call convention
and bytes. Inside `invoke` it:
1. Loads the function (cached: e.g. `luaL_loadfile` once per file,
   `dlsym` once per function).
2. Pushes the input bytes into the language's native types (Lua
   strings on the Lua stack, C pointers in registers, etc.).
3. Calls the function and receives the return value in the language's
   native form.
4. Serializes the return value back into bytes in `out_buf` and
   sets `*out_size`.
5. Returns 0 for success, nonzero for any failure.

Nonzero return aborts the program (issue 303).

The output buffer for `invoke` is heap-allocated to the box's
declared maximum output size. If the box's output is variable-size
(uses the large-value heap, issue 302), the spec writes into that
heap and stores the handle in `out_buf`.

### 5. Write output (push to downstream input slots)

Output is a routing event. The action walks the box's outgoing
connection list and calls `slot_push(downstream.input_slot,
output_buf, output_size)` for each one — copying the output bytes
into every consumer's input slot.

For comparator boxes: only push to the connection whose
`from_branch` matches the lt/eq/gt result. For iterator boxes:
only push to the connection whose `from_branch` matches
`iterator_outputs[counter]`.

After each push, the spawn-on-input-ready check (issue 302) fires
on the receiving box. If that push completes its input set, a
fresh task spawns for the consumer.

### 6. End-of-stream propagation

Slots are durable for the life of the run; there is no per-task
unref step. Instead, the action checks whether the **producer set**
on each pushed-to slot has changed: if this push was the last
push from a producer that's now finished, the producer is removed
from each of that consumer's input slots' producer sets. When a
consumer's producer set goes empty AND its slot is drained, the
consumer is at end-of-stream on that port.

For an iterator, "end-of-stream on the input" is what tells the
dispatch layer to stop spawning successor tasks. For a non-iterator
that's done with all its expected invocations, the same condition
applies — no further pushes will arrive, no more tasks spawn.

### 7. Post-action: counter advancement (iterators only)

Routing already happened in phase 5 — pushes targeted the right
downstream input slots based on the box's mode. The remaining
post-action work is iterator-specific:

- **Iterator**: increment `box_runtime_state[box_id].counter`
  modulo `n_iter_outputs`. The next iterator task spawned for this
  box (when its input slot has another value) will read the
  updated counter value from the box state.

There is no "successor task" spawned by the iterator itself. The
spawn-on-input-ready rule in issue 302 handles iterator
re-invocation the same way it handles every other box: the
iterator's input slot has further values queued, the spawn rule
fires again automatically.

Plain call and comparator boxes do nothing in phase 7.

### 8. Return ACT_DONE
The action completes. The pool decrements the active-task counter
(issue 301). If the counter reaches zero and the runner is waiting,
the run ends.

## Initial submission

At startup, the pool runner walks the graph and pushes the
literal-input values (from `value` fields on input ports) directly
into the corresponding input slots. Boxes whose input set becomes
fully populated by literals — entry-point boxes — immediately
satisfy the spawn-on-input-ready condition, and their first task
gets queued. From there, every subsequent task spawn follows from
the regular phase-5 push → spawn-check chain.

Iterator counter state starts at 0; literal-only entry iterators
spawn their first task at startup and increment from there.

## Cross-iterator pairing

When a non-iterator consumer C has two input ports wired from two
different iterators A and B, each input port has its own queue.
Tasks for C spawn FIFO: task 1 pops the first value from each
queue; task 2 pops the second from each; etc.

Pairing is deterministic **as long as A and B push in deterministic
order**. In a single-threaded run that's automatic. In a parallel
run where A and B run on different workers and push concurrently,
the pairing depends on push ordering — which is non-deterministic
across runs.

For phase 3, FIFO pairing is the policy. If a workload needs
strict pairing (a_i with b_i regardless of push race), the right
fix is per-invocation index tags on the values: each iterator
push tags its value with its counter, the consumer's queues sort
by tag before popping. That's a follow-on issue (no number yet),
opened only if a real workload needs it.

## Comparator vs iterator vs plain call

Three box modes drive routing differently:

| Mode        | Output kind   | Invoke spec? | Routing behavior                                                    |
|-------------|---------------|--------------|---------------------------------------------------------------------|
| Plain call  | single value  | yes          | fire all outgoing connections                                       |
| Comparator  | passthrough   | no           | dispatch layer compares input to threshold, fires lt / eq / gt only |
| Iterator    | passthrough   | no           | fire connection matching counter, increment counter, re-spawn       |

The dispatch action branches on box mode early (phase 3) and
dispatches into the appropriate helper. Three small functions, not a
single switch, keep each mode's logic isolated.

## Open questions

(none currently — earlier questions resolved as follows:)

- Output buffer sizing: heap-allocated to the box's declared output
  size at runtime. No fixed cap. Variable-size outputs go through the
  large-value heap (issue 302).
- Queued-input slot lifetime: producers hold refs while alive,
  consumer holds a ref while consuming, slot freed when all drop.
  References are the producer-liveness tracking mechanism.
- Comparator output encoding: dispatch layer handles comparison
  semantics directly. No spec involvement, no encoded "branch tag" in
  the slot — the branch is a routing-time value computed by the
  dispatch layer from the input.

## Suggested implementation sequence

1. Define `dispatch_task_t` and the `dispatch_action` skeleton.
2. Implement phase 1 (input check + ACT_BLOCK on slot wait list).
3. Implement phase 2 (input read into heap-allocated buffers).
4. Implement phase 4 (invoke for plain call boxes).
5. Implement phase 5 (output write via slot_push).
6. Implement phase 6 (unref consumed single-value inputs).
7. Smoke test: `hello` map (one plain call box, no routing) end-to-end
   through the pool. Assert the output slot contains the expected
   value.
8. Implement plain-call fan-out firing (phase 7, plain case).
9. Implement comparator routing (phase 3 + phase 7 comparator case).
   Test `branch-test` map.
10. Implement iterator routing and self-respawn (phase 3 + phase 7
    iterator case). Test an iterator map.
11. Wire up entry-box initial submission in the pool runner.

## Relevant files

- `src/004-executor.lua` — synchronous executor that this layer replaces
- `issues/301-pool-lifecycle-and-worker-init.md` — pool that runs this action
- `issues/302-wire-value-slot-store.md` — slot API used here
- `issues/303-language-runtime-spec.md` — `invoke` interface called here
- `issues/221-iterator-box.md` — iterator routing and self-rescheduling
- `issues/completed/108-branch-box-and-predicate-routing.md` — comparator routing
- `issues/213-queued-inputs-and-task-model.md` — queued-input slot semantics
