# 320 — Box reconfigure via third input ring

## Status
open · design draft; a handful of open questions remain but the
shape is settled enough to scope an implementation slice

## Current behavior

A box's identity is frozen at graph-load. The loader parses the
box's JSON once (`parse_box_file` in `src/010-graph-loader.c`),
fills in every field — `kind`, `lang`, `ref`, `fn`, the routing
shape, the input port list, the connections array — and the
dispatch layer (`src/012-dispatch.c`) walks that record on every
fire. Nothing in the running program changes those fields.

The two paths the runtime already has to "change the graph while
it's running" — issue 319's `create_box` / `connect` primitives,
and issue 248's load-time encapsulation splice — both **add** to
the graph; they never **rewrite** a box that already exists. A
box is the box it was born as.

This issue introduces the third path: a running box can be
**reconfigured** mid-run. A new configuration arrives as a
JSON-shaped message inside the normal input stream, on any input
port; the worker that's gathering inputs for the next fire
encounters the reconfigure tag, applies it, and continues
gathering under the new config before firing.

## Concept

Every input slot grows a **third ring** alongside the native +
JSON pair from issue 312. The third ring carries one kind of
message only — a JSON object **exactly matching the box's own
JSON schema**, the same shape a `boxes/<id>.json` file holds at
load. Any input port can carry reconfigure messages; producers
push to whichever port makes sense for their fan-in topology, and
the dispatch layer treats reconfigure cells as a peer to native
and JSON cells.

