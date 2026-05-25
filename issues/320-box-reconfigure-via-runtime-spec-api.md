# 320 — Box reconfigure via the runtime spec API

## Status
open · design settled (consolidated through several discussion
rounds), no code landed yet

## Current behavior

A box's identity is frozen at graph-load. The loader parses
`boxes/<id>.json` once, fills in every field — kind, language,
function pointer, input port list, routing shape, outgoing
connections — and the dispatch layer reads that record on every
fire. Nothing in the running program changes the box's record.

The two ways the runtime already has to "modify the graph while
running" — the `create_box` and `connect` primitives from issue
319, and the load-time encapsulation splice from issue 248 —
both **add** to the graph. Neither one **changes** a box that
already exists.

This issue introduces the third path: a running box can be
replaced wholesale via a runtime API. Anyone holding a reference
to the runtime can call `runtime_apply_box_spec(target_id,
spec_json)` and atomically swap a box's identity. The same API
covers create (target id is new), reconfigure (target id
exists), and delete (spec asks for deletion).

## Concept in one paragraph

Every box has a tiny atomic flag and a small per-box control
queue. A reconfigure request arrives as a fully-parsed
replacement box struct, pushed onto the target's control queue.
When a worker arrives at the box to fire it, the worker first
tries to claim the flag (CAS 0 → 1); if it wins, it checks the
control queue. A non-empty queue means there's a pending
mutation — the worker becomes the mutator, pops the replacement
struct, atomically swaps the graph slot pointer from old to new,
releases the flag, and yields-and-requeues. An empty control
queue means the worker proceeds with a normal fire under the
gather flag. Either way: a worker visit is exactly one of fire
or mutate, never both. The flag holds for microseconds; in-flight
fires don't take it. Old boxes drift toward freedom via a
reference count once the graph slot stops pointing at them.

## The five structural pieces

### 1. The gather flag

Each box record carries one atomic int — the gather flag — with
two states:

- **0 (free):** any worker may attempt to enter.
- **1 (held):** exactly one worker is currently in the gather
  section.

Workers entering a box do a CAS from 0 to 1. If the CAS wins,
the worker enters the gather section. If it loses, the worker
yields-and-requeues — its task goes back to the tail of the
pool queue, the pool picks it up later. No spinning, no
blocking.

The gather section is brief: peek a few input ports' ordering
rings, decide fire-or-mutate, pop the appropriate cells, release
the flag. Microseconds. Workers on DIFFERENT boxes don't contend
— the flag is per-box.

### 2. The control queue

Every box has one slot dedicated to incoming control messages,
allocated from the slot store at box-creation time. Cell capacity
is sizeof(void*); cell count is small (8 is plenty). The cells
hold pointers to pre-built replacement box structs.

The control queue is structurally identical to any other slot
the dispatch uses — same ring-buffer primitive, same per-slot
spinlock for concurrent producers. The only thing that
distinguishes it is what's stored in the cells (pointers, not
value bytes) and who reads it (the mutator inside the gather
section, not the spec's invoke).

### 3. The graph slot as liveness indicator

The graph stores boxes as an array of pointers, one per box id.
Worker lookup goes: `graph_box(graph, id) → returns the current
pointer at that index`. The mutator's job during a reconfigure
is to atomically swap that pointer from the old box's address
to the new box's address. Any worker that LATER looks up the
same id gets the new box; any worker that already grabbed the
old pointer keeps it for the duration of its fire.

A box is "live" when the graph slot points at it. A box is
"superseded" when the graph slot points at something else (or at
NULL, for the delete case).

The two-state flag doesn't need a third "redirect" state because
the graph slot itself is what new arrivals consult. Workers
arriving fresh get the current box via the graph slot; workers
mid-fire on the old box finish on the old box. Both work cleanly.

### 4. The reference count

Each box record has an atomic refcount. Workers increment it at
the top of their fire (right after resolving the pointer through
the graph slot) and decrement at the bottom. The mutator
increments it when it becomes the mutator and decrements after
the swap.

A box is freeable when BOTH:
- The graph slot doesn't point at it (it's superseded), AND
- The refcount has dropped to zero.

