# 104 — Runner: synchronous executor (task boundary)

## Status

open

## Blockers

- 103 (graph must be loaded and validated before execution)
- 102 (drivers must exist to be invoked)

## Current behavior

No executor exists. There is no mechanism to walk the graph and invoke
boxes in dependency order.

## Intended behavior

soramech-runner.lua, after a successful load/validate, executes the map:

  1. Start at entry box
  2. For each box ready to fire (all input ports satisfied):
     a. Collect input values from wired predecessor outputs
     b. Invoke driver: look up extension in driver table, call driver
        script with file + fn + args
     c. Parse JSON from driver stdout into output values
     d. Store output values keyed by (box_id, output_name)
     e. For each outgoing connection from this box:
        - If predicate present (branch box): evaluate predicate against
          the output value; if true, mark target box's input as satisfied
        - If no predicate: mark target box's input as satisfied
     f. Enqueue newly-satisfied boxes
  3. Continue until no boxes remain in the queue
  4. Write tmp/last-run.json with all box outputs and exit status

Each box invocation is wrapped in a task_fn(inputs) -> outputs function.
This boundary is the future threading integration point. In v1 the task
function runs synchronously in the main coroutine; in a later phase the
same function signature is handed to the thread pool or effil-jit.

Error handling: if a driver exits non-zero, the runner logs the box id,
the driver's stderr, and the exit code to tmp/logs/, then halts. No
fallback, no skip, no continue-on-error.

## Suggested implementation steps

1. Write src/004-executor.lua — public function:
   - execute(graph, map_dir) -> ok, err
   Internally uses a ready-queue (lua table as FIFO).
2. The task boundary: define a local run_task(box, inputs, driver_table)
   function that invokes the driver and returns {outputs, ok, err}.
   This function is the swap point for threading later.
3. Predicate evaluation: a small local function eval_predicate(pred, value)
   handles op: "eq", "lt", "gt", "lte", "gte", "contains", "matches".
   "matches" is a lua pattern match. No arbitrary lua eval — structured
   predicates only.
4. Write tmp/last-run.json on completion:
   { ok=bool, boxes={ [id]={ inputs={}, outputs={}, status="ok"|"error" } } }
5. Write tests/002-executor-test.lua — runs maps/hello/ end-to-end, asserts
   expected output values in last-run.json.

## Related documents

- docs/001-architecture.md — execution model section
- docs/003-driver-system.md — driver invocation contract
- issues/103 — graph loader (graph argument comes from here)
- issues/108 — branch box predicate evaluation expanded

## Notes

The ready-queue approach (enqueue boxes as their inputs are satisfied)
is the natural path to concurrent execution later: in a threaded executor,
the enqueue step becomes a pool_spawn call instead of a direct function
call. The box output store (keyed by box_id + output_name) becomes shared
memory guarded by the pool's result-slot mechanism.

Do not use os.execute or io.popen with complex shell pipelines for driver
invocation. Build the command as a table of arguments, invoke via
io.popen with a single quoted command string. If a more robust approach
is needed, write a small helper that builds the invocation string safely.