The per-cell **ring-selector tag** (the bookkeeping that already
tells the consumer "this cell came in on the native ring, or the
JSON ring" — issue 312's dual-ring `which_ring` flag, extended
through issue 318a's per-call ordering) grows a third value:
**reconfigure**. When the consumer's input-gathering walks a port
and sees the next cell tagged `reconfigure`, it doesn't push the
bytes into an input buffer for the box function; it interprets
them as a re-parse of the box's own JSON.

### The mixed-stream scenario

Imagine a box with three input ports under the old config. The
dispatch begins gathering inputs for the next fire and pops cells
in turn:

```
port_a → native value (popped, retained)
port_b → JSON value   (popped, retained)
port_a → native value (popped, retained — second value on a)
port_c → RECONFIGURE
```

The worker:

1. Sees the `reconfigure` tag on `port_c`'s next cell.
2. **Keeps every already-popped value** for ports a / b / a-second
   in worker-local stack buffers.
3. Acquires the YARQ barrier (see below). If acquisition fails
   because another worker is already inside the box, this worker
   re-submits its task to the tail of the pool queue and exits;
   the re-submission picks up later, after the contended box is
   done.
4. Pops the reconfigure-tagged cell, runs the JSON through the
   same `parse_box_file` machinery (operating on the in-memory
   buffer instead of a path), and rewrites the box record in
   place.
5. Releases the YARQ barrier.
6. Resumes input-gathering for the SAME fire — pops once more
   from the same port (or the next one per the new config's port
   shape) and continues until every required input is satisfied.
7. Fires the box under the new config, passing the retained
   already-popped values plus the new pops in the order the
   new config expects.

The fire that triggered the gather still happens. The reconfigure
is a side-effect that lands between the start of input-gathering
and the function call itself, not a separate scheduling unit.

### What about the values popped before the reconfigure?

The retained values were popped under the OLD port shape. If the
new config keeps every original port (rename allowed, add allowed,
remove disallowed in this slice → see open questions), the
retained values still map by port name. If the new config renames
a port that has a retained value, the retained value follows the
rename via the port's previous-name → new-name correspondence
recorded during reconfigure. If a future slice allows port
removal mid-gather, the retained values for the removed port get
dropped on the floor and a JSONL event records the discard.

## YARQ — the yield-and-requeue barrier

A reconfigure has to be atomic with respect to other workers
firing the same box. While one worker is rewriting the record,
no other worker may read inconsistent state — half the new
`inputs[]` array with the old `routing.kind`, for example.

A spinlock would work but burns a worker. The user's phrasing
captures the intended shape exactly:

> "it's essentially 'hey we're not ready yet, put yourself back
> at the beginning of the queue please'"

**YARQ** — yield-and-requeue — is the project name for this
primitive. Mechanism:

- One atomic word on the box record (`box->yarq`, or similar).
- **Acquire**: CAS from 0 to 1. On success, the worker owns the
  box for the duration of the rewrite. On failure, the worker
  re-submits its current task to the **tail** of the pool queue
  and returns — no busy-wait, no parking, no priority inversion.
  The tail (rather than the head) is the **longest distance from
  re-running**: the contended worker doesn't immediately
  re-collide with whoever is currently inside the barrier; other
  pending work runs first.
- **Release**: atomic store of 0.

The cost is one extra trip through the scheduler for each
contended worker. The benefit is that the synchronization
primitive is *cooperative with the scheduler* rather than
*adversarial to it* — the scheduler keeps doing what it does
best (managing many small units of work fairly), and the barrier
just nudges contended workers to circle back later.

The YARQ barrier is acquired by **every** path that mutates the
box record — not just reconfigure but also `connect` (issue
319d's runtime wire attachment, which already does its own
copy-and-publish atomic dance on the connections array). Folding
those into the same barrier means the two mutation paths can't
interleave dangerously when both fire at once.

## Schema for the third-ring message

A message on the third ring is a JSON object identical in shape
to what `parse_box_file` parses today:

```json
{
    "id":      "my_box",          // must match the target box
    "kind":    "call",            // any kind, including a kind change
    "lang":    "lua",
    "ref":     "lib/new_impl.lua",
    "fn":      "new_handler",
    "routing": { "kind": "plain" },
    "inputs":  [
        { "name": "x", "type": "string" },
        { "name": "y", "type": "string", "optional": true }
    ],
    "connections": [
        { "to_box": "next", "to_input": "value" }
    ]
}
```

The `id` field MUST match the target box (a safety check; mixed
ids hard-error). Every other field is the new state. Fields
absent get default treatment (same as a freshly-parsed box file).
The schema validator runs in full — a malformed reconfigure
can't half-apply.

## Reconfigure semantics — what changes

**Slice 1 supports every field comprehensively.** No "cheap vs
medium vs expensive" split; the same machinery that parses a box
at load time runs against the in-memory reconfigure buffer and
applies the result wholesale. Splitting the work into easy /
hard tiers means coming back later to finish, and round-trips
are where details get forgotten.

That means slice 1 has to handle, in one pass:

- **Cheap-field rewrites** (lang / ref / fn / routing.kind /
  routing.comparand / routing.weights / output_capacity /
  returns / cflags / link_libs / headers) — pointer assignments
  on the live record. The arena holds the strings; the rewrite
  swaps pointers.
- **Connections array swap** — uses the same copy-and-publish
  pattern issue 319d established for `box_add_connection`,
  except wholesale-replace instead of append. Old array goes to
  the graph's stale list and is freed at graph_destroy.
- **Input port shape changes** — adding, removing, renaming
  ports. Triggers:
  - Slot-store growth or shrink (issue 319b's slot growth in
    the additive case; symmetric drain-and-release in the
    removal case).
  - Re-resolution of every producer's connection
    `to_input_idx` against the new port names.
  - Rebinding of retained pre-pop values via the old-name →
    new-name correspondence (rename allowed; removal of a
    retained port drops its values to the JSONL transcript).
- **Kind changes** (BOX_CALL ↔ BOX_WRITE, etc.) — the dispatch
  table is already keyed off kind; flipping the field flips
  which branch runs next. BOX_MAP is the special case (would
  re-trigger the encapsulation pass) — see open questions.

A reconfigure that hands in an invalid spec is a hard error: the
box dies, the JSONL transcript records the failure with the
offending JSON, the run terminates. No silent reject-and-continue
— matches the project's "errors over fallbacks" rule.

## How the third ring gets fed

Two complementary mechanisms, symmetric with issue 319's
create_box / connect:

1. **A `reconfigure` box kind** — one input port `spec` carries
   the box-JSON, one field (or a second input port) names the
   target box id and the target's input port to push the message
   onto. On fire, the box pushes the reconfigure-tagged cell onto
   the target slot's third ring.

2. **A runtime API** —
   `runtime_reconfigure(target_id, target_port, json, size, err)`
   — that any language spec's invoke can call. Same shape as
   `runtime_create_box` and `runtime_connect` from issue 319d.

A box can reconfigure itself by pushing onto its own third ring;
the next time the dispatch gathers inputs for this box, the
reconfigure lands.

## Suggested implementation steps

1. **Extend the per-cell ring-selector tag** in
   `src/009-slot-store.{c,h}`. The dual-ring slot's `which_ring`
   field grows a third value (`RING_RECONFIGURE`).
2. **Add the third ring to every input slot.** Same shape as the
   JSON ring (variable-size cells, large-value-heap backed). Cell
   capacity per slot is small (8? 16?) — reconfigures shouldn't
   pile up faster than the box can apply them.
3. **Add the YARQ barrier field** to `box_t`
   (`src/010-graph-loader.h`). One `_Atomic int`;
   acquire / release / yield-and-requeue helpers in
   `src/012-dispatch.c`.
4. **Fold issue 319d's `box_add_connection` into YARQ.** Today
   it has its own copy-and-publish atomic dance; sharing the
   barrier keeps mutation paths from racing.
5. **Refactor `parse_box_file`** so the JSON-parsing core
   accepts an in-memory buffer (not just a file path). The
   reconfigure path uses the same parser; one schema, one
   validator, two entry points.
6. **Add a `reconfigure_apply` function** in
   `src/010-graph-loader.c` that takes a parsed box record and
   merges it into a live box, handling port-rename via name
   correspondence, slot growth / shrink, connection array swap,
   and producer re-resolution.
7. **Teach the dispatch's input-gather loop** to peek the
   ring-selector tag and, on `RING_RECONFIGURE`, acquire YARQ,
   retain pre-popped values, apply the reconfigure, release
   YARQ, then resume gathering under the new shape.
8. **Implement the `reconfigure` box kind** in
   `src/012-dispatch.c` and add parse-side support in
   `src/010-graph-loader.c`.
9. **Add `runtime_reconfigure`** to
   `src/018-runtime-builtins.{c,h}`.
10. **JSONL event** for every reconfigure: full before / after
    (or at least the changed fields), task id, worker id.
    Reconfigures change behaviour mid-run; the audit log must
    show them.
11. **Fixtures**:
    - `tests/maps/320-reconfigure-routing/` — comparator routing
      flipped to iterator routing mid-run; downstream
      connections fire on the new branches.
    - `tests/maps/320-reconfigure-fn/` — same box, different
      function on the same lang; output changes mid-stream.
    - `tests/maps/320-reconfigure-port-shape/` — input port
      added (slot store growth) and renamed (producer
      re-resolution); pre-popped retained values still land on
      the right inputs.
    - `tests/maps/320-reconfigure-kind/` — BOX_CALL flipped to
      BOX_WRITE; subsequent fires write to disk instead of
      invoking a function.
    - `tests/maps/320-yarq-contended/` — two reconfigure-feeders
      race on the same target; both reconfigures land, neither
      is lost, the box behaves correctly after both apply.

## Relevant files

- `src/009-slot-store.{c,h}` — ring-selector tag, third ring
  storage per slot.
- `src/010-graph-loader.{c,h}` — `parse_box_file` refactor,
  `reconfigure_apply`, YARQ field on box_t.
- `src/012-dispatch.c` — dispatch input-gather changes, YARQ
  acquire / release / yield-and-requeue, `reconfigure` box kind.
- `src/018-runtime-builtins.{c,h}` — `runtime_reconfigure` entry
  point for language specs.
- `src/013-jsonl-events.{c,h}` — reconfigure event emission.
- `issues/319-builtin-library-for-map-self-construction.md` —
  sister issue: create_box / connect are the additive
  primitives, this is the mutative primitive.
- `issues/318a-per-call-output-format-threads-to-push.md` and
  `issues/312-same-language-wire-fast-path.md` — the per-cell
  ring-selector mechanism the third ring extends.
- `issues/319b-slot-store-growth.md` — slot growth in the
  additive port-add direction; symmetric shrink is new.

## Open questions

- **Scheduler ordering between value-pops and reconfigure-pops.**
  The mixed-stream scenario above shows reconfigure cells
  interleaved with value cells on the same input port; the
  consumer encounters them in producer-push order. Is that
  ordering strict per port, or strict across all ports of the
  box (so a reconfigure on port A blocks pops from port B until
  it's applied)? The cleanest reading of "ordered same as all
  the other inputs" is **strict per port**, with the consumer
  applying reconfigures as it hits them. Across-port ordering
  would need a per-box sequence number on every cell, which is
  more bookkeeping than the user described.
- **Slot growth and shrink for input port changes.** Issue 319b
  defined slot growth in the create_box direction; this issue
  extends it to the within-a-box direction (adding a new port
  to an existing box). Shrink — releasing a slot when a port
  goes away — is genuinely new and needs an issue of its own,
  or rolled in here as part of slice 1.
- **Port-rename correspondence.** When a reconfigure renames
  port `x` to `y`, the rename is unambiguous if both
  configurations declare the same number of ports in the same
  position-order. When the port count or order changes too,
  the correspondence isn't obvious — fall back to name match
  (ports keeping their names get rebound by name; ports
  whose names no longer exist get dropped). Worth documenting
  precisely so the implementation behaves consistently.
- **Recursive reconfigure / loop guard.** A box reconfigures
  itself, the new config includes a producer that pushes
  another reconfigure, etc. Cap per-box reconfigure depth per
  run? Or trust the user and rely on integration tests to
  surface infinite loops?
- **BOX_MAP as a reconfigure target / source.** Changing a
  BOX_CALL to a BOX_MAP would require re-running the
  encapsulation pass on just this one box. The load-time pass
  assumes single-pass flat-graph construction; running it
  per-box at runtime is structurally awkward. Probably
  out-of-scope unless a clear use case appears. Hard-error
  the BOX_MAP transition in slice 1.
- **Reconfigure visible in the compile artifact.** Issue 309's
  compile pipeline bakes the box JSONs into the built program.
  Reconfigure machinery is runtime-only by nature, but the
  compile artifact needs to keep the YARQ barrier field, the
  third ring per slot, and the in-memory parser path linked in
  regardless. Probably means slice 1 ships always-on; the
  per-box opt-out flag (`reconfigurable: false`) is a future
  refinement if anyone needs the slot-store savings.
- **JSONL event shape.** Full before/after JSON is the most
  faithful audit trail; diff-only is smaller; "just record
  the new state" loses the before. Default: full before/after
  so the transcript is self-describing without cross-event
  reasoning.
- **Pre-popped retained values that don't fit the new schema.**
  Reconfigure renames port `x` → `y` and adds port `z`; the
  worker had already popped a value for `x` before hitting
  the reconfigure cell. Rebind by name correspondence? If
  the new config has no port `y` after a removal-rename
  (effectively a delete), the retained value goes to the
  JSONL transcript as a discard event and the box still fires
  with whatever the new shape requires.

## Design history

The shape of this issue evolved across two passes. The first
draft (now discarded) proposed a per-box config inbox treated as
a separate scheduling unit — i.e. reconfigure as its own task,
distinct from any fire. The user redirected to **per-port**
placement, with reconfigure cells riding the same ring-selector
tag mechanism that already distinguishes native from JSON cells
on each port. The mixed-stream scenario described in the
"Concept" section is the user's clarifying example.

The YARQ name landed in the same conversation. The user's
phrasing:

> "yield and requeue works. yarq for short. YARQ what a weird
> name!! I want to name my dog yarq. BARK BARK says yarq, haha
> what a cute puppy ^_^"

So: YARQ in the code, YARQ in the docs, and somewhere out there
a dog who answers to the same name. Both kinds of YARQ go where
the scheduler tells them — that's the joke and that's the
mechanism.

On the YARQ requeue target (tail vs head of the pool queue),
the user's instruction was:

> "put it at the part of the queue that has the longest time
> until running again. I think that's the end of the queue, but
> you can decide. The tip of the tail maybe? snakes move
> forward after all..."

End-of-queue (the "tip of the tail") is the chosen default.
The contended worker won't immediately re-collide with whoever
is inside the barrier; the scheduler gets a chance to drain
other pending work first.

On scoping, the user's direction was decisive:

> "they should all be configurable with the same interface,
> so... don't split up the work in that way. Just do it
> comprehensively the first time so we don't forget anything
> on the return trip."

Slice 1 covers every reconfigurable field — including the
port-shape changes that require slot store growth / shrink and
producer re-resolution. No tiered "cheap then medium then
expensive" split; one comprehensive pass.
