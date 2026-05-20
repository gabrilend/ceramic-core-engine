# 310 — Priority queue: wired in, no-op behaviorally

## Status
complete (2026-05-20) — `pool_spawn` carries a priority argument,
the pool's singly-linked queue orders by descending priority with
FIFO tiebreak, and existing call sites all submit at priority 0.
Behaviorally a single-priority FIFO until a real use case raises
`SM_MAX_PRIORITY`; the mechanism is in place so future hooks land
as a constant change, not a signature change.

## Current behavior
The SoraMech thread pool's `pool_spawn` (modeled on 3d-rts's API)
takes a priority parameter:

```c
task_id_t pool_spawn(task_pool_t *pool,
                     action_fn_t  *actions,
                     void        **action_args,
                     int           n_actions,
                     int           priority);
```

Higher priority tasks are dispatched ahead of lower priority tasks
when multiple are ready. The pool's queue handles the ordering.

Phase 3 needs to specify what SoraMech does with this knob. The
simplest answer is the right one for now: do nothing.

## Decision

Every dispatch task is submitted at priority `1`. The compile-time
constant `MAX_PRIORITY` is set to `1`. There is no graph-level or
box-level concept of priority. The runner does not expose any user-
facing knob for it.

The mechanism is wired through the codebase — every `pool_spawn`
call passes a priority, the constant is referenced rather than
hard-coded inline — but the behavior is identical to a single-
priority pool. Tasks run in submission order (modulo whatever
ordering the pool guarantees within a single priority level).

## Why wire it in if it does nothing

Two reasons:

1. **Future hooks land as config changes, not refactors.** When a
   real priority use case appears (allocator cleanup at lowest
   priority; user-marked urgent boxes; iterator queue draining
   prioritized over new entry submission), raising `MAX_PRIORITY`
   and emitting different priorities at the right call sites is a
   line edit, not a structural change. If the priority parameter
   were stripped out, adding it later means going back through every
   `pool_spawn` callsite and threading a new argument.

2. **The pool's API requires it.** `pool_spawn` takes a priority.
   Callers must pass something. Passing `1` consistently is the
   straightforward expression of "no priority concept yet."

## Specific call sites

```c
// src/dispatch.c — every dispatch task
pool_spawn(pool, &dispatch_action, &task_arg, 1, SM_PRIORITY_DEFAULT);

// src/008-pool-runner.c — entry-box submission at startup
pool_spawn(pool, &dispatch_action, &task_arg, 1, SM_PRIORITY_DEFAULT);

// src/slot-store.c — deferred allocator cleanup task (issue 302)
pool_spawn(pool, &cleanup_action, &cleanup_arg, 1, SM_PRIORITY_DEFAULT);

// langs/bash/spec.c — ... none, the spec doesn't spawn pool tasks
```

`SM_PRIORITY_DEFAULT` is `1`. Defined in `src/008-pool-runner.h` (or
a sibling header) so all callers use the same name.

## Future hooks (not in scope, but called out)

When `MAX_PRIORITY` is raised, the natural assignments are:

| Future use case                                     | Priority |
|-----------------------------------------------------|----------|
| User-marked "urgent" boxes (UI / interactive)       | 3        |
| Iterator continuation when its queue is non-empty   | 2        |
| Default dispatch task                               | 1        |
| Allocator cleanup task (deferred coalescing, 302)   | 0        |

These are speculative. The actual priority schema is decided when
there is a workload that demonstrates the need.

A box-level priority field could go in the box JSON (`"priority":
2`) and be plumbed through `dispatch_task_t`. The dispatch action
reads it, passes it to `pool_spawn` for any successor tasks. This
is a future change.

## Open questions

(none currently — this is intentionally a small change to the call
sites. The design conversation about what priorities should mean is
deferred until concrete need.)

## Suggested implementation sequence

1. Define `SM_PRIORITY_DEFAULT` (= 1) and `SM_MAX_PRIORITY` (= 1)
   in a runner-wide header.
2. Make every `pool_spawn` call site pass `SM_PRIORITY_DEFAULT`.
3. Set the pool's max priority at `pool_create` time using
   `SM_MAX_PRIORITY` (if the pool API takes a max-priority hint).
4. That's it. No tests required beyond verifying the constants
   compile and the pool runs.

## Relevant files

- `src/008-pool-runner.c` — calls `pool_spawn` for entry boxes
- `src/dispatch.c` — calls `pool_spawn` for successor tasks
- `src/slot-store.c` — calls `pool_spawn` for deferred cleanup
- `libs/task-pool/900-task-pool.h` — `pool_spawn` priority parameter
- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
- `issues/304-task-dispatch-layer.md` — dispatch task submission
- `issues/302-wire-value-slot-store.md` — deferred cleanup tasks

## Implementation log

### API + ordering — 2026-05-12

`pool_spawn` gained a fourth argument: `int priority`. Higher
priorities dequeue first; ties between equal priorities resolve
to FIFO. Linear-scan insertion (strict less-than at the
comparison) on the singly-linked queue — small queue, O(n) walk
is fine. A unit test in `tests/301-pool-test.c` queues six tasks
of varied priority before the init barrier releases the
single-worker pool, and verifies the execution order matches the
priority/FIFO rule.

Existing call sites all pass priority 0; behaviorally the queue
is still a plain FIFO. 304's dispatch action can grow into
priority-aware scheduling without another signature change.