The worker whose decrement returns 0 checks both conditions. If
both hold, that worker frees the old box. If only refcount = 0
but graph slot still points at the box, the box is just
transiently idle — leave it alone.

The mutator's "hold a +1 during the reconfigure" discipline
ensures the refcount can only fall to zero AFTER the mutator
has finished its work. Without that, a race could decrement to
zero while the mutator is still mid-swap.

### 5. The replacement box struct

A reconfigure message IS a fully-built box struct, sitting in
memory. The caller of `runtime_apply_box_spec` is responsible
for parsing the spec JSON into a `box_t` struct in their own
thread. Once built, the caller pushes the struct's pointer to
the target's control queue.

This means the mutator's hot path doesn't include JSON parsing.
The mutator pops a pointer, does slot resolution (see below),
and atomically swaps the graph slot. The slow work happens in
producer threads, in parallel with anything else going on.

## The worker arrival flow

When a worker is handed a task to fire box X:

```
1. look up the graph slot for X → get current box pointer (call it B).
2. CAS the gather flag on B from 0 to 1.
   - lose: yarq this task. exit.
   - win: continue.
3. increment B's refcount.
4. peek B's control queue.
   - non-empty: become MUTATOR (see below).
   - empty: continue to gather inputs.
5. for each input port i:
     pop one ordering message from input slot i.
     pop the corresponding value from the indicated data ring (native or json).
6. release B's gather flag (set to 0).
7. call the spec's invoke with the gathered values.
8. push outputs to downstream slots per B's connections.
9. decrement B's refcount.
   - if refcount == 0 AND graph slot != B: free B.
```

Important: pop is committed only AFTER the gather flag is held
and the control queue is checked. There's no race window where
a popped value could be lost.

## The mutator flow

When a worker enters the gather section, finds the control queue
non-empty, and becomes the mutator:

```
1. pop one box-pointer (call it NEW) from the control queue.
2. resolve NEW's input_slot_ids by port-name matching against OLD:
     for each port in NEW:
       if same name exists in OLD: copy that slot id into NEW.
       else: allocate a fresh slot from the slot store; record the new id in NEW.
3. record OLD's "abandoned slots" = ports in OLD but not in NEW.
   these slots get released when OLD is freed; any remaining
   queued values emit a discard JSONL event.
4. NEW gets its own freshly-allocated control queue slot.
5. atomically swap the graph slot for the box id from OLD to NEW.
6. release B's (= OLD's) gather flag.
7. yarq this task. (a fresh task will arrive later, find NEW
   via the graph slot, and fire under the new shape.)
8. decrement OLD's refcount.
   - if refcount == 0: free OLD (along with its abandoned slots).
```

Step 7 is non-obvious but load-bearing: the values that were
queued on OLD's input slots are still there (the slot store
didn't change), but the worker that triggered the mutation
didn't fire. Without an explicit re-queue, the queued values
would sit unprocessed until the next producer push. Yarqing
guarantees a fresh task gets scheduled so the queued values
actually get fired under the new shape.

## Slot diffing by port name

The slot resolution in mutator step 2 is the same algorithm used
for tree diffing in UI frameworks: match by stable identifier,
ports-only-in-old get abandoned, ports-only-in-new get freshly
allocated, ports-in-both keep their slot id (and therefore keep
all their queued values).

Renames are NOT detected automatically — a port renamed from
"x" to "x_renamed" looks like "delete x, create x_renamed" from
the algorithm's perspective. Any values queued on "x" get
dropped to the JSONL discard event when OLD is freed. If a
future use case calls for value-preserving renames, the spec
JSON can grow a `"rename": [{"from":"x", "to":"x_renamed"}]`
field; for now, rename-as-delete-plus-add is the conservative
default.

## The unified runtime API

One entry point handles create, reconfigure, and delete:

```c
int runtime_apply_box_spec(const char *target_id,
                           const char *spec_json, int spec_json_len,
                           char **err_out);
```

The runtime parses `spec_json` in the caller's thread, validates
it, builds the replacement box struct, and dispatches based on
the spec's content:

