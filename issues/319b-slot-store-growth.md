# 319b — Slot store growth primitive

## Status
open — planning stub. Full design in parent issue 319's "Q1"
resolution.

## Parent issue
Sub-issue of 319 (built-in library for map self-construction).
The design for this slice is fully specified in 319's "Q1 —
When does the new box exist?" resolution section.

## Current behavior

`src/009-slot-store.{c,h}` allocates all slots at graph load.
The store has no growth primitive; slot pointers are stable for
the run because the array never moves. The header explicitly
states: "Lifetime is the run. Slots are freed when the store is
destroyed at the end of the run."

## Intended behavior

The slot store grows at runtime via a three-layer chunked-append
+ RCU-swap structure (see 319's Q1 resolution for the full
diagram and rationale). Slots themselves never move; chunks
themselves never move; only the top-level index array swaps when
it fills, and the swap uses double-buffer-with-defer-free.

## Suggested implementation

To be expanded when this sub-issue is picked up. The design is
already final in 319.

Touches: `src/009-slot-store.{c,h}`, `tests/009-slot-store-test.c`.

## Not in scope

Per-slot ring buffer growth (separate concern — same double-buffer
pattern under the per-slot lock, but a different code path).
