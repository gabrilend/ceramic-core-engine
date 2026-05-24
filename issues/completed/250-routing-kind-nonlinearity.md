# 250 — Routing kind: nonlinearity (three intent-named variants)

## Status
complete · the kind is named `nonlinearity` (default variant
`confidence`); calibration extrapolates past fixed bounds
rather than clamping; everything else lands as designed.
Schema + C loader + dispatch picker + editor controls + three
on-disk fixtures (one per variant) all shipped.

## Current behavior

The routing kinds shipped so far — `plain`, `comparator`,
`iterator`, `randomizer` (240), `weighted` (241),
`distributor` (242), multi-band-comparator (243's
`thresholds` extension on comparator) — all pick a **branch**
and forward the producer's value to the consumer **unchanged**.
The schema and the dispatch picker treat the wire payload as
opaque on the way out; routing has only ever decided *where* a
value goes, never *what* it is.

There is no routing kind that **transforms the value** on the
way through. A user who wants a producer's output remapped
into a smoothed, bounded "response" — for feeding a UI slider,
an audio level, a heat-map cell, a downstream decision pipeline
— has to insert a dedicated call box whose entire job is one
arithmetic expression. That's a lot of box for one knob.

## The shape of the new kind — value mutation as routing

The new routing kind emits a **scoring response** in a bounded
output range. The original input value isn't forwarded; the
**score** (a smoothed, normalised number reflecting how strongly
the input falls on one side of the box's bounds) is what travels
on the wire. If the user wants the *scaled input* downstream
rather than the response itself, they wire the upstream
producer to BOTH the nonlinearity AND a multiplication box, and
multiply input × response in the multiplier. The nonlinearity
is composable in this shape — small, single-responsibility, one
output port.

Three intent-named **variants** under one routing kind let the
user pick the right semantics without having to know what tanh
or sigmoid is. Each variant is a different smooth function with
a different center and output range:

| Variant | Output range | Midpoint | What it's for | Multiplication composes like |
|---|---|---|---|---|
| `decision`    | `[-1, 1]` | `0`   | Agree / disagree / abstain. Signed strength. | Soft AND with sign-tracking (positive×positive→positive, sign agreement reinforces) |
| `confidence`  | `[0, 1]`  | `0.5` | Probability / belief. Unsigned strength. | Probabilistic conjunction (independence assumption) |
| `calibration` | `[0, 1]`  | (none) | Pure range remap, no decision semantics. | Linear scaling — no opinion to compose |

Under the hood the three are tanh, logistic sigmoid, and a
clamped linear scale, but the user never has to use those names.
The dropdown shows `decision / confidence / calibration` with a
short tooltip per option.

### Why three variants are the right granularity

The user identified the underlying insight in their notes: an
S-curve produces strong outputs only for strongly positive or
strongly negative inputs; values close to the midpoint stay
close to the midpoint. Multiplying two values that are close to
the midpoint produces a value still close to the midpoint —
weak evidence stays weak, strong evidence reinforces. That's
how neural networks compose decisions, and the three variants
above are the three primitive neuron types: signed (`decision`,
like a perceptron), unsigned probability (`confidence`, like a
Bayesian-belief node), and linear (`calibration`, like a sensor
preprocessor).

## Bounds mode — auto or fixed, decided by which sides the user pinned

The bounds (the min and max the input gets normalised against)
have an elegant mode-switch from the user: **presence of a
supplied bound on each side is the mode for that side**. Two
fields on the routing block:

```json
"routing": {
  "kind":  "nonlinearity",
  "shape": "confidence",      // decision / confidence / calibration
  "min":   0,                 // omit for auto-tracked low side
  "max":   100                // omit for auto-tracked high side
}
```

- **Both `min` and `max` supplied** → fully fixed bounds; auto
  tracking off on both sides.
- **Neither supplied** → fully auto; the box tracks the running
  min and max it has seen so far and normalises against them.
- **One supplied, the other omitted** → asymmetric. The
  supplied side is locked; the omitted side keeps tracking.
  Useful when one bound is known ("values are always
  non-negative") but the other isn't ("I don't know how big
  they get").

C's `NULL` reading as 0 would have made "omitted" ambiguous if
the fields were raw doubles. The schema treats absence
distinctly from presence-of-zero: the JSON-level shape is "the
field IS or IS NOT in the object," not "the field's value is or
isn't zero." On the C side that surfaces as two per-bound
"is_fixed" flags on the routing struct alongside the two
doubles.

A second wire-fed input or third ring reconfigure (issue 320)
can push a fresh `{min, max}` object at runtime to switch
modes; the same presence-vs-absence rule applies.

## Midpoint and dampening — same elegant absence-is-mode pattern

A `midpoint` field follows the same rule:

- **Supplied** → asymmetric mode. The midpoint is anchored at
  the user's value; the auto-tracked bounds drift TOWARD that
  midpoint over time (the dampening pulls outliers in without
  changing the anchor).
- **Omitted** → symmetric mode. The midpoint is whatever the
  current `(min + max) / 2` is, computed each fire. The
  midpoint drifts with the data.

The dampening for the auto-tracked sides is an exponential
moving average (EMA) toward the midpoint:

```
mid = supplied_midpoint OR (min + max) / 2
min = α * min + (1 - α) * mid    // for auto sides only
max = α * max + (1 - α) * mid    // for auto sides only
if value < min:  min = value     // immediate widen on outlier
if value > max:  max = value
```

The decay rate `α` (default `0.99`, i.e. halve toward midpoint
roughly every 70 fires) lets the bounds settle after the last
outlier has aged out. A minimum-gap floor (`max - min >= 2ε`)
keeps the bounds from collapsing to a degenerate point on
constant input.

## Output — emit the response, always as a double

The output is always a `double` regardless of the input's type.
Sigmoid's image is dense in `[0, 1]`; rounding to int would
collapse it to `{0, 1}` and lose the score. Downstream
consumers that want an int should hard-error rather than
silently truncate.

The **scoring response** is the wire payload, not the scaled
input. If `input = 5` and the confidence variant says "5 lands
73% of the way through your bounds," the wire carries `0.73`,
not `3.65`. The user composes input × score in a downstream
multiplier if they want the scaled input.

## Coercion — strict, hard-error on failure

The kind operates on numbers only. The dispatch picker tries to
coerce the inbound value:

- **number on the wire** — used directly.
- **string** — parsed as a double via `strtod` with the
  trailing-junk-rejected variant (so `"3.14abc"` is a parse
  failure, not 3.14). Matches the project's strict-over-permissive
  preference for parsing.
- **boolean** — `true` becomes `1.0`, `false` becomes `0.0`.
  Worth a fixture: the C side treats 1 and 0 as true and false,
  and the routing kind's first-class numeric handling should
  match that convention so a boolean wire feeding a
  nonlinearity behaves predictably.
- **everything else** (blob, sentinel, unknown JSON shape) —
  hard error, unconditional.

The earlier draft considered "extract the first number from a
string" for LLM-output tolerance — the user's own conclusion
discarded that: a model that says `"2 × 2 = 4"` would feed `2`
into the nonlinearity, not `4`, so the lossiness is wrong-shape
anyway. Strict parsing it is.

## Out-of-range under fixed — clamp per variant (the certainty-threshold reinterpretation)

The user reframed the original "what to do when a value exceeds
the user's bounds" question elegantly: under fixed mode, the
supplied bounds **mean** "past this value, I'm 100% certain."
Clamping isn't erasing information — it's the right behaviour
for that semantic.

Per-variant defaults:

- **`confidence`**: `input > max` → output = `1.0` (certain
  yes); `input < min` → output = `0.0` (certain no). Clamp.
- **`decision`**: `input > max` → output = `+1.0`; `input <
  min` → output = `-1.0`. Clamp.
- **`calibration`**: don't clamp — the variant has no
  certainty semantics. Out-of-range values either extrapolate
  linearly (output goes past the [0, 1] range) or hard-error
  per a per-box toggle. Default: extrapolate.

Auto sides (when the bound is being tracked) widen on
out-of-range automatically, so the clamp doesn't fire on auto
sides at all. The clamp only applies where the user has
explicitly pinned the bound.

## Steepness `k` — sensible default, reconfigurable, inspector-exposed

The steepness `k` governs how sharp the S-curve is. `k = 1` is
gentle: the middle 80% of the input range maps to roughly
`[0.27, 0.73]`. `k = 6` is sharp: the middle 80% maps to
roughly `[0.05, 0.95]`. Default to `k = 1` (more forgiving,
better for "let weak signals stay weak" use cases).

The inspector exposes `k` as a number input with a small SVG
preview of the chosen curve, with vertical guide-lines at the
80% marks so the user sees both the curve shape AND the
"middle 80% maps to [low, high]" numbers update live. The
preview is cute and informative; both reasons it stays in scope.

A user who wants to change `k` at runtime feeds the box a
reconfigure message via issue 320's third input ring — the
same path that lets any box's JSON shape be updated mid-run.
This keeps the inspector knob and the runtime-tunable knob
working off the same field.

## Per-box atomic min/max — local to the box, contested by parallel instantiations

The auto-tracked min and max live in two atomic cells on the
box record (not per-worker, not per-thread). If the same box
fires under N parallel inputs at once (e.g. an upstream
iterator pushes 500 values at once), all N instantiations
contend on the same two cells. CAS-update widens the bounds;
plain reads return whatever's currently stored — no read lock
so reads stay free.

Auto bounds are **expected to be non-deterministic across runs**
because the order of arrivals depends on the pool's scheduler.
This is a feature, not a bug; the kind is for scoring streams
of data where the recent past defines what "normal" looks like.
A user who wants deterministic behaviour should supply fixed
bounds. No debugging output for the non-determinism — a user
who wants to inspect the running bounds can wire a small log
box downstream of the nonlinearity and read the score
trajectory.

## Reset across runs — no persistence by default

The auto bounds reset to "unseen" at the start of each run. A
user who wants the previous run's bounds carried forward
supplies them as fixed values on the current run's box JSON.
Persistence-by-default would add a hidden file on disk per
nonlinearity box; explicit-on-request is cleaner.

## Editor surface

- **Variant dropdown**: `decision / confidence / calibration`,
  with a short tooltip per option ("Soft +/- with clear
  neutral" / "Soft yes/no with uncertainty" / "Range remap, no
  curve").
- **min, max, midpoint** number inputs. Each has a per-field
  "auto" checkbox; checked means "omit this field, let the box
  track it." Unchecked means the value in the input is the
  pinned bound.
- **k (steepness)** number input.
- **Live preview**: small SVG showing the chosen variant's
  curve under the current `k`, with vertical guides at the
  middle-80% input range and horizontal guides at the output
  values those map to. Numbers updated live as `k` changes.
- The box's single output port keeps the plain-routing name
  (`out`) so wires drawn before the kind was switched in don't
  need renaming.

## Suggested implementation steps

1. **Schema** (`src/001-schema.lua`): accept `kind:
   "nonlinearity"`; validate `shape` is one of `decision /
   confidence / calibration`; optional `min`, `max`, `midpoint`,
   `k`.
2. **Slot store** (`src/009-slot-store.{c,h}`): add
   `SLOT_DOUBLE` cells with CAS-update widening semantics —
   atomic-load returns the current value; widen is a CAS loop
   that retries until the new value sticks. Two such cells per
   nonlinearity box that has auto sides (one for each direction
   that might widen).
3. **Graph loader** (`src/010-graph-loader.c`): read the
   nonlinearity routing fields into the box record; allocate
   the auto-side slot cells; record the per-bound "is_fixed"
   flags.
4. **Dispatch** (`src/012-dispatch.c`): implement the
   value-transform branch in `push_routed`. This is the first
   routing kind that **writes a new value** to the output wire
   instead of forwarding the input unchanged; the path
   currently passes the input slot pointer through. The branch
   reads bounds + midpoint + k, computes the normalised
   position, applies the variant's curve, clamps per the
   per-variant rule, writes the double to the output buffer,
   pushes downstream.
5. **Inspector** (`assets/js/004-inspector.js`): variant
   dropdown, four number inputs each with the auto checkbox, k
   input, SVG preview component. Reuses the setter pattern from
   240/241/243 (sever outgoing wires on shape change — though
   here the output port shape is stable, so wires survive).
6. **Canvas** (`assets/js/002-boxes.js`): single output port
   named `out`, same as plain routing. No new port-shape
   handling needed.
7. **Fixtures**: at least one per variant.
   - `nonlinearity-confidence-route/` — feed integers 1..100
     into a confidence box with fixed bounds [0, 100]; assert
     output 50 maps to ~0.5, output 100 clamps to 1.0.
   - `nonlinearity-decision-route/` — feed -10..10 into a
     decision box with fixed bounds [-10, 10]; assert 0 → 0,
     +10 → +1, -10 → -1.
   - `nonlinearity-calibration-route/` — fixed bounds [0, 200],
     input 50 → 0.25; out-of-range 250 extrapolates to 1.25
     (not clamped, calibration variant).
   - `nonlinearity-auto-route/` — no fixed bounds; feed a
     stream; assert later values get sensible scores once the
     box has seen enough data.
   - `nonlinearity-bool-input-route/` — input is `true` /
     `false` strings; assert the coercion path produces sane
     scores.

## Open questions

- **Name of the kind.** `nonlinearity` is precise but
  intimidating; `squash` matches neural-network vocabulary;
  `remap` is the most boring and most discoverable. Pick one
  before the schema lands — once on disk in fixture maps,
  renaming is a migration. The three intent-named variants
  inside the kind soften the kind-name's importance somewhat
  (users pick a variant, not the kind), so the kind-name only
  has to read sensibly in the dropdown of routing kinds.
- **Default variant.** Which of the three variants does a
  freshly-created box land on? `confidence` is the most
  general-purpose (probability semantics fit most "score this"
  use cases). `calibration` is the most neutral (no curve,
  pure remap — closest to plain routing for a user who picked
  the kind by accident). `decision` is the most opinionated.
  Default to `confidence` unless a use-case-survey says
  otherwise.
- **Calibration out-of-range mode.** Extrapolate (default
  above) vs hard-error vs clamp-to-1. Extrapolate is the most
  honest for a "pure remap" semantics; the other two are
  defensible too. Settle when a fixture forces a choice.

## Notes on architecture

The nonlinearity is the first routing kind whose dispatch path
emits a value that wasn't already on the input wire. The
existing `push_routed` machinery passes the input bytes through
verbatim; the new path produces a fresh `double`, formatted as
bytes via `snprintf("%g", score)`, and pushes that. Worth being
explicit about so future kinds (a hypothetical `square`,
`log`, `abs`, `quantise`) can follow the same shape — and so
the architecture review notices when a third or fourth value-
transforming kind appears and considers whether the category
deserves a first-class concept rather than per-kind one-offs in
push_routed.

One existing routing kind already breaks the "every output is
exactly one branch fired" rule: the iterator. Its fire emits on
one port but the port chosen depends on the counter, not on a
branch index baked into a wire. The nonlinearity also has a
single output port; the user picks a variant, the variant
decides what to emit. So the "single output port + variable
output value" shape isn't unprecedented.

The dump truck on the other side of town is still picking up
trash, and the raccoons and the hillbillies are still arguing
about whose turn it is to do the dishes. The user's earlier
note about playing hard and loose with the users-who-misuse-the-
box — "you're gonna go far but that's because the dump truck
that picks up the trash (aka you) has to drive really far to
get to the dump which is way on the other side of town with
the raccoons and the hillbillies" — stays preserved here
because that's how feedback memory works on this project:
sometimes the design lives in the comment, not the spec.