- **Spec describes a new box; target_id is not in the graph:**
  install at a fresh graph slot. Equivalent to today's
  `runtime_create_box`.
- **Spec describes a box; target_id already exists:** push the
  new struct's pointer onto the target's control queue. The
  next worker that visits the target becomes the mutator and
  swaps.
- **Spec is `{"action":"delete", "id":"target_id"}`:** install
  NULL at the graph slot via the same supersession path. Old box
  drifts toward freedom via refcount.

The same shape works for any language. Lua's `soramech.apply_box`
serializes the spec to JSON and calls in. Bash's
`soramech_apply_box` is the same. The C bridge wraps the API
directly. Adding a fourth language only requires the language
to format JSON, which every language can do.

## Reference counting policy — what gets freed when

- **Box record:** freed when refcount = 0 AND graph slot ≠ this
  box. Triggered by whoever decrements to 0.
- **Slots used by both OLD and NEW:** kept alive (NEW still
  references them).
- **Slots in OLD but not in NEW (abandoned):** freed at the
  same time OLD is freed; their remaining queued values get a
  JSONL discard event per slot.
- **The control queue's cells (pointers to replacement structs):**
  if a producer pushed a replacement struct and the box was
  deleted before any mutator ran, those pending replacement
  structs are freed when the control queue is freed (which
  happens when OLD is freed).
- **Live boxes that nobody uses (idle):** not freed. The graph
  slot still points at them; they're "live, just waiting." This
  is intentional — we cannot prove a box won't run again in a
  system that allows runtime creation and connection, so we
  don't try. Users who want to actively clean up call
  `runtime_apply_box_spec(id, {"action":"delete"})`.

## JSONL events

- `box_reconfigure` — fires after the swap. Carries the box id,
  the new spec (re-serialised from the post-apply struct so the
  log records what actually applied, not what was requested),
  task id, worker idx. Always emitted.
- `reconfigure_discard` — fires per abandoned port when a slot
  has leftover queued values. Carries the box id, the port name,
  the count of dropped values, and the first value bytes (up to
  256, for debugging). Always emitted.
- `box_delete` — fires after a delete supersedes a box. Carries
  the box id, task id, worker idx. Always emitted.

## Suggested implementation steps

1. **Add the gather flag and refcount to box_t.** Two atomic
   ints, alongside the existing connections + n_connections
   atomics in `src/010-graph-loader.h`.
2. **Convert the graph's box storage from array-of-structs to
   array-of-pointers.** `graph_box(g, i)` becomes a load from
   the array. The graph still owns the storage; the array
   entries become atomic pointers so the mutator can swap them.
3. **Add the per-box control slot.** At box-creation time
   (graph_load AND runtime_create_box), allocate one extra slot
   per box, cell capacity = sizeof(void*), cell count = 8,
   reachable via a new `box->control_slot_id` field.
4. **Refactor `parse_box_file` to accept an in-memory JSON
   buffer.** The file-path entry point becomes a thin wrapper.
   The runtime API uses the buffer entry directly.
5. **Add the gather lock acquire / release helpers in
   `src/012-dispatch.c`.** CAS 0 → 1 for acquire; store 0 for
   release. Yield-and-requeue helper for the CAS-loss path:
   push the task back to the pool's tail and return without
   firing.
6. **Wrap `dispatch_action`'s top-of-fire path with the gather
   lock acquire.** On loss, yarq. On win, check the control
   queue before doing the normal input gather.
7. **Implement the mutator path inside `dispatch_action`.** Pop
   the replacement struct from the control queue. Run the slot
   resolution. Atomic-swap the graph slot pointer. Release the
   gather flag. Yarq.
8. **Add the unified `runtime_apply_box_spec` to
   `src/018-runtime-builtins.{c,h}`.** Parse JSON, build the
   replacement struct, push the pointer to the target's control
   queue. Same shape as the existing runtime_create_box /
   runtime_connect entrypoints.
9. **Update the language bridges (Lua, C, Bash) to expose
   `soramech.apply_box(target_id, spec)`.** Each bridge
   serialises its native spec representation to JSON and calls
   the runtime API.
