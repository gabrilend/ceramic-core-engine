# 004-executor.lua — Public API

> **Phase 2 path, superseded.** The runtime that ships is the C
> pool runner; its execution layer is `src/012-dispatch.c` running
> across a thread pool. This executor is single-threaded and
> invokes boxes through the old `drivers/*.sh` scripts rather than
> the `langs/*/spec.so` plugins. It does not implement the routing
> kinds beyond the phase 2 set, and it has no concept of the two
> input methods. Retiring it is an open item on the phase 3
> progress page.

Synchronous FSM executor. Walks the graph and invokes drivers.

## M.execute(graph: table, map_dir: string) -> ok: bool, err: string|nil

Executes a loaded and validated graph. Drives boxes in dependency order
via a ready-queue: a box is enqueued when all its input ports are filled.
Each box invocation calls run_task() — the future thread pool swap point.

On completion writes tmp/last-run.json:
  { ok, error, boxes={ [id]={ inputs, output, status, error } } }

Returns true/nil on success, false/error-string on first failure (halts).
