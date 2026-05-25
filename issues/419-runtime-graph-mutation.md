# 419 — Runtime graph mutation (parent)

## Status
open · phase 4 · the architectural ground rules have converged
across many design conversations; eight open questions still
gate implementation. Sub-issues 420 / 421 / 422 / 423 / 424
carry the per-sub-feature detail.

## Why this phase exists

Phase 3 attempted runtime graph mutation under the original 319
family (issues 319, 319c, 319d, 319e, 319f — all now in
`issues/` with "Rolled back — superseded by phase 4" markers
explaining why they came back). The shipped mechanism worked
end-to-end but committed to surfaces the broader design walks
back from:

- per-language wrappers (`soramech.create_box`,
  `soramech_create_box`) put graph mutation at the language
  layer rather than the runtime substrate
- id-as-input rather than wire-endpoint-as-target
- three separate code paths for create / reconfigure / delete

Phase 4 restarts from architectural ground rules learned through
that prior attempt. The implementation effort wasn't wasted; it
produced the rules below.

## Architectural ground rules

1. **Graph mutation is a box-kind operation, not a language-
   bridge function.** Users express mutations by placing
   utility-boxes in their maps and wiring them, not by writing
   code that imports the runtime as a library. The runtime is
   the substrate; users express through it.
2. **The wire endpoint is the target.** A utility-box's
   outgoing wire ends at the box being operated on. Multiple
   wires fan the operation out to multiple targets in one fire.
3. **One unified box kind absorbs create / reconfigure /
   delete.** The kind dispatches based on the spec's content
   (empty payload → delete; populated payload → install at
   slot, which transitions an empty placeholder to populated
   for "create" or one populated record to another for
   "reconfigure").
4. **Reconfigures ride existing input slots, tagged
   RING_RECONFIGURE.** No separate control queue; the slot
   store's ordering ring grows a third tag value.
5. **The swap uses a gather flag + refcount + atomic pointer
   store at the graph slot.** Workers cooperate on a per-box
   gather flag during their peek-then-pop window; lose the CAS
   means yield-and-requeue. Refcount governs OLD's free.
6. **Ids are editor-only metadata.** Runtime targeting is by
   wire endpoint (= graph-slot index). The id field on a spec
   carries no programmatic role at runtime.

## Sub-issues

- **[420 — utility-box kind](420-utility-box-kind.md)** — the box
  kind itself; gather flag; reconfigure path; mutator flow;
  port-by-name slot mapping.
- **[421 — unified connections mutation through box swap](421-unify-connections-mutation-through-box-swap.md)**
  — collapses the "append a connection" path into the same swap
  mechanism; drops the `_Atomic` qualifiers on box_t's
  connection fields (already done as part of the rollback).
- **[422 — box delete: user-facing experience](422-box-delete-via-runtime-spec-api.md)**
  — the explicit-delete spec shape; convenience wrapper; the
  push-to-deleted-target policy.
- **[423 — RING_RECONFIGURE slot tag](423-ring-reconfigure-slot-tag.md)**
  — the slot-store side; third tag value on the ordering ring.
- **[424 — runtime-mutation JSONL events](424-runtime-mutation-jsonl-events.md)**
  — `box_reconfigure`, `reconfigure_discard`, `box_delete`
  events on the transcript.

## Open questions to resolve before implementation starts

These carried forward from the convergence discussion that
produced the wire-as-target design. Each sub-issue's own
open-questions section carries the detailed framing; this is the
parent-level inventory.

1. **Empty-spec-as-delete detection.** Explicit RING_DELETE tag
   versus RING_RECONFIGURE with NULL payload. The two-tag form
   keeps the mutator's branch table flat; the implicit form
   reuses one tag and one slot. Lean explicit.
2. **Where the wire endpoint points for the create path.** The
   create path needs the new box's id (or graph-slot index) to
   flow OUT somewhere so downstream consumers can reference it
   later. Two-port utility-box (one wire is target, one is
   id-out) was rejected — no two-output boxes outside routing.
   Mode-dependent cargo on a single wire (control for
   reconfigure/delete, value for create) is the alternative;
   needs a clear disambiguator that isn't an `action` field.
3. **Whether reconfigure/delete REQUIRE the spec's id field to
   match the wire endpoint(s).** If the id field stays at all,
   it's editor-only metadata; safety-check against wire endpoint
   may still be useful.
4. **Utility-box name.** Candidates: `utility` / `spec_applier`
   / `mutate` / `control`. Lean `utility` — vague but accurate.
5. **Multi-target semantics on conflicting id + wire.** When a
   spec carries an id AND a wire-endpoint points elsewhere,
   apply to BOTH targets (multi-target fan-out) or reject as
   ambiguous? Multi-target via wires is the natural shape
   (5 wires from one utility-box = 5 simultaneous reconfigures
   with the same spec); a spec id pointing at a 6th target is
   the ambiguous case.
6. **Push-to-deleted-target default.** Silent drop + JSONL
   `target-gone` event + opt-in strict mode, or hard-error
   default? Lean silent — a single deleted leaf-box shouldn't
   crash a long-running graph.
7. **Per-cell atomic on `graph.boxes`.** Type qualifier
   (`_Atomic(box_t *) cells[]`) versus `__atomic_*` builtins.
   Lean type qualifier.
8. **Slot reclamation on abandoned ports.** Reconfigure-
   abandoned slots leak today (no `slot_free`). Block 420 on
   building slot-free first, or accept the leak? Lean accept;
   defer reclamation to its own follow-on.

## Prior art

The four planning documents at `/tmp/soramech-320-321-322-*.md`
captured the design conversation at four detail levels (verbose,
mid, dense, voice). These are scratch documents, not promoted
to docs/; rewrite into `docs/datapath-graph-mutation.md` once the
open questions resolve and the shape is final.

The rolled-back 319-family issues in `issues/319*.md` describe
what the prior attempt built. The historical-behavior sections
inside those files are the closest thing to executable design
specs for what NOT to repeat.

## Relevant files (anticipated, not committed)

- `src/009-slot-store.{c,h}` — RING_RECONFIGURE tag (423).
- `src/010-graph-loader.{c,h}` — gather flag + refcount + RCU
  pointer per id (420 / 421).
- `src/012-dispatch.c` — gather acquire/release; mutator path
  (420).
- `src/013-jsonl-events.{c,h}` /
  `src/014-event-queue.{c,h}` — three new event types (424).
- `src/018-runtime-mutation.{c,h}` — a new module (NOT the
  rolled-back 018-runtime-builtins; a new shape that owns the
  utility-box's fire path and slot-mapping helpers).
