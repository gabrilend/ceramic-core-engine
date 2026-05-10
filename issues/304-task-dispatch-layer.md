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
- Picks the output branch per `routing.kind` (issue 233)
- Pushes the function's output to the picked branch's downstream
  input slots, triggering downstream task spawns

There is no Lua-level executor in the phase 3 path. `004-executor.lua`
is replaced wholesale.

## Task struct

A task is one ephemeral invocation of a box. Its struct carries
just the box ID:

```c
typedef struct {
    int  box_id;          // index into the graph's box array
} dispatch_task_t;
```

There is no per-task counter. Iterator counter state lives in a
`SLOT_ATOMIC_COUNTER` slot owned by the box (see issue 302); the
action reads and atomically increments it via `slot_read_inc`
when it needs the routing index.

There is no `input_slots` or `output_slot` array on the task —
the box's input slots are durable per-box state (see issue 302).
The action looks them up through `box_runtime_state[box_id]`.

Output is a routing event, not a stored value. When the action
finishes, it pushes copies of the return value into the input
slots of every downstream box wired to this output (or only the
matching branch for comparators and iterators).

Tasks are spawned by the dispatch layer's "spawn on input ready"
rule (issue 302). Each spawn allocates a fresh `dispatch_task_t`
on the pool's task allocator, pre-populated with the box ID. The
action's first read of the input slots is what drains them — pop
or peek one cell per input port, per the port's mode.

## The dispatch action

```c
action_result_t dispatch_action(task_ctx_t *ctx, void *arg);
```

Phases, in order:

### 1. Read inputs into byte buffers

