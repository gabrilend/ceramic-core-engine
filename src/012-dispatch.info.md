# 012-dispatch.c — public surface

Phase 3 dispatch layer: the C function the thread pool runs per
task, and everything a task needs around it. One task is one box
fire. The action reads the box's inputs — honoring each port's
input method, consumed-on-use or referenced-on-use — invokes the
box (through its language spec for call boxes; read, write, and
map kinds are handled directly), then routes the single output
along the box's routing kind, pushing into downstream slots and
spawning any consumer the push made ready. Iterator-style boxes
re-spawn themselves while their consuming inputs still hold
queued deliveries.

## API

- `int dispatch_ctx_init(...)` — wire a context over graph, slot
  store, spec registry, pool, and (optionally) the event queue.
- `void dispatch_ctx_destroy(ctx)` — release per-box runtime state.
- `int dispatch_push_literals(ctx, &err)` — the startup pass:
  deliver every typed-in literal to its port exactly once.
  Referenced ports keep that value forever; consuming ports treat
  it as the first delivery.
- `void dispatch_spawn_if_ready(ctx, box_id, priority)` —
  readiness-gated spawn. Single-fire boxes pass through an atomic
  once-only guard; multi-fire boxes spawn once per arriving push;
  read boxes never spawn (they are pull-on-demand value sources).
- `void dispatch_spawn(ctx, box_id, priority)` — unconditional
  spawn, bypassing guard and readiness. Used by tests and the
  iterator re-spawn path.
- `void dispatch_action(arg)` — the pool action: read inputs,
  invoke, route, push, re-spawn.
- `const char *dispatch_captured_output(ctx, box_id, &size)` —
  the box's last output, when output capture is enabled
  (tests / the CLI summary).

## Types

- `dispatch_ctx_t` — graph + slots + registry + pool + events,
  plus two diagnostic atomics: `tasks_dispatched` (every action
  run) and `tasks_failed` (actions whose box invoke failed).
  Tests assert `tasks_failed` stays zero — a failure that only
  stderr sees can hide inside a passing test for months, which is
  how bug 324's cousin stayed invisible in the burst test.
- `dispatch_task_t` — one queued fire: box index, back-pointer to
  the context, task id, and the allocator chunk it rides in.

## Related

- Issue 304 — design; issue 305 — the graph the action walks.
- Issues 312 / 323 / 324 — per-edge formats and the two input
  methods the read path honors.
- `src/009-slot-store.info.md` — the peek / pop cell semantics
  underneath.
