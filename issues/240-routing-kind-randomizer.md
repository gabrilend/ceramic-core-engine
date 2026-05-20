# 240 — Routing kind: randomizer

## Status
open · design carried forward from issue 233

## Current behavior

Issue 233 shipped the unified `routing` schema with three kinds —
`plain`, `comparator`, `iterator`. The runtime dispatch layer
(`src/012-dispatch.c`) reserves enum slots for `randomizer`,
`weighted`, and `distributor` but falls them through to plain.
The schema (`src/001-schema.lua`) rejects `kind: "randomizer"`
outright so we don't accumulate dead state until the rule lands.

## Intended behavior

A call box configured for randomizer routing fires its function
on every invocation and then picks a single output branch
uniformly at random. Two-output box with `kind: "randomizer"`
fires `out_0` ~50% of the time and `out_1` ~50% of the time. The
distribution is deterministic-given-seed but spread across
branches — consecutive calls do not go to consecutive branches.

### Schema

```json
"routing": {
  "kind":      "randomizer",
  "n_outputs": 4
}
```

Output port names follow the iterator convention: `out_0` …
`out_<n-1>`. The schema must accept `kind: "randomizer"` and
require `n_outputs` to be a positive integer; reject anything
else.

### Branch picker (dispatch layer)

The slot store already owns a `SLOT_ATOMIC_COUNTER` slot per
routing-counter box (issue 302). Randomizer's picker reads and
increments the counter, then hashes the result before taking
mod n_outputs:

```c
uint32_t i      = slot_read_inc(counter_slot, UINT32_MAX);
uint32_t branch = hash(i) % n_outputs;
```

`hash` is a cheap mixing function (xorshift, FNV) — not
cryptographic. The point is breaking the monotonic counter so
adjacent invocations don't land on adjacent branches.

### Inspector UI

Mode dropdown gains a `randomizer` option (same row as plain /
comparator / iterator from issue 233). When selected, the
inspector shows a single `n_outputs` integer control — the same
shape iterator uses today.

## Suggested implementation steps

1. `src/001-schema.lua` — add `randomizer` to
   `valid_routing_kinds`, mirror the iterator's `n_outputs`
   validation.
2. `src/012-dispatch.c` — implement the branch picker.
   The hash function is one new helper inside `push_routed`.
3. `assets/js/004-inspector.js` — add `randomizer` to the mode
   dropdown's option list and re-use the iterator's
   `n_outputs` control for the inspector body.
4. `assets/js/002-boxes.js` — randomizer's output ports use the
   same `out_<i>` naming as iterator; the existing rendering
   branch covers it.
5. Fixture: `tests/maps/randomizer-route/` — single-input box
   feeding into N branches; the integration check asserts the
   distribution is roughly uniform across a few hundred
   invocations.

## Relevant files

- `issues/completed/233-unified-routing-schema.md` — parent
  issue; carries the wider design discussion this issue extracts.
- `src/001-schema.lua` — schema acceptance for the new kind.
- `src/012-dispatch.c` — branch-picker dispatch.
- `assets/js/004-inspector.js` — mode dropdown extension.
- `issues/302-wire-value-slot-store.md` — atomic-counter slot
  semantics consumed by the picker.
- `issues/304-task-dispatch-layer.md` — dispatch table updates.

## Open questions

- **Hash choice.** xorshift is fine. FNV-1a is also fine. The
  requirement is "spread monotonic counters across branches"
  with O(1) cost. Decide at implementation time.
- **Test stability.** Asserting uniformity in a fixture is
  noisy — pick a sample size large enough that a 99%-confidence
  interval excludes mis-implementation, document the
  expectation in the fixture's README.
