# 004-executor.lua — Public API

Synchronous FSM executor. Walks the graph and invokes drivers.

## M.execute(graph: table, map_dir: string) -> ok: bool, err: string|nil

Executes a loaded and validated graph. Drives boxes in dependency order
via a ready-queue: a box is enqueued when all its input ports are filled.
Each box invocation calls run_task() — the future thread pool swap point.

On completion writes tmp/last-run.json:
  { ok, error, boxes={ [id]={ inputs, outputs, status, error } } }

Returns true/nil on success, false/error-string on first failure (halts).
