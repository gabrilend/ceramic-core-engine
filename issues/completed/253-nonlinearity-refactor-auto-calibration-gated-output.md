# 253 — Nonlinearity refactor: auto-calibration + gated output

## Status
complete — single `range` toggle (signed/unit) replaces the
three variant names; per-box ring buffer of last `memory`
values replaces EMA-decayed bounds; output is v × score
(gated linear unit) instead of score alone; editor surfaces
the simplified controls; two cold-start fixtures cover both
ranges. 250's first slice stays in completed/ as the
historical record.

## Why

The 250 implementation that just landed has three issues
relative to the actual use case (decision lattices with
inputs arriving at unpredictable scale):

1. **Three intent-named variants are confusing.** Math-aware
   users don't recognise `decision` / `confidence` /
   `calibration`; non-math users still don't understand the
   underlying math. Both audiences are served poorly. A simple
   range toggle (signed `[-1, 1]` vs unit `[0, 1]`) reads
   correctly to both groups.
2. **Bounds via fixed-or-EMA fits the wrong story.** The
   actual workload is "arbitrary input arrives, calibrate on
   the fly from the values seen so far." EMA decay is a
   continuous smoothing; the user wants discrete event
   semantics — "remember the last N observed values; the
   current bounds are the min and max of that window."
3. **Output is the score alone, not input × score.** The user
   wants the **gated output**: the input value multiplied by
   the S-curve's response. That's the soft-AND composition the
   downstream multiplier was supposed to compose externally,
   but since multiplication is the load-bearing pattern, it
   belongs in the box.

## The refined design

### Output range — single toggle, two values

```json
"routing": {
  "kind":   "nonlinearity",
  "range":  "signed",       // "signed" → [-1, 1]; "unit" → [0, 1]
  "memory": 16,             // ring-buffer size N (default 16)
  "k":      1               // steepness (default 1.0)
}
```

That's the entire schema. No `shape`, no `min` / `max` /
`midpoint` (the ring buffer derives them), no per-bound flags.

The range toggle picks the S-curve:
- `signed` → tanh (centred on 0, output `[-1, 1]`)
- `unit`   → sigmoid (centred on 0.5, output `[0, 1]`)

### Bounds via ring buffer, not EMA

A per-box ring buffer of size N holds the last N observed
values. On every fire:

1. Acquire the per-box mutex on the ring buffer.
2. Overwrite the oldest slot with the incoming value `v`.
3. Recompute the buffer's `min` and `max` (O(N) scan; N is
   small, this is cheap).
4. Release the mutex.

The bounds ARE the min and max of the buffer. No drift, no
EMA. A new value extends the bounds immediately if it's
extreme; an outlier ages out of the buffer after N fires and
the bounds tighten back to the next-most-extreme value.

The user's "trigger only on extreme-value overwrite"
optimisation is deferred — every-fire recompute is fine while
N stays small.

### Gated output — input × score

```
v        = input value (arbitrary scale)
t        = normalised position of v in [min, max]
score    = S-curve(t * k)    // tanh or sigmoid per range toggle
output   = v * score
```

`t` is `(v - min) / (max - min)` mapped into the variant's
domain — `[-1, 1]` for signed (centred at midpoint), `[0, 1]`
for unit (zero at min).

Saturation: values beyond the current bounds get the bounds'
score (the S-curve at ±1 for signed, at 0 or 1 for unit). The
ring buffer absorbs them on the same fire so subsequent calls
see the widened bounds.

For signed range:
- v near positive bound → score ≈ +1 → output ≈ v × 1 (full
  positive amplitude passed through)
- v near negative bound → score ≈ -1 → output ≈ v × -1
  (positive amplitude, sign-flipped) — useful as a magnitude
  detector with bipolar input
- v near midpoint → score ≈ 0 → output ≈ 0 (suppress)

For unit range:
- v near max → score ≈ 1 → output ≈ v (passthrough)
- v near midpoint → score ≈ 0.5 → output ≈ v / 2 (attenuated)
- v near min → score ≈ 0 → output ≈ 0 (gate closed)

This is the "gated linear unit" pattern from neural networks:
the score is a soft gate that scales the input rather than
replacing it.

### Decision-lattice composition

The whole point: chain N of these into a downstream aggregator
that's itself a nonlinearity. The midpoint values
(uncertainty) suppress to ~0; the strongly-bounded values
(certainty) pass through near full amplitude. The aggregator
sees a sum of "votes weighted by certainty" rather than a sum
of raw signals. Compose more layers, and weak evidence stays
weak while strong evidence reinforces — the soft-AND property
the original 250 doc described, but with the multiplication
baked in.

## Implementation plan

1. **Schema** (`src/001-schema.lua`): drop `shape` /
   `nonlinearity_shape` validation; accept `range`
   (`"signed"` / `"unit"`); accept `memory` (positive
   integer, default 16); keep `k`.
2. **Loader** (`src/010-graph-loader.{c,h}`): replace
   `nonlinearity_shape_t` with `nonlinearity_range_t`
   (`NL_SIGNED` / `NL_UNIT`); remove fixed-bound fields and
   the running atomic cells; add ring-buffer fields
   (allocated double array + write index + per-box mutex);
   parser reads `range`, `memory`, `k` and allocates the
   buffer.
3. **Dispatch** (`src/012-dispatch.c`): rewrite the
   `ROUTING_NONLINEARITY` case. Acquire the mutex, append to
   the buffer, compute min/max, normalise, apply curve,
   compute `v × score`, format and push.
4. **Editor** (`assets/js/004-inspector.js`): replace
   variant dropdown with a `range` toggle (`signed` /
   `unit`); replace bound inputs with a single `memory`
   number input; keep `k`.
5. **Fixtures**: update the three existing fixtures
   (currently asserting score-only output) to assert
   `v × score` output. The deterministic-input fixtures
   compute as:
   - confidence at 50 with bounds [0,100]: ring buffer has
     [50]; min=max=50; normalised v=midpoint; sigmoid(0)=0.5;
     output = 50 × 0.5 = 25.
   - decision at 0 with bounds [-10,10] (ring buffer needs
     more than one value to set the bounds): need to seed the
     buffer with the bounds. Rework the fixture to feed
     min/max/midpoint through an iterator before the test
     value.
   - calibration variant goes away (no more variants).
6. **Issue file** updates: this issue closes on merge.

## Open questions

- **Initial bounds behaviour.** With an empty ring buffer (no
  values seen yet), what does the first fire emit? Options:
  - Emit `v × 0` (neutral score until calibrated) — clean but
    loses the first value.
  - Emit `v × 1` (passthrough until bounds form) — preserves
    the value but the score isn't meaningful yet.
  - Emit nothing on the first fire, wait for the second. Wrong
    shape for a dataflow box that's supposed to fire on every
    input.
  Lean toward `v × 0` for cleanliness; the first value seeds
  the buffer so subsequent fires have something to normalise
  against. Document the "first fire is silent" behaviour as
  the warm-up cost of auto-calibration.
- **Fixed-bounds override.** Drop entirely (auto-only)? Or
  keep `min` / `max` as optional pinned overrides? Dropping
  is simpler; pinning is useful when the user knows the input
  range and wants to skip the calibration warm-up. Default to
  dropping; restore if a use case asks.
- **Concurrent appends.** A per-box mutex is the simplest
  shape. For boxes that fire many times per second from
  parallel upstream iterators, a lock-free ring (Vyukov's
  MPSC, etc.) would be faster, but the simple mutex is fine
  while throughput stays modest.
