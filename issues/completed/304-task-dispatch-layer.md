# 304 — Task dispatch layer (C, replaces synchronous executor)

## Status
complete — every routing kind ships, every fixture passes
end-to-end, task structs are slab-allocated from the unified
allocator (no per-spawn malloc bookkeeping), and the
`live_wire_count` placeholder is in the struct layout for the
future box-retirement slice.

The structural "attempt-task model rewrite" that was tracked
here was reconsidered (see "Attempt-task rewrite — reconsidered"
below) and walked back. The two perf / scaffolding tweaks that
turned out to be the actually-useful pieces of it landed
without restructuring the dispatch action.

## Current behavior
`src/012-dispatch.c` ships the C dispatch layer. The action body
reads each input port (peek for single-spawn boxes, pop for
multi-spawn ones; dual-ring for cross-language wires), invokes the
resolved spec's `invoke` callback, picks the output branch per
`routing.kind`, and pushes the output bytes to every outgoing
connection's consumer slot. Each push fires `dispatch_spawn_if_ready`
on the destination; consumers with all required inputs ready spawn
as fresh tasks. Read boxes are pulled on demand by consumers, not
spawned (issue 244).

Routing kinds, all implemented:

- `plain` — fan to every outgoing connection.
- `comparator` — compare output to `comparand`, pick `lt`/`eq`/`gt`.
- `iterator` — `slot_read_inc(counter, n_outputs)` picks the
  branch; counter doubles as the per-cell tag so parallel iterator
  tasks land in invocation order downstream.
- `randomizer` — xorshift-mix the counter, modulo n_outputs.
- `weighted` — cumulative-band lookup against counter scaled to a
  fixed precision; ties absorbed by the last band.
- `distributor` — argmin over downstream slot fill levels. Ties
  resolve via counter-rotated iteration so a steady stream
  spreads across equally-empty branches instead of always biasing
  toward index 0.

The synchronous executor at `src/004-executor.lua` is wholly
replaced.

## Concept

The task dispatch layer is the C component every pool worker runs.
Every task in the pool is an **attempt task** — same shape, same
allocation, same call site. An attempt task either advances the
graph by one box invocation (when its consumer's inputs are
ready) or returns without doing anything (when they aren't).

There is no separate "readiness check" task kind. There is no
"work task" kind distinct from a check. There is one kind, the
attempt; the work it performs is conditional on what it finds.

The architecture doc's "structural shell" section frames this:
one allocator per kind of operation, one scheduler (the pool),
one task shape. The dispatch action is the one place the runtime:

- Reads from input ring buffers (or pulls from read-box predecessors)
- Calls the language spec's `invoke`
- Picks the output branch per `routing.kind`
- Pushes the output value into every downstream consumer slot
- Submits attempt tasks for the consumers whose input sets may
  have just become satisfiable

`004-executor.lua` is replaced wholesale. There is no Lua-level
executor in phase 3.

## Task struct (cell layout)

A task struct is a 64-byte slab cell. The layout reserves space
for every field every attempt may need; the fields' meaning is
the same for "this attempt found ready inputs and ran" and "this
attempt didn't find ready inputs and returned." One shape, two
outcomes.

```c
typedef struct {
    void   *next_free;          /* 8B — free-list link, overwritten on alloc */
    box_t  *box;                /* 8B — the box this attempt is for */
    uint32_t task_id;           /* 4B — event-log correlation (311) */
    uint32_t flags;             /* 4B — work / attempt / priority hint */
    void   *output_destinations;/* 8B — per-wire consumer slot pointers */
    void   *input_snapshot;     /* 8B — values OR pointer to values-array */
    uint32_t live_wire_count;   /* 4B — box-refcount slot (244 / retirement) */
    uint32_t _pad;              /* 4B — alignment */
    /* remaining 16B available for inline input snapshot or extensions */
} attempt_task_t;
```

The `live_wire_count` slot is the documented placeholder for the
box-level retirement system. The dispatch action populates it at
creation; the box-retirement consumer that DOES something with
the count is the stretch goal in 302. Until that ships, the slot
is honest but unread — its presence is documentation that the
field belongs in the design.

The `input_snapshot` is the FIFO-popped values from each input
slot's ring buffer (or the pointer to a read-box's value when
pulled per 244). Once the attempt has snapshotted, the slot
state is independent of the task's execution.

The `output_destinations` is the array of consumer slot pointers
the attempt will write to — one per outgoing wire (or one per
firing branch, for routed boxes). Pre-computed at graph load and
referenced through the box pointer.

## The dispatch action — the attempt loop

```
producer task (a completing attempt that ran):
   compute output via spec invoke
   for each downstream consumer box C this producer just touched:
      write all values to C's input slots
         (snapshotted into the attempt's cell at the END)
   for each distinct C:
      slab_alloc() → attempt_task for C
      pool_spawn(attempt_task)
   decrement input $ref counts
   decrement upstream boxes' live_predecessor_count if applicable
   slab_free(this cell)

attempt task (newly picked up by a worker):
   if every required input on C is satisfiable
      (slot value or data-box pull per 244):
      snapshot values into local vars
      invoke spec, get output
      [continue as producer task above]
   slab_free(this cell)
```

Three properties of this loop:

- **Targeted attempts.** A producer knows which consumer boxes it
  just pushed to; it submits attempts for *those specific
  consumers*, not a generic re-evaluation.
