# 420 — Box reconfigure via the utility-box kind

## Status
open · phase 4 · sub-issue of [419](419-runtime-graph-mutation.md).
Design substantially settled; a few naming / mechanical details
still need a fresh-morning pass (see "Open items for later" at
the bottom). Renumbered from 320 when runtime graph mutation
moved to phase 4 as part of the phase-3 release-candidate
rollback.

## Current behavior

A box's identity is frozen at graph-load. The loader parses
`boxes/<id>.json` once, fills in every field — kind, language,
function pointer, input port list, routing shape, outgoing
connections — and the dispatch layer reads that record on every
fire. Nothing in the running program changes a box's record.

Issue 319f already added a language-agnostic way to **create**
new boxes at runtime: the BOX_CREATE_BOX kind. A box of that
kind takes a JSON spec on a `spec` input port; when it fires, it
parses the spec, allocates the new box at a fresh graph slot,
and emits the new id as a string on its output wire. Downstream
consumers can take the id and wire the new box up further (via
BOX_CONNECT).

This issue extends that pattern to **reconfigure** and
**delete** — also via a box kind, not via library functions in
the language bridges. The new mechanism is the utility-box: a
single box kind that subsumes create / reconfigure / delete by
inspecting the spec it receives.

## Architectural ground rules (from the design discussion)

1. **Graph mutation is a box-kind operation, not a language-
   bridge function.** Users express mutations by placing
   utility-boxes in their maps and wiring them, not by writing
   code that imports the runtime as a library. The runtime is
   the substrate; users express through it.
2. **Manual JSON construction is supported but not assisted.**
   A user who wants to build a long-running Lua box that
   accumulates JSON for its own future reconfigure can do so —
   the manual path works as long as the bytes end up on a wire
   that feeds a utility-box. We don't provide convenience
   helpers for this path; the box-and-wire path is the intended
   one.
3. **The wire endpoint is the target.** When a utility-box's
   output wire points at box X's input port, that wire IS the
   declaration of "this utility-box operates on box X." No
   separate target_id input.
4. **One unified box kind.** create / reconfigure / delete are
   all the same kind, dispatched by the spec's content. Folds
   in 319f's BOX_CREATE_BOX.
5. **Reconfigures ride the regular input slots, tagged with
   RING_RECONFIGURE.** No separate control queue. The gather
   lock makes peek-then-pop honest within the lock, so the
   peek-vs-pop race that would have plagued an unsynchronised
   design doesn't appear.

## The five structural pieces

### 1. The utility-box kind

One box kind absorbs 319f's BOX_CREATE_BOX. The behaviour
depends on the spec's content:

