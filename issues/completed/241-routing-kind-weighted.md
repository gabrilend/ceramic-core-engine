# 241 — Routing kind: weighted

## Status
complete · schema accepts `kind: "weighted"` with a non-empty
`weights` array of non-negative numbers; loader and dispatch's
cumulative-band picker already shipped (issue 233's groundwork);
editor dropdown grows the option, with a comma-separated weights
text input that severs outgoing wires on edit (the array length
sets the output-port count); `out_<i>` canvas rendering reads
weights.length; `tests/maps/weighted-route/` asserts that
weights = [1, 0, 0] routes deterministically to branch 0.

## Current behavior

Issue 233 shipped the unified `routing` schema with three kinds —
`plain`, `comparator`, `iterator`. The dispatch layer
(`src/012-dispatch.c`) reserves an enum slot for `weighted` and
falls it through to plain; the schema rejects the kind value
until this issue lands.

## Intended behavior

Weighted routing picks a branch by probability. With a two-
output box and weights `[0.8, 0.2]`, ~80% of invocations fire
`out_0` and ~20% fire `out_1`. The distribution is deterministic-
given-seed because the counter advances per invocation, but the
mapping from counter to branch is the weighted band lookup, not
modulo.

### Schema

```json
"routing": {
  "kind":    "weighted",
  "weights": [0.8, 0.2]
}
```

`weights` is an array of non-negative numbers; their sum should
be 1.0 (the schema renormalises silently if not — see open
questions). The output port count is `weights.length`, with the
same `out_<i>` naming.

### Branch picker (dispatch layer)

At compile time the loader normalises weights into a cumulative
table on an integer precision scale (e.g. 1000):

```
weights [0.8, 0.2]  →  cumulative [800, 1000]
weights [1, 1, 2]    →  cumulative [250, 500, 1000]   (normalised)
```

Per call:

```c
uint32_t r      = slot_read_inc(counter_slot, PRECISION);
uint32_t branch = lookup_band(r, cumulative_table);
```

Same `SLOT_ATOMIC_COUNTER` slot machinery as iterator and
randomizer; only the table lookup differs.

### Inspector UI

The mode dropdown gains a `weighted` option. The per-kind
control is a horizontal slider with N-1 knobs dividing the bar
into N segments. One line per output port below the slider
shows a visual band with the port's percentage:

```
out_0    [████████░░░░░░░░░░░░]    80%
out_1    [░░░░░░░░████████████]    20%
```

Adjusting a knob updates the percentages live; percentages are
also editable directly (typing `30` into the right column
shifts the relevant knob and renormalises the others).

## Suggested implementation steps

1. `src/001-schema.lua` — accept `kind: "weighted"`, validate
   `weights` as a non-empty array of non-negative numbers.
2. `src/010-graph-loader.c` — compute the cumulative table at
   load time so the dispatch layer's hot path is just a lookup.
3. `src/012-dispatch.c` — implement `lookup_band` (linear scan
   over the cumulative table; binary search if N grows large
   enough to matter).
4. `assets/js/004-inspector.js` — build the slider-with-knobs
   control. Each knob writes back to `routing.weights` and
   renormalises siblings; saving renormalises a final time so
   the persisted weights always sum to 1.0.
5. `assets/js/002-boxes.js` — render `out_<i>` ports per the
   length of `routing.weights`.
6. Fixture: `tests/maps/weighted-route/` — a known-distribution
   call with a large invocation count, assert the realised
   distribution matches the declared weights within tolerance.

## Relevant files

- `issues/completed/233-unified-routing-schema.md` — parent
  issue.
- `src/001-schema.lua`, `src/010-graph-loader.c`,
  `src/012-dispatch.c` — runtime side.
- `assets/js/004-inspector.js`, `assets/js/002-boxes.js` —
  editor side.
- `issues/302-wire-value-slot-store.md` — atomic counter slot.
- `issues/240-routing-kind-randomizer.md` — sibling kind that
  shares the counter-driven dispatch shape.

## Open questions

- **Renormalisation timing.** Schema-side renormalise on save
  (always store summed-to-1 weights) vs trust user input and
  renormalise only at compile time. Latter is simpler; pick it
  unless the editor needs precise round-trips.
- **PRECISION value.** 1000 buys 0.1% resolution and lets
  cumulative entries fit in `uint32_t` easily. Likely fine; bump
  only if a use case shows up that needs finer.
- **Editing UX edge case.** What does "set out_2 to 0%" mean —
  remove the slot, or keep it but never fire? Issue 233's
  brainstorm leans toward "keep but never fire" so the wire
  layout doesn't shift unexpectedly.
