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

## Implementation log

### Skeleton — 2026-05-12

`src/012-dispatch.{c,h,info.md}` ships the types and the action's
signature with a stub body that increments a diagnostic counter
on the context and frees the task. `dispatch_ctx_t` bundles the
graph, slot store, spec registry, and pool — every task carries a
pointer to the context so the action has everything it needs
without globals. `dispatch_spawn(ctx, box_id, priority)`
allocates a task and calls `pool_spawn`.

Two skeleton tests in `tests/012-dispatch-test.c` build the full
runtime (graph + slots + registry + pool) and submit either one
task per box, or 200 tasks at varied priorities, and assert that
the counter advances by the right amount.

What's still ahead inside 304:
- Reading inputs from per-port slots (peek vs pop based on
  compile-time wire classification — that classification work
  itself is the deferred size-class step in 305).
- Invoking the per-worker spec handle. Needs 303's pool hook in
  place so each worker has its handles populated; right now the
  context's `specs` registry is reachable but worker handles
  aren't routed through the worker context.
- Picking the output branch per `routing.kind` and pushing to
  consumer slots; this is the routing-table dispatch from the
  unified schema in issue 233.
- Spawn-on-input-ready chaining: each push to a consumer slot
  triggers a check, which may spawn the consumer's next task.

### Plain-call dispatch + spawn-on-ready — 2026-05-12

`src/012-dispatch.c` now has a real action body. For each task:
1. Look up the box from the graph.
2. Read every input port via `slot_peek` into a heap buffer.
3. Dispatch on `box->kind`:
   - **BOX_CALL** — resolve the per-worker spec handle via
     `pool_current_worker->handles[box->spec_idx]`, call
     `spec->invoke(handle, ref_path, fn_name, inputs, sizes,
     n_present, out_buf, out_cap, &out_size)`.
   - **BOX_DATA** — open `map_dir/box->path`, read up to
     `out_capacity` bytes into the output buffer.
   - **BOX_FILE_WRITE** — read inputs "path" and "text",
     `fwrite` text to the resolved path.
4. Capture the output for inspection (optional), push to every
   outgoing connection's input slot, and fire
   `dispatch_spawn_if_ready` on each consumer.
5. Emit `task_start` / `task_end` events through the optional
   event queue (issue 311) with monotonic-clock durations.

Three end-to-end fixture maps cover the major paths:

- `tests/maps/calc/` — single Lua call box with two literal
  inputs. `add(17, 25) = "42"`.
- `tests/maps/hello/` — two-box data → Lua pipeline.
  `who → "World"` → `greet → "Hello, World!"`.
- `tests/maps/pipeline/` — five-box multilang chain.
  `data "5" → Lua double "10" → C addone "11" →
  Bash shout "11!" → file_write` writes `11!` to disk.

Four dispatch tests in `tests/012-dispatch-test.c` exercise the
calc fixture (with and without literals), a counter-burst
verifying many concurrent tasks against the same box, and the
skeleton dispatch path.

Still ahead inside 304:
- **Iterator multi-spawn.** Each iterator task should re-spawn
  itself when its input queue still has values; that needs the
  N-cell pop-mode slot shape from 302 and a different
  `box_ever_spawned` rule.
- **Routing kinds beyond plain.** Comparator routes by output
  value; iterator/randomizer/weighted/distributor route by the
  atomic counter or downstream fill. The hooks in `box->routing`
  are populated by 305; only the dispatch branch picker is
  missing.
- **Variable-size payloads via the large-value heap** (issue 302).

### Comparator + iterator routing — 2026-05-12

`push_routed(ctx, b, bytes, size)` now branches on
`b->routing.kind`:
- **PLAIN** — fan to every outgoing connection (existing behavior).
- **COMPARATOR** — parse the output as a double via `strtod`,
  compare against `b->routing.comparand`, push to the
  matching `lt` / `eq` / `gt` connection only.
- **ITERATOR** — `slot_read_inc(ctx->slots, b->counter_slot_id,
  n_outputs)` picks the branch index, then push to the
  `out_<idx>` connection only.
- **RANDOMIZER / WEIGHTED / DISTRIBUTOR** — fall back to plain
  for now; deferred to follow-ons.