- **Spec describes a box with no existing target** (or target
  doesn't exist in the graph): CREATE. Allocate at a fresh
  graph slot. Emit the new id as a string on the output wire.
- **Spec describes a box with an existing target id** (the
  wire's endpoint matches an existing box): RECONFIGURE.
  Build a replacement box struct. Push the struct pointer to
  the target box's input slot (whichever port the wire ends
  at), tagged RING_RECONFIGURE.
- **Spec is empty / minimal** (no fields, no function): DELETE
  signal. Push a delete-marker to the target box's input slot,
  tagged RING_DELETE (or maybe just RING_RECONFIGURE with a
  null struct — see open items).

### 2. The gather flag

Each box record carries one atomic int — the gather flag — with
two states:

- **0 (free):** any worker may attempt to enter.
- **1 (held):** exactly one worker is currently in the gather
  section.

Workers entering a box CAS from 0 to 1. Win → enter gather.
Lose → yield-and-requeue (push task back to pool tail and
return). No spinning, no blocking. Per-box, so workers on
different boxes don't contend.

### 3. The graph slot as liveness indicator

The graph stores boxes as an array of pointers, one per box id.
`graph_box(graph, id)` returns the current pointer. The mutator
atomically swaps that pointer from OLD to NEW. Workers that
LATER look up the same id get NEW; workers mid-fire on OLD
finish on OLD.

A box is **live** when the graph slot points at it. **Superseded**
when the graph slot points at something else (or NULL, for
delete). The two-state flag doesn't need a third "redirect"
state because the graph slot itself is the source of truth for
"is this still the current box."

### 4. The reference count

Each box record has an atomic refcount. Workers increment at
the top of their fire (just after resolving the pointer through
the graph slot) and decrement at the bottom. The mutator
increments when it becomes the mutator, decrements after the
swap.

Freeable when BOTH:
- Graph slot doesn't point at this box (superseded), AND
- Refcount has dropped to zero.

Whoever decrements to zero AND observes the graph-slot
mismatch frees the box. Mutator's "hold a +1" discipline
ensures the refcount only reaches zero AFTER the mutator
finishes its work.

### 5. The reconfigure tag on input slots

Each input slot's ordering ring grows a RING_RECONFIGURE value
alongside the existing RING_NATIVE and RING_JSON. When a worker
enters gather and peeks each input port's ordering ring, a
RING_RECONFIGURE tag at any port's head means "this is a
control message, not a value." The cell holds a pointer to a
pre-built box_t struct (built by the utility-box during its
fire, in the utility-box's worker thread — no parsing happens
inside the target's mutator path).

## The worker arrival flow

```
1. look up the graph slot for X → get current box pointer (B).
2. CAS B's gather flag from 0 to 1.
   - lose: yarq this task. exit.
   - win: continue.
3. increment B's refcount.
4. peek each of B's input ports' ordering rings.
5. branch on what's there:
     all heads are values:
        pop one ordering message + value from each port.
        release gather flag.
        call spec's invoke with the gathered values.
        push outputs to downstream slots per B's connections.
        decrement B's refcount; free if it qualifies.

     any head is RING_RECONFIGURE:
        pop the struct pointer from that port.
        run mutator path (see below).

     some ports empty:
        don't pop anything. release gather flag. exit.
        the pool will respawn this box when more values arrive.
```

## The mutator flow

```
1. with B's gather flag held and refcount +1:
2. pop the replacement struct pointer (call it NEW) from the
   RING_RECONFIGURE-tagged cell.
3. resolve NEW's input_slot_ids by port-name matching:
     for each port in NEW:
       if same name exists in B (= OLD): copy that slot id.
       else: allocate a fresh slot.
4. record OLD's abandoned slots (in OLD but not in NEW). They
   get released when OLD is freed; remaining queued values
   emit a discard JSONL event per slot.
5. atomically swap the graph slot for the box id from OLD to NEW.
6. release B's gather flag.
7. yarq this task. (queued values on B's slots — still there,
   slot store didn't change — get fired under the new shape
   when the requeued task runs.)
8. decrement OLD's refcount; free OLD + abandoned slots if 0.
```

The yarq in step 7 is load-bearing — without it, queued values
on B's input slots sit unprocessed until the next producer
push.

## Slot diffing by port name

Same algorithm as UI framework tree diffing: match by stable
identifier (port name), only-in-old gets abandoned, only-in-new
gets freshly allocated, in-both keeps its slot id (and its
queued values).

Renames are NOT auto-detected — "x" → "x_renamed" looks like
"delete x, create x_renamed." Any queued values on "x" get
dropped to JSONL. Value-preserving rename via explicit
`{"rename":[...]}` field is a possible future addition.

## JSONL events

- `box_reconfigure` — fires after the swap. Carries box id,
  post-apply spec (re-serialised from the new struct), task id,
  worker idx. Always emitted.
- `reconfigure_discard` — fires per abandoned port with leftover
  values. Carries box id, port name, count of dropped values,
  first value bytes (up to 256). Always emitted.
- `box_delete` — fires when the spec resolves to a delete.
  Carries box id, task id, worker idx. Always emitted.

## Open items for later (the things to think about with a fresh head)

These are settled enough in shape that implementation can
start, but the exact mechanics want one more pass:

1. **How exactly does "empty spec → delete" detection work?**
   The user's framing: "we essentially clear the box's fields
   one by one until we reach the end and realize 'hey this box
   isn't attached to anything, doesn't have a function, and
   has a refcount of 0 — it's obviously meant to be deleted.'"
   Two possible mechanics:
   - **Wholesale replace then check:** apply the entire new
     spec, then look at the resulting struct. If function
     pointer is null, no connections, no inputs — interpret as
     delete, swap graph slot to NULL instead of NEW.
   - **Explicit DELETE tag:** the utility-box decides up
     front based on spec content and pushes RING_DELETE
     (separate tag from RING_RECONFIGURE) so the mutator
     doesn't have to do the emptiness check after applying.
   Lean toward the explicit tag — keeps the mutator's
   decision simple — but the implicit "fields-equal-zero
   means delete" is closer to the user's stated intuition.

2. **For CREATE specifically: where does the wire endpoint
   point?** Reconfigure and delete both target an existing
   box (the wire endpoint). Create has no existing target —
   the box is born when the spec is applied. Today's
   BOX_CREATE_BOX emits the new id as a string output, and
   downstream consumers receive that string normally. We
   should keep that shape: for create, the output wire
   carries the new id as a value, not a control message.

3. **Should the wire endpoint be the only target indicator?**
   The user leaned both ways in the discussion: "we shouldn't
   take an ID as input at all" AND "lean option B" (content-
   driven by id field). These are in tension. Probably the
   right resolution:
   - For reconfigure/delete: wire endpoint is the only target;
     no id in spec needed.
   - For create: the spec's id field IS required (since
     there's no existing target to wire to). The output wire
     carries the new id.
   - If spec for reconfigure/delete includes an id, it must
     match the wire endpoint's box id (safety check).

4. **Detecting "reconfigure vs create" from the spec / wire.**
   - If the wire endpoint exists → it's a reconfigure or delete
     (decided by spec content emptiness).
   - If there's no wire endpoint (or it points at something
     invalid) → it's a create.
   Mechanically: the utility-box, on fire, looks at where its
   output wire is going. If the endpoint exists, push the
   struct pointer with a control tag. If not, allocate a new
   graph slot, install, emit the new id.

