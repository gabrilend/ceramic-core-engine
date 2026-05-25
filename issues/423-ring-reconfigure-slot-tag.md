# 423 — RING_RECONFIGURE slot-store tag

## Status
open · phase 4 · sub-issue of [419](419-runtime-graph-mutation.md).
Waits on [420](420-utility-box-kind.md) locking in its mutator
flow shape before implementation starts.

## Current behavior

The slot store's dual-ring slots carry an ordering ring whose
cells are (which_ring, idx) tuples — the two ring values being
SLOT_RING_NATIVE (0) and SLOT_RING_JSON (1). Same-language
producers push to the native ring; cross-language producers push
to the JSON ring; consumers peek the ordering ring's head to
preserve arrival order across both data rings.

There is no control-message support on input slots. The phase-3
runtime mutation that shipped used a separate language-bridge
entry point (`runtime_create_box`, `runtime_connect`) and never
asked the slot store to carry mutation instructions.

## Intended behavior

Add a third ring-tag value (RING_RECONFIGURE = 2) alongside
NATIVE and JSON. A cell tagged RING_RECONFIGURE carries a
`box_t *` payload — a pointer to a pre-built replacement box
record. The utility-box fire pushes the tagged cell; the
target's next worker pops it under its gather flag and runs the
mutator path.

The data payload (the pointer's bytes) rides through one of the
existing data rings — the simplest shape is "push 8 bytes
through whichever data ring the slot was configured with, mark
the ordering cell with RING_RECONFIGURE so the consumer treats
the bytes as a pointer instead of a value." Alternative: add a
third internal ring just for control payloads. The reuse-the-
existing-ring form is cheaper; needs slot cell capacity ≥ 8
bytes, which most slots already are.

## Why a tag rather than a separate ring

A separate control ring per input port would need peek-twice
semantics: one peek on the value ring, one on the control ring,
both under the gather flag. A single ordering ring with a tagged
cell folds the discriminator into the existing peek-then-pop
discipline.

## Suggested implementation steps

1. Add `SLOT_RING_RECONFIGURE` (= 2) to the ordering-tag
   constants in `src/009-slot-store.h`.
2. Add `slot_push_reconfigure(store, slot_id, void *payload,
   uint32_t tag)` — pushes the 8-byte pointer into the slot's
   data ring (which one is implementation choice; lean native)
   and appends a RING_RECONFIGURE entry to the ordering ring.
3. Extend the dual-ring peek/pop helpers so they expose the tag
   to the consumer. `slot_peek_tag(store, slot_id) -> int32_t`
   returns NATIVE / JSON / RECONFIGURE for the head cell.
4. Extend `slot_pop_ordered` so a RING_RECONFIGURE pop returns
   the pointer bytes correctly.

## Open questions

- **RING_DELETE as a separate tag** versus
  RING_RECONFIGURE-with-NULL-payload — depends on 420's
  delete-detection mechanism. See 419 open question 1.
- **Slot store back-pressure on a RING_RECONFIGURE push.** A
  utility-box pushing to a full input slot — does it block,
  drop, or wait under back-pressure? Probably the same policy
  the value rings use today, but worth confirming.
- **Which data ring carries the pointer.** Native ring is the
  default because the producer is the utility-box (no language)
  and the target's consumer is the mutator path (no language).
  The JSON ring choice would require the pointer to look like a
  JSON value, which doesn't make sense.

## Relevant files

- `src/009-slot-store.{c,h}` — tag enum and helper variants.
- `src/009-slot-store.info.md` — document the third tag.