A new `push_branch(ctx, b, branch, bytes, size)` helper filters
connections by `from_branch` and pushes to each match (also
firing the spawn-on-input-ready check on each consumer).

The randomizer/weighted/distributor enum values stay reserved
in the routing-kind table; the dispatch falls through to plain
so nothing deadlocks.

Two fixture maps cover the two routing kinds end-to-end:

- `tests/maps/comparator/` — `classify` (lua, comparator,
  `comparand=5`) feeds three sinks `low`/`mid`/`high` via
  `from_branch="lt"/"eq"/"gt"`. With literal `v="8"`, only the
  `high` sink runs and captures `"HIGH:8"`.
- `tests/maps/iter-route/` — `iter` (lua, iterator,
  `n_outputs=3`) feeds three sinks `a`/`b`/`c` via
  `from_branch="out_0"/"out_1"/"out_2"`. First invocation
  (counter=0) routes to `a`; manual `dispatch_spawn` calls then
  walk through `b` and `c` as the counter advances.

Three new tests in `tests/012-dispatch-test.c`: comparator
routing, single-fire iterator routing, and a 3-call round-robin
iterator covering all three branches.

### Task-id correlation — 2026-05-12

`dispatch_task_t` gained a `task_id` field assigned at
`dispatch_spawn` time (atomic fetch-add on
`ctx->next_task_id`). The same id correlates the
`task_submit` / `task_start` / `task_end` events emitted
through the event queue. `task_submit` lands on the spawning
thread (worker_idx = -1 since the task hasn't been picked up
yet); start/end land on the dispatching worker with its real
`thread_idx`.

### Iterator multi-spawn + N-cell pop reads — 2026-05-12

Three pieces together unlock real iteration:

1. **Per-input slot mode.** `box->input_slot_modes[i]` is set by
   `graph_attach_runtime` to either `SLOT_MODE_PEEK` (single
   cell, the legacy path) or `SLOT_MODE_POP` (multi-cell ring;
   each read drains one cell). `read_inputs` picks `slot_pop`
   or `slot_peek` based on the mode.
2. **multi_spawn propagation.** Iterators are marked
   `multi_spawn = 1`; a forward BFS propagates the flag through
   the connection graph so every box reachable from an iterator
   is also `multi_spawn`. Multi-spawn boxes get N-cell pop slots
   (currently 16 cells); single-spawn boxes keep their 1-cell
   peek slots.
3. **Auto-re-spawn.** After a multi-spawn box's action completes
   successfully, it calls `dispatch_spawn` on itself if any POP
   input still has queued values. A single push wakes the entire
   chain — an iterator with three queued inputs walks the counter
   through all three branches without external intervention.

The single-spawn guard now applies only to single-spawn boxes.
Multi-spawn ones spawn on every push; the per-slot atomic-flag
spinlock serializes pops so two concurrent tasks each get a
distinct cell.

`test_iterator_multi_fire` in `tests/012-dispatch-test.c` is the
headline test: push three values to the iterator's input slot,
spawn once, all three branches a/b/c fire with the right tagged
output. The atomic counter walks 0 → 1 → 2 across the
auto-re-spawned tasks.

### Cell-tagged ordering across parallel iterator workers — 2026-05-19

Earlier multi-spawn slot pushes carried `tag = 0`; pop order was
fill order (which is FIFO under the per-slot spinlock but
non-deterministic across producer interleaving).

Tag propagation is now wired through:

- `push_one_connection` / `push_branch` take an explicit `tag`
  argument forwarded to `slot_push`.
- Iterator routing passes the freshly-incremented counter as
  the tag; randomizer and weighted routing pass their counter
  snapshot so any downstream tagged slot still serves in stable
  order; plain/comparator pushes pass `0`.
- `graph_attach_runtime` sets `SLOT_FLAG_TAGGED` on every
  multi-spawn box's input slots. The slot store's tagged-pop
  rule (lowest tag first; FIFO among ties via filled-index
  scan) means that even when two iterator tasks run on
  different workers in opposite order, the downstream consumer
  reads them in invocation order.

Verified by re-running all 6 fixture maps and the dedicated
`test_iterator_multi_fire` test (which now exercises tagged
input slots).