Tasks are spawned only when every input port has a value
available (issue 302's spawn-on-input-ready rule), so the action
does not need to check or block — input availability is the spawn
precondition.

For each input port:
- **Peek-mode (1-cell) port**: `slot_peek` into a heap buffer.
  The cell is not drained; subsequent tasks on this box read the
  same value.
- **Pop-mode (N-cell) port**: `slot_pop` into a heap buffer. One
  cell drains; values queued behind it remain. For tagged slots,
  pop returns the lowest-tag cell; for untagged, FIFO.

Variable-size payloads work via the large-value heap (issue 302):
the cell holds a `{size, offset}` handle that points into the
heap; the spec follows the handle if it needs the bytes.

Buffers are freed when the action ends.

### 3. Dispatch is uniform across routing kinds

Every call box runs its function (phase 4) and pushes the
function's output (phase 5). The only thing that varies is which
downstream branch receives the push (phase 5b — the branch-pick).

Boxes carry an optional `routing` field (issue 233):
- No `routing`: plain call. Function runs; output fans to all
  outgoing wires unconditionally.
- `routing.kind: "comparator"`: function runs; output is
  compared to `comparand`; only the matching `lt` / `eq` / `gt`
  branch fires.
- `routing.kind: "iterator"`: function runs; output is routed to
  `outputs[slot_read_inc(counter_slot, n_outputs)]`.
- `routing.kind: "randomizer" / "weighted" / "distributor"`:
  function runs; output is routed by the kind-specific rule.

The function always runs. There is no "iterator skips invoke"
case — the iterator differs from a plain call only in how the
function's output is routed, not in whether the function runs.
Iterators that want passthrough behavior use an identity function
(or the editor defaults to one when no `ref`/`fn` is set).

### 4. Invoke (every call box)
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

The action picks the branch (or branches) and pushes the
function's output. Branch picking dispatches on `routing.kind`:

- **No routing field** (plain call): push to every outgoing
  connection.
- **comparator**: compare output to `comparand`, push to the
  matching `lt` / `eq` / `gt` connection only.
- **iterator**: `idx = slot_read_inc(box->counter_slot,
  n_outputs)`; push to the connection at branch `out_idx`.
- **randomizer / weighted / distributor**: kind-specific rule,
  same shape (compute `idx`, push to that one connection). See
  issue 233 for the rules.

For routing kinds that derive an order tag (iterator,
randomizer, weighted), the push carries `tag = idx` so consumers
downstream of parallel iterators preserve order.

After each push, the spawn-on-input-ready check (issue 302) fires
on the receiving box. If that push completes its input set, a
fresh task spawns for the consumer.

### 6. (No phase 6)

Slots are durable for the run; tasks don't unref. Run termination
is governed by the pool's active-task counter (issue 301). The
dispatch action goes from phase 5 directly to phase 7.

### 7. Post-action: nothing for the action itself

Routing already happened in phase 5. Counter advancement, where
applicable, happened inside `slot_read_inc` during phase 5 (the
read-and-increment is the same op). No box-level mutable state
needs touching here.

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

## Iterator counter and parallel iteration

The iterator's counter lives in a `SLOT_ATOMIC_COUNTER` slot
allocated for the box at graph load time (issue 302). The
dispatch action reads and increments it atomically, mid-action:

```c
uint32_t branch_idx = slot_read_inc(box->counter_slot, n_iter_outputs);
slot_push(downstream[branch_idx].input_slot, output_buf, output_size,
          /* tag = */ branch_idx);
```

`slot_read_inc` returns the current counter value `mod
n_iter_outputs` and atomically advances the underlying counter.
Concurrent iterator tasks on different workers each call
`slot_read_inc` independently and each get a unique value —
**iterator tasks parallelize across workers**, picking distinct
branches without coordination.

No box-side mutable state, no per-task counter snapshot. The
counter is just data in a slot the dispatch layer reads.

## Cross-iterator pairing under parallel iteration

With parallel iterator tasks, push ordering at the consumer is
non-deterministic — task 6 of iterator A may push to consumer C
before task 5 if 6 happened to land on a faster worker. FIFO pop
at C would consume them out of iteration order.

The fix is **counter-tagged pushes**. Each iterator push (in phase
5) carries the counter value the action read for this invocation:

```c
slot_push(downstream.input_slot, output_buf, output_size,
          /* tag = */ branch_idx_or_counter_snapshot);
```

Consumer slots downstream of iterators are allocated with the
`SLOT_TAGGED` flag (issue 302). `slot_pop` on a tagged slot
returns the cell with the lowest tag — preserving iterator-counter
order regardless of push race.

Compile-time analysis decides which slots are tagged: any slot
whose feeding wire originates (transitively, through a fan-out
chain) at an iterator branch gets tagged. Slots not downstream of
iterators are untagged and pop FIFO at no extra cost.

Plain-call and comparator pushes carry tag = 0 (or the snapshot of
their own iterator-ancestor's counter, if applicable, propagated
through their phase-5 pushes). Tags compose naturally through
multi-hop chains.

### Cross-iterator pairing of multiple iterator inputs

When non-iterator consumer C has two input ports wired from two
different iterators A and B, each input port has its own tagged
queue. Both queues pop in tag order independently:
- C's task K pops the K-th tag from A's queue (which equals A's
  K-th invocation's value).
- The same K-th tag from B's queue.

Pairing by tag = `(A_K, B_K)` for every K, deterministic
regardless of which iterator's tasks run faster.

## Routing kinds (issue 233)

Every call box runs its function. Routing kinds differ only in
how the function's output is dispatched to downstream
connections:

| `routing.kind`   | Output ports        | Branch picker                                                |
|------------------|---------------------|--------------------------------------------------------------|
| (absent / plain) | single port         | fan to all outgoing connections                              |
| `comparator`     | `lt` / `eq` / `gt`  | compare(output, comparand) → lt/eq/gt                        |
| `iterator`       | `out_0` … `out_N-1` | `slot_read_inc(counter_slot, N)`                             |
| `randomizer`     | `out_0` … `out_N-1` | `hash(slot_read_inc(counter_slot, MAX)) % N`                 |
| `weighted`       | `out_0` … `out_N-1` | cumulative-band lookup against a counter scaled to PRECISION |
| `distributor`    | `out_0` … `out_N-1` | argmin over downstream slot fill levels                      |

Plain call (no `routing` field) skips the branch picker entirely
— phase 5 fans the output to all wires. The function still runs
the same as for any branched call. Issue 233 carries the schema
and the per-kind UI.

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