- **One attempt per (producer, consumer-box) pair.** Even if a
  producer writes to multiple slots on the same consumer, only
  one attempt is submitted for that consumer. Dedup happens at
  the end of the producer's run, after every output value has
  been placed. The attempt is guaranteed to find a coherent
  input set, not a half-written one.
- **No parking.** An attempt that finds its inputs unsatisfied
  returns. The shell does not hold tasks in waiting states. The
  next producer push to any of the consumer's input slots
  triggers another attempt with the same shape. Eventually one
  of those attempts finds the input set complete and runs the
  work.

The attempt task's "if inputs ready, run" branch IS the
inline-fast-path for same-language chains. There is no separate
spawn step between the readiness decision and the work; the work
just happens, in the same allocation, on the same worker. A
producer pushing into a fast same-language consumer fires an
attempt that runs immediately on whichever worker picks it up —
one allocation, one queue round-trip per box invocation.

## Initial submission

At startup the pool runner walks the graph and submits an attempt
task for each **entry box** (a box whose required inputs are all
satisfiable from literal values or read-box predecessors, i.e.
whose attempt would succeed on first try). Read boxes do NOT
push at startup under 244 — they're pulled on demand from inside
attempts that find their slots empty and their port has a
read-box predecessor.

Iterator counter slots start at 0. Literal-only entry boxes
submit their first attempt at startup and increment from there.

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

## Attempt-task rewrite — reconsidered

The "attempt-task model" rewrite (originally drafted 2026-05-20,
preserved in the historical log below) framed itself as
"replacing the two-phase dispatch with a unified attempt-task
shape — one allocation per invocation, one loop body, no
separate readiness check." On inspection the current code
already has one task shape (`dispatch_task_t`), one allocation
path, and the readiness check is just a few lines of C in the
producer's epilogue — not a separate task kind. The rewrite
was framing a few lines of inline logic as a structural unit
and proposing to "move" them; the "from A to B" wasn't real.

The two pieces of the rewrite that *were* useful, restated and
shipped:

1. **Slab allocation of task structs.** `dispatch_spawn` now
   pulls task cells from the unified allocator (`ua_alloc`)
   instead of `malloc`-ing per spawn. `dispatch_action` releases
   via `ua_unref` paired with a `chunk` back-reference on the
   task itself. Hot graphs no longer pay per-call malloc
   bookkeeping; the free-list amortises spawn/free pairs across
   the run. ~10 lines of code; no semantic change.

2. **`live_wire_count` field placeholder** on `dispatch_task_t`,
   initialised to 0 by `dispatch_spawn` and otherwise unread for
   now. The box-retirement consumer that *does* something with
   the count is the lifetime-tracking slice — it lands when a
   future feature needs it (run statistics, GC of unused
   producer outputs, etc.). The field is in the layout so a
   future slice can wire up the read without changing the
   struct shape callers depend on.

What was *not* shipped — and would be a mistake to ship without
a concrete trigger:

- Moving the readiness check from the producer's epilogue to a
  consumer-side attempt prologue. The epilogue check is the
  honest signal mechanism: producer writes inputs, checks
  "consumer ready?", submits iff yes. The attempt-side check
  would submit attempts that find inputs not ready and return —
  strictly more work for the same outcome.
- Restructuring `dispatch_action` into a single attempt-loop
  body. The action already has one loop body; the "two-phase"
  framing was paper.

If the same architectural pressure surfaces again — usually
phrased as "tasks have two meanings" or "we should unify the
shape" — the answer is to trace the runtime, not the issue
body. Two meanings on paper, one shape in code, is fine.

### Design rewrite to the attempt-task model — 2026-05-20

What changed in the design (not yet in the code):

- The two-phase model (producer's dispatch action calls
  `dispatch_spawn_if_ready` inline on each consumer; if ready,
  spawns a fresh `dispatch_task_t` for the consumer) is replaced
  by the **attempt-task model**. Every task in the pool is an
  attempt; the work happens conditionally when the attempt finds
  inputs ready. One allocation per invocation, not two.
- The task struct grows from `{ box_id }` to the 64-byte cell
  layout above (free-list link, box pointer, task_id, flags,
  input snapshot, output destinations, live_wire_count
  placeholder).
- Read-box producers stop pushing at startup; they become
  pull-on-demand sources per issue 244.
- The "no parking" rule from the structural-shell doc holds: an
  attempt that finds its inputs unsatisfied returns and is
  collected; the next push to the consumer triggers a fresh
  attempt.

What's still in the code from the older model:

- The `dispatch_action` body in `src/012-dispatch.c` is the
  two-phase shape: read inputs → invoke → push → inline
  `dispatch_spawn_if_ready` per consumer. Plain, comparator, and
  iterator routing all work under this shape.
- `dispatch_task_t` is still `{ box_id, task_id }` plus a
  context pointer.
- Read boxes still push at startup.

The refactor lands with issue 302's slab-allocator + LVH
refcount work, since the task-cell layout and the refcount
plumbing share the same allocator changes. After 302 ships:

1. `src/012-dispatch.c::dispatch_action` becomes the single
   attempt-loop body.
2. Task allocation moves from per-spawn malloc to slab pop.
3. Spawn logic moves from "producer inlines check" to "producer
   submits attempts" — one submission per distinct downstream
   consumer box, at the END of the producer.
4. Read-box predecessors are recognized at attempt time and
   pulled round-robin per 244.
5. The `live_wire_count` field on the cell is populated by
   graph load; the box-retirement consumer is the stretch
   piece that may or may not ship with 302.
