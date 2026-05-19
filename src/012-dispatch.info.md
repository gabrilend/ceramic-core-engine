# 012-dispatch.c — public surface

Phase 3 dispatch layer. The C function the thread pool runs per
task. Currently a skeleton: every spawned task lands in
`dispatch_action`, increments the context's `tasks_dispatched`
counter, and returns. Real input-reading, spec invocation, and
output-pushing land in follow-on iterations.

## API

- `void dispatch_spawn(const dispatch_ctx_t *ctx, int box_id, int priority)`
  — allocate a task for `box_id`, submit to the pool.
- `void dispatch_action(void *arg)` — pool action signature;
  receives `dispatch_task_t *` cast as `void *`.

## Types

- `dispatch_ctx_t` — graph + slots + registry + pool. Plus a
  diagnostic `tasks_dispatched` atomic.
- `dispatch_task_t` — `{ int box_id; const dispatch_ctx_t *ctx }`.

## Related

- Issue 304 — design.
- Issue 305 — graph the action walks.
- Issue 302 — slot store the action reads/writes.
- Issue 303 — spec registry the action invokes through.
- Issue 301 — pool the action is submitted to.