5. **Should the utility-box have a name change?** "utility-
   box" was the user's tentative rename. Alternatives floated:
   "spec-applier", "box-control", "reconfigure-box." Lean
   utility-box because it absorbs three operations.

## Suggested implementation steps (subject to open-item resolution)

1. Add the gather flag and refcount to box_t in
   `src/010-graph-loader.h`.
2. Convert the graph's box storage from array-of-structs to
   array-of-pointers so the mutator can swap.
3. Refactor `parse_box_file` to accept an in-memory JSON
   buffer (the utility-box's invoke will call this on the
   spec it received).
4. Add RING_RECONFIGURE (and possibly RING_DELETE) as
   ordering-ring tag values in `src/009-slot-store.{c,h}`.
   Each input slot's existing ring system grows the new tag
   variants.
5. Wrap `dispatch_action`'s top with the gather-flag CAS;
   yarq on loss, continue on win.
6. Add the mutator path inside `dispatch_action`: pop the
   tagged cell, resolve slot mappings, swap the graph slot,
   release the flag, yarq.
7. Add the utility-box kind in `src/010-graph-loader.{c,h}`
   and `src/012-dispatch.c`. Folds in 319f's BOX_CREATE_BOX
   behaviour for the create path; adds the reconfigure /
   delete paths.
8. Add the JSONL events (box_reconfigure, reconfigure_discard,
   box_delete).
9. Fixtures under `tests/maps/`:
   - `320-utility-create` — regression that the unified
     utility-box still creates (matches 319f's existing
     create test).
   - `320-utility-reconfigure-fn` — fires once with old fn,
     reconfigures, fires again with new fn.
   - `320-utility-reconfigure-port-shape` — adds a port,
     verifies new slot allocated.
   - `320-utility-reconfigure-kind` — flips BOX_CALL to
     BOX_WRITE.
   - `320-utility-delete` — deletes a box; downstream
     gracefully handles the now-gone target.
   - `320-utility-contended` — two utility-boxes race on the
     same target; both reconfigures apply in arrival order.

## Relevant files

- `src/009-slot-store.{c,h}` — extend ordering ring tag with
  RING_RECONFIGURE (and possibly RING_DELETE).
- `src/010-graph-loader.{c,h}` — gather flag + refcount on
  box_t; parse_box_file refactor; graph array → array of
  pointers.
- `src/012-dispatch.c` — gather lock acquire/release; mutator
  path; utility-box kind dispatch.
- `src/013-jsonl-events.{c,h}` / `src/014-event-queue.{c,h}` —
  three new event types.

## Design history

Two prior shapes were considered and discarded:

1. **Per-port third ring with three-state flag (0/1/2)** — the
   original sketch. Discarded once we recognised the graph
   slot pointer can act as the liveness indicator, eliminating
   the third state.
2. **Per-box control queue + language-bridge `apply_box`
   function** — proposed as a way to keep control messages
   separate from data messages. Discarded once the user
   clarified the architectural principle: graph mutation is a
   box-kind operation, not a library function. The control
   queue was a layer mismatch — the existing input slots with
   the RING_RECONFIGURE tag (atomic gather makes peek-then-pop
   safe inside the lock) work fine.

The YARQ name (yield-and-requeue) came from the user's
phrasing: "it's essentially 'hey we're not ready yet, put
yourself back at the beginning of the queue please.'"

The unification of create / reconfigure / delete into one
utility-box kind came from the user's observation: "What if we
made the 'create-a-box' box take the same type of struct as the
reconfigure process?" The architectural principle that drove
the final shape: graph mutation is a box-kind, the wire
endpoint is the target, and the runtime is the substrate, not
the library.