10. **Add the JSONL events** (`box_reconfigure`,
    `reconfigure_discard`, `box_delete`) to
    `src/013-jsonl-events.{c,h}` and
    `src/014-event-queue.{c,h}`.
11. **Implement reference-count-driven box GC.** Whoever
    decrements to zero AND observes the graph slot doesn't
    point at this box does the free.
12. **Fixtures**:
    - `tests/maps/320-reconfigure-fn` — same lang, different
      function on the same box. Fires once with old fn, then
      reconfigures, then fires again with new fn.
    - `tests/maps/320-reconfigure-port-shape` — add a new
      input port via reconfigure. Verifies new slot allocated
      and slot diffing matches by name.
    - `tests/maps/320-reconfigure-kind` — flip a BOX_CALL to a
      BOX_WRITE via reconfigure. Subsequent fires write to disk.
    - `tests/maps/320-reconfigure-delete` — reconfigure with
      delete spec; box is gone, downstream still works (or
      fails gracefully if it depended on the deleted box).
    - `tests/maps/320-reconfigure-contended` — two reconfigure
      pushers race on the same target. Both land in the control
      queue; both apply in arrival order; final state reflects
      the last one applied.

## Relevant files

- `src/009-slot-store.{c,h}` — already provides the ring-buffer
  primitive the control queue reuses.
- `src/010-graph-loader.{c,h}` — gather flag + refcount fields
  on `box_t`; `parse_box_file` refactor; graph array → array
  of pointers; control slot allocation at box creation.
- `src/012-dispatch.c` — gather lock acquire/release wrapping
  `dispatch_action`; mutator path; yarq helper.
- `src/018-runtime-builtins.{c,h}` — `runtime_apply_box_spec`
  unified entry point.
- `src/013-jsonl-events.{c,h}` / `src/014-event-queue.{c,h}` —
  three new event types.
- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` —
  one new bridge function per language for `apply_box`.

## Open issues for after 320 lands

- **Connections-array atomic-publish dance unification** —
  today's `runtime_connect` does its own copy-and-publish on
  the per-box connections array. Once 320 lands, that pattern
  becomes redundant: a connection add can ride the same
  box-swap mechanism. Tracked as a separate issue.
- **Explicit value-preserving rename declaration in the spec
  JSON** — currently rename-as-delete-plus-add is the default.
  If a use case appears, add a `"rename": [...]` field that the
  slot resolution honours.
- **Tunable control queue depth** — currently 8 cells per box.
  If bursty reconfigure traffic ever exceeds this, push back
  to producers via the slot's existing fill-management.

## Design history

The shape of this issue evolved across many discussion rounds.
Two prior shapes were considered and discarded:

1. **Per-port third ring** — reconfigure messages would have
   ridden a third ring on every input slot, with the per-cell
   ordering tag growing a RECONFIGURE value. Workers would peek
   each port's ordering ring and apply reconfigures encountered
   during gather. Discarded in favour of the per-box control
   queue: control messages aren't conceptually input values,
   and a per-box channel handles empty-input boxes cleanly.
2. **In-place field rewrites with three-state flag (0 / 1 / 2)**
   — the mutator would have rewritten the live box's fields in
   place, with flag = 2 acting as a "redirect to new box"
   indicator. Discarded in favour of the build-new-and-swap
   pattern: the graph slot pointer naturally acts as the
   liveness indicator, the third flag state was redundant, and
   build-new avoids any "torn read" possibility because the old
   box is never modified.

The YARQ name (yield-and-requeue) came from the user's
phrasing: "it's essentially 'hey we're not ready yet, put
yourself back at the beginning of the queue please.'" The
mechanism is cooperative with the scheduler rather than
adversarial — the scheduler keeps doing what it does best
(managing many small units of work fairly), and the barrier
just nudges contended workers to circle back later.

The unification of create, reconfigure, and delete into one API
came from the user's observation: "What if we made the
'create-a-box' box take the same type of struct as the
reconfigure process? And if it has the same box ID as an
already existing box, it just goes through the reconfigure
process." Structurally honest about what these operations are:
all three are "express what this box should be now," and the
runtime decides how to land that expression.
