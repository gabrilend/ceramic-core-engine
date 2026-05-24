# 242 — Routing kind: distributor (load-aware)

## Status
complete · schema accepts `kind: "distributor"` with
`n_outputs`; loader and dispatch's argmin-over-downstream-fill
picker (with counter-driven tiebreaker) already shipped; editor
dropdown grows the option, reuses the iterator's n_outputs
control; `out_<i>` canvas rendering shared with iterator and
randomizer; `tests/maps/distributor-route/` asserts the
empty-everywhere case (tiebreaker counter at 0 → branch 0) wins.

## Current behavior

Issue 233 shipped the unified `routing` schema. `distributor` is
reserved as an enum value in `src/012-dispatch.c` but falls
through to plain; the schema rejects the kind until this issue
lands.

## Intended behavior

Distributor routing sends the function's output to the
**least-busy** downstream consumer. The dispatch action inspects
the fill level of every input slot wired to each branch and
picks the branch whose downstream slot has the fewest queued
values.

### Schema

```json
"routing": {
  "kind":      "distributor",
  "n_outputs": 3
}
```

Same `out_<i>` port naming and `n_outputs` count as the other
counter-driven kinds.

### Branch picker (dispatch layer)

Unlike the counter-driven kinds, distributor reads downstream
state at dispatch time:

```c
uint32_t best_branch = 0;
uint32_t best_fill   = UINT32_MAX;
for (int i = 0; i < n_outputs; i++) {
    uint32_t fill = downstream_fill(box, i);
    if (fill < best_fill) { best_branch = i; best_fill = fill; }
}
// tie-breaker: atomic counter, so ties don't always go to branch 0
if (count_ties(box, best_fill) > 1) {
    best_branch = slot_read_inc(tiebreaker_slot, n_outputs);
}
```

`downstream_fill(box, i)` reads `tail - head` on the slot wired
to `out_<i>`. The slot store already exposes
`slot_fill_count` per issue 302.

For fan-out (one branch with multiple consumers), the design
question is whether to use max, sum, or mean of the consumer
fills. Issue 233 leans toward **max** — bottleneck is the
slowest consumer.

### Inspector UI

Mode dropdown gains `distributor`. The per-kind control is the
same `n_outputs` number input the iterator and randomizer use.

## Suggested implementation steps

1. `src/001-schema.lua` — accept `kind: "distributor"`,
   validate `n_outputs`.
2. `src/012-dispatch.c` — implement the picker. Tie-breaker
   uses a counter slot the box already has from issue 302.
3. `src/009-slot-store.c` — verify `slot_fill_count` is
   available cheaply (already shipped per 302 unit tests).
4. `assets/js/004-inspector.js` — add `distributor` to the
   dropdown, reuse iterator's `n_outputs` control.
5. `assets/js/002-boxes.js` — `out_<i>` rendering already
   covers it.
6. Fixture: `tests/maps/distributor-route/` — three downstream
   sinks with different inherent latencies, send N inputs
   through the distributor, assert the slow sink received
   fewer than the fast ones.

## Relevant files

- `issues/completed/233-unified-routing-schema.md` — parent.
- `issues/302-wire-value-slot-store.md` — `slot_fill_count`
  this picker calls.
- `src/012-dispatch.c` — dispatch table.
- `assets/js/004-inspector.js`, `assets/js/002-boxes.js` —
  editor side.

## Open questions

- **Fan-out fill semantics**: max vs sum vs mean of downstream
  consumers when a branch fans to multiple boxes. Issue 233
  leans max; revisit if a workload says otherwise.
- **Sampling cost**: reading every downstream slot's fill on
  every invocation is O(n_outputs × fan_out_per_branch). For
  N=3 that's negligible; for N=32 with deep fan-out it could
  matter. Defer optimisation until a profile demands it.
- **Empty-everywhere case**: every downstream slot has 0
  fill. Pick branch 0? Round-robin via the tie-breaker?
  Round-robin is what the design specifies and is the right
  answer — no branch is "more empty" than any other.
