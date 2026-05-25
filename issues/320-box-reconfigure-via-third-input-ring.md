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

### The pre-gather scenario (post-audit)

The audit confirmed it: `read_inputs` in `src/012-dispatch.c`
walks each input port exactly once and does exactly one
peek / pop per port per fire (line 459: `for (int i = 0; i <
b->n_inputs; i++)`). There is no "second pop on the same port
in one fire" — the original mixed-stream scenario was wrong, and
this section is the corrected design.

Because each fire consumes at most one cell per port, a
reconfigure cell on any port's third ring will be observed at
most once per fire of the target box. The cleanest place to
observe it is **before any value-pop**: at the top of
`read_inputs`, walk every input port once and check the third
ring's `slot_has_value` / peek. If any port has a reconfigure
queued, apply it (under YARQ) before any value cells are popped.
Then drop into the regular gather loop under whatever shape
exists after.

This means:

1. The worker enters `read_inputs` for box B.
2. **Pre-gather sweep**: for each port `i`, check the third ring.
   For every port with a queued reconfigure:
   - Try CAS-acquire YARQ. On fail, yield-and-requeue (see
     below) and exit.
   - On success, pop the reconfigure cell, run the JSON through
     the same `parse_box_file` machinery (operating on the
     in-memory buffer instead of a path), and rewrite the box
     record in place via `reconfigure_apply`.
   - Release YARQ.
3. **Gather**: walk each input port (under the now-possibly-new
   shape) and pop / peek values as usual.
4. Fire the box under the now-current config.

The "what about pre-popped values" question dissolves: nothing is
popped before reconfigures are applied, so there's nothing to
retain or rebind. Port renames / additions / removals between
reconfigure and gather are uneventful because the value-pop loop
runs against whatever `box->inputs[]` / `box->input_slot_ids[]`
the reconfigured box now exposes.

### What about reconfigures that arrive mid-gather?

If worker A starts gather (post-sweep), and worker B pushes a
new reconfigure cell while A is mid-value-pop, A doesn't notice
— the sweep has already run. The reconfigure waits on the third
ring; the **next** fire of the box picks it up in its pre-gather
sweep. This is fine: gathered values were collected under the
shape A observed, and the box function runs under that same
shape. The new reconfigure simply lands one fire later than
strictly possible. We trade tightness for simplicity, and the
trade is good because pre-gather application removes all the
in-flight-snapshot bookkeeping.

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

The YARQ barrier is acquired by **every path that MUTATES the
box record** — reconfigure_apply and runtime_connect (issue
319d's wire attachment, which already does its own copy-and-
publish atomic dance on the connections array). Folding those
into the same barrier means the two mutation paths can't
interleave dangerously when both fire at once.

**Workers that only READ the box record (input-gather + box
function invocation) DO NOT acquire YARQ.** They're not mutating
anything; making them block on the mutator path would burn
worker time for no consistency win. They read the box's mutable
fields through the same atomic-load patterns issue 319d already
uses for `connections` / `n_connections`.

### Reader consistency under mutation — design knob

Today, `n_connections` and `connections` are an `_Atomic` pair
with a "pointer first, count second" publish order so a reader
doing "count first, pointer second" sees either the old pair or
the new pair. That pattern relies on the connections array being
append-only — the new array's first `old_n` entries are
identical to the old array.

Reconfigure can SHRINK or REORDER the inputs array, so the same
trick doesn't work for `inputs[]` / `input_slot_ids[]` /
`input_slot_modes[]` / `input_edge_native[]` / `n_inputs`.
Three viable shapes:

- **A — bundle into one atomic-swap struct**. Allocate an
  `input_config_t` containing every per-port array + n_inputs;
  reconfigure_apply allocates a new config and atomic-stores the
  pointer; readers do ONE atomic load and use the snapshot.
  Clean and contention-free, but restructures box_t.

- **B — generation counter retry**. Add `_Atomic uint32_t
  inputs_gen`. Reader reads gen, reads arrays, reads gen again;
  on mismatch, retry. Less restructuring; small overhead per
  read; the retry case is rare. Doesn't capture the "snapshot
  is internally consistent" property as strongly as A — readers
  rely on retry to catch torn reads.

- **C — workers DO acquire YARQ for reads**. Simplest mechanism
  but contradicts the user's stated preference and adds a CAS to
  every fire.

This is a real fork in the road; the rest of the design
proceeds the same way for any of the three. Defer the pick until
the user weighs in.

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
  - No retained-value rebind needed (the audited pre-gather
    model never has retained values; reconfigures apply BEFORE
    any value-pop).
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
7. **Add a pre-gather sweep** to the dispatch's `read_inputs`.
   Walk every input port once, check the third ring on each
   port. For every port with a queued reconfigure: acquire
   YARQ (or yield-and-requeue on contention), pop the cell,
   apply via `reconfigure_apply`, release YARQ. Then drop into
   the existing value-pop loop under the now-current shape.
   No value cells are popped before reconfigures land, so no
   retain-and-rebind bookkeeping.
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
  RESOLVED by the audit: the pre-gather model applies
  reconfigures before any value cell is popped, so there are
  no retained values to rebind. Question is moot.

- **Reader consistency across mutation** (new, surfaced by
  the audit). See "Reader consistency under mutation — design
  knob" in the YARQ section. Three viable shapes (atomic-swap
  bundled struct / generation-counter retry / workers acquire
  YARQ); user picks before implementation lands.

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
