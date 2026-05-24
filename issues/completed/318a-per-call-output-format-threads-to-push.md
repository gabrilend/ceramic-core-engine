# 318a — per-call output_native threads through to push_one_connection

## Status
complete

## Parent issue
Follow-on to 318. Issue 318's closeout documented that intra-Lua
`$lang_opaque` reconstruction failed when the producer was
forced to JSON output by a cross-language sibling consumer; the
consumer's same-language wire's per-edge classification said
"native" but the slot received JSON bytes. This sub-issue fixes
the underlying mismatch.

## Current behavior

The producer's per-call `output_native` flag (1 = native, 0 =
JSON, computed inside `invoke_box_impl` as
`all_wires_native(b)`) now threads through to
`push_one_connection`. On a dual-ring destination slot:

- If `output_native = 1` AND the per-edge bit is 1, push to the
  native ring (same-language native form).
- Otherwise, push to the JSON ring. This covers two cases:
  - per-edge bit = 0 (cross-language wire) — JSON is the right
    form regardless.
  - per-call output_native = 0 (producer wrote JSON because of
    some cross-language sibling) — the bytes are JSON, must go
    to the JSON ring so the consumer's `slot_pop_ordered`
    returns `which_ring = JSON` and the spec runs json_to_native.

The signature change ripples through `push_routed`,
`push_to_downstream`, and `push_branch` — each takes a new
`output_native` parameter that's forwarded down. For non-CALL
boxes (read / write / create_box / connect) the dispatch_action
defaults the value to 1, matching their "writes plain bytes"
semantics. CALL boxes get the value populated by `do_call_box`
via a new out-parameter.

## Validation

- The 318-lang-opaque fixture's consumer now correctly
  reconstructs the Lua function across the JSON wire. Output
  changes from `consumer → fn-type=nil` (silent fallback to raw
  bytes) to `consumer → doubled=42` (function called with the
  input value of 21).
- All 18 integration tests pass, all unit suites still pass.

## Why this isn't the per-cell native flag the 318 closeout
suggested

The dual-ring slot ALREADY carries per-cell flag info — the
ordering ring's `(which_ring, idx)` tuples record which ring
each cell came from, and `slot_pop_ordered` returns
`which_ring` directly. The actual gap was on the WRITE side:
push_one_connection was choosing the ring based on the per-edge
bit, which lies when the producer's per-call format is JSON.
Once the per-call flag reaches the push, the per-cell flag on
the read side is automatically correct because the cell ends up
in the ring the producer actually wrote.

So the 318 closeout's "needs per-cell native flag at read time"
was misdiagnosed — the per-cell flag already exists; the bug
was that the push wasn't using the same per-call flag the
producer used. Fixing the push side restored the round-trip
without any slot store changes.

## Relevant files

- `src/012-dispatch.c` — `push_one_connection`, `push_branch`,
  `push_to_downstream`, `push_routed`, `do_call_box`,
  `dispatch_action` all updated to thread `output_native`.
- `tests/maps/318-lang-opaque/` — fixture's consumer now
  asserts `doubled=42` (was `fn-type=nil`).
- `scripts/run-tests.sh` — fixture's check_map updated to
  include the `consumer → doubled=42` substring.

## Lessons

- A "per-cell" diagnosis covered a real symptom but not the
  cause. The fix turned out to be one level up — at the
  write-side ring choice. The slot store's per-cell tagging
  was already correct; it was being told to put the cell in
  the wrong ring.
- Threading a value through ~5 functions to reach the right
  decision point is sometimes the right move. The alternative
  (thread-local global) would have worked but obscured the
  data-flow shape of the dispatch.
