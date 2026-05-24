# 250 — Routing kind: nonlinearity (squash)

## Status
open · design carried forward from issues 240 / 241 / 242 / 243

## Current behavior

The routing kinds shipped or queued so far — `plain`, `comparator`,
`iterator`, `randomizer` (240), `weighted` (241), `distributor`
(242), `multi-band-comparator` (243) — all pick a **branch** and
forward the producer's value to the consumer **unchanged**. The
schema (`src/001-schema.lua`) and the dispatch picker
(`src/012-dispatch.c`) treat the wire payload as opaque on the
way out; routing has only ever decided *where* a value goes, never
*what* it is.

There is no routing kind that **transforms the value** on the way
through. A user who wants the producer's output remapped into a
new numeric range — for feeding a UI slider, an audio level, a
heat-map cell — has to insert a dedicated call box that runs a
Lua / C function whose entire job is one arithmetic expression.
That's a lot of box for one knob.

## Intended behavior

A new routing kind, **`nonlinearity`** (working name; "squash"
is the alternative — see open questions), maps each numeric value
that flows through the box into a bounded output range via a
chosen unary function (default: logistic sigmoid). The input is
normalised against either:

- **`auto`** — the running min and max observed on this wire
  during the current run, OR
- **`fixed`** — two user-supplied numeric bounds (`min`, `max`).

After normalisation the value lands in `[0, 1]` (or `[-1, 1]` for
tanh — see open questions), the chosen non-linearity is applied,
and the result is forwarded on the box's single output port. The
routing kind has exactly one output — fan-out happens the same
way every other single-output box's fan-out happens (multiple
wires leaving one port).

-- to switch between auto and fixed, the user just needs to supply one or two
   values - if they supply one, the minimum / maximum is set (the slots in the
   editor and json are specified as "min" or "max" so there's no ambiguity about
   which is which) if they supply both then it's set to "fixed" mode as specified.
   I want there to be only one mode, and auto is the fallback if no fixed values
   are specified. C treats NULL as zero, so if we wanted to switch back to auto
   after specifying a value then we'd need to have two separate flags on-top of
   the min and max value - they basically say "is this a fixed value or is it
   an auto value?" - if the flag is set to fixed, then the value won't change if
   we receive a higher/lower value. If the flag is unset to auto, then we will
   adjust the min/max value depending on the min/max value we've received.

-- I don't know the math very well so you'll have to take the lead on this part,
   but we need a function that will work equally well without normalization.
   The vision I had, which might be more / less useful for AI purposes than some
   other kind of function, is essentially an S curve with the minimum and max
   set to certain values, and whatever inputs come in will get fairly mid value
   unless they're very clearly a "strong" value one way or the other. This is
   why 0 is selected as the midpoint usually, I think, so that values close to
   mid, or perhaps a fairly even mixture of two values, can be multiplied and
   the result will be close to 0. So, maybe we need three - one that has the
   midpoint at 0, one that has the midpoint at 1, and one that scales? I'm not
   sure, can you picture why I would want such a thing? If so, can you offer any
   ideas about how to create intelligent behavior with this structure?

-- what if, for the scaling one, we also added a "dampening" effect where each
   time the function is run, it reduces the min/max a little toward an equilibrium.
   it doesn't necessarily have to be 0 or 1 or whatever, it should just adjust so
   that the values become normalized over time. How would we ensure that there's
   a decent enough gap between the min and max? How would the input values which
   expand the min/max be considered? We'd re-calculate the curve, probably, right?

### Schema

```json
"routing": {
  "kind":     "nonlinearity",
  "shape":    "sigmoid",        // or "tanh" / "linear" / "softsign"
  "mode":     "auto",           // or "fixed"
  "min":      0,                // present only when mode == "fixed"
  "max":      100,              // present only when mode == "fixed"
  "out_range":"unit"            // or "signed" — see open questions
}
```

`mode: "auto"` requires no bounds — the dispatch picker maintains
a running min and max in two slot-store cells (issue 302) per
nonlinearity box, updated atomically on each fire. `mode:
"fixed"` requires both `min` and `max`; schema rejects either
missing or `min >= max`.

### Branch picker (dispatch layer)

There is no branch to pick — the kind always emits on the single
output. The dispatch code path differs from the existing
routing kinds in that it **mutates the value** rather than
**selecting an index**. Pseudocode:

```c
double x = coerce_to_number(value_in);     // hard-error path on failure

double lo, hi;
if (mode == AUTO) {
    lo = slot_min_load(min_slot);
    hi = slot_max_load(max_slot);
    slot_min_cas_update(min_slot, x);      // best-effort widen
    slot_max_cas_update(max_slot, x);
} else {
    lo = box->fixed_min;
    hi = box->fixed_max;
}

double t = (hi == lo) ? 0.5 : (x - lo) / (hi - lo);   // bootstrap case

double y;
switch (shape) {
    case SIGMOID:  y = 1.0 / (1.0 + exp(-k * (t - 0.5))); break;
    case TANH:     y = tanh(k * (t - 0.5));               break;
    case LINEAR:   y = t;                                 break;
    case SOFTSIGN: y = (t - 0.5) / (1.0 + fabs(t - 0.5)); break;
}

emit_number(out_port, y);
```

`k` is a steepness constant (see open questions). The bootstrap
case (`hi == lo`, true on the very first value seen under `auto`
mode) maps to `t = 0.5` so the sigmoid emits 0.5 — a defensible
"no information yet" value.

-- what if we take the midpoint of the min/max and we say "if above, then do
   this branch. If below, then this branch." That way, we're saying which path
   the value takes while also including a "strength" or "magnitude" value in the
   modified output value. Is that a good idea?

### Coercion and the hard-error path

The kind operates on numbers only. The dispatch picker tries to
coerce the inbound value:

- **number on the wire** — used directly.
- **string** — parsed as a number (`strtod`). On parse failure,
  the box raises a hard error matching the project's "errors over
  fallbacks" rule. The error names the box id, the offending
  string, and the wire it arrived on.
- **other types** (boolean, blob, sentinel) — hard error,
  unconditional. No silent coercion.

-- C treats 1 and 0 as true and false, so keep an eye out for that... Make sure
   there's a test case for that.

Hard errors stop the box from emitting. No "skip on bad input"
fallback — that's a warning shape this project treats as an
error.

### Inspector UI

Mode dropdown on the routing inspector gains a `nonlinearity`
option (same row as the other routing kinds). When selected, the
inspector body shows:

- **Shape** dropdown: sigmoid / tanh / linear / softsign.
- **Bounds mode** dropdown: auto / fixed.
- **Min** and **Max** number inputs (visible only when bounds
  mode is fixed).
- A small **live preview** plot — the chosen shape rendered into
  a thumbnail SVG so the user sees what they're picking. (Stretch
  goal; not blocking the first slice.)

-- the svg is cute ^_^

The box's single output port keeps the plain-routing name
(`out`) so wires drawn before the kind was changed don't need
renaming.

## Suggested implementation steps

1. `src/001-schema.lua` — accept `kind: "nonlinearity"`; validate
   `shape`, `mode`, the `min` / `max` pair when `mode == fixed`,
   and the `out_range` choice.
2. `src/302-wire-value-slot-store.*` — add `SLOT_MIN_DOUBLE` and
   `SLOT_MAX_DOUBLE` slot kinds with CAS-update semantics
   (compare-and-swap loop that retries if another worker widened
   the slot between the load and the store). Reuse if such slots
   already exist; otherwise extend.
3. `src/010-graph-loader.c` — read the nonlinearity routing
   fields into the box record; allocate the two slot cells when
   `mode == auto`.
4. `src/012-dispatch.c` — implement the value-transform path.
   This is the first routing kind that **writes a new value** to
   the output wire instead of forwarding the input unchanged; the
   dispatch code currently passes the input slot pointer through.
   Generalise so a routing kind can supply a transformed value.
5. `assets/js/004-inspector.js` — add `nonlinearity` to the mode
   dropdown; render the shape / mode / min / max controls.
6. `assets/js/002-boxes.js` — single output port named `out`,
   same as plain routing.
7. Fixture: `tests/maps/nonlinearity-route/` — feed a sequence of
   integers 1..100 into a sigmoid box, assert the outputs are
   monotonic, bounded in `[0, 1]`, and the midpoint is near 0.5.
   A second fixture exercises `mode: "fixed"` with bounds
   tighter than the input range to confirm the box does **not**
   clamp (it lets sigmoid saturate naturally) — or does, per the
   open-questions decision.

## Relevant files

- `issues/240-routing-kind-randomizer.md`,
  `issues/241-routing-kind-weighted.md`,
  `issues/242-routing-kind-distributor.md`,
  `issues/243-routing-multi-band-comparator.md` — sibling
  routing-kind issues; copy the schema-and-inspector shape.
- `issues/completed/233-unified-routing-schema.md` — the parent
  schema this kind plugs into.
- `src/001-schema.lua`, `src/010-graph-loader.c`,
  `src/012-dispatch.c` — runtime side.
- `assets/js/004-inspector.js`, `assets/js/002-boxes.js` —
  editor side.
- `issues/302-wire-value-slot-store.md` — slot kinds the running
  min/max cells live in.

## Open questions

- **Name of the kind.** `nonlinearity` is descriptive but long
  and abstract. `squash` is shorter and matches the
  neural-network vocabulary the user invoked when proposing it.
  `remap` is the most boring and most discoverable. Pick one
  before the schema lands — once it's on disk in fixture maps,
  renaming is a migration.
- **Default shape.** Sigmoid is the user's suggestion. Tanh
  centres around zero (useful when the consumer wants signed
  output). Linear (i.e. just the normalisation, no curve) is
  the "I only want the rescaling, not the bend" case. Softsign
  is a cheaper sigmoid-shaped alternative that doesn't need
  `exp`. Default could reasonably be any of the four.
- **Output range — `[0, 1]` vs `[-1, 1]`.** Sigmoid naturally
  lands in `[0, 1]`; tanh lands in `[-1, 1]`. The `out_range`
  field lets the user pick which the box emits, with the
  dispatch picker applying a `(y * 2 - 1)` or `(y + 1) / 2`
  fixup as needed. Or — drop the field and let the chosen shape
  dictate the range. Symmetry argument both ways.

-- I was thinking we'd output the scaling value directly. Then, the user can
   multiply it by the input value if they want in the next box. All they'd have
   to do is take the upstream box, wire it to both the non-linearity box, and
   also to the downstream multiplication box - the scaling would start running
   it's processing, and it wouldn't get scaled until it finished because the
   input value, also inputted to the downstream box, would sit and wait in the
   queue until the non-linearity finished and supplied it's output downstream.

- **Steepness `k`.** A sigmoid with `k = 1` is gentle; the
  middle 80% of the input range maps to roughly `[0.27, 0.73]`.
  `k = 6` gives a much sharper S-curve, with the middle 80%
  mapping to roughly `[0.05, 0.95]`. Expose `k` as an inspector
  control? Pick a sensible default and hide it? The latter is
  cleaner UI but less expressive.

-- I say sensible default, and let the user provide json values when reconfiguring
   (see issue 320 I think?) if they want a different steepness K or whatever.
   if it's a common operation then we can expose it in the editor. I think
   k = 1 sounds better, 0.27 and 0.73 are more forgiving. But, I also don't
   want to be opinionated... well, I can be opinionated in structure, but not in
   methodology. So ahhhhh screw it let's just expose it in the editor. Just make
   sure we both show the graph as an svg, and we show those 80% values you specified
   because those are both useful.

- **Bootstrap behaviour under `auto`.** The first value seen
  has no min/max to normalise against (`hi == lo`). The design
  above emits 0.5 in that case. Alternatives: emit the input
  unchanged, emit a sentinel "no data yet" value, suppress
  the emit entirely and wait for a second value to establish a
  range. Suppressing is appealing on theoretical grounds but
  breaks the "every fire emits" contract every other routing
  kind upholds.
- **`auto` mode is non-deterministic across runs.** The same
  input sequence produces different outputs depending on what
  was seen earlier in the run. Users debugging a downstream
  consumer will see drift. Worth documenting; maybe surface a
  "freeze observed bounds" inspector action that snapshots the
  current `auto` bounds into `fixed` bounds on save.

-- It's expected to be non-deterministic. We don't have to have any debugging
   output related to this concern. If the user wants that, they can put a small
   box after that writes to a log file.

- **Out-of-range under `fixed` mode.** When a value arrives
  outside the user's `[min, max]`, three behaviours are
  reasonable: clamp into range (`t = clip(t, 0, 1)`), let
  sigmoid saturate naturally (no clamp; sigmoid handles it
  gracefully but linear/softsign won't), or hard-error
  ("user said the range was X, this isn't"). Default leans
  toward natural saturation for the curved shapes and clamp
  for linear — but inconsistency between shapes is its own
  surprise.

-- okay here's what I'm thinking. we shouldn't clamp, that erases values. But,
   if we still need to get it within 0 and 1, we could somehow find a scaling
   value to be applied to the initial input value. The scaling value would be
   the value between 0 and 1. Not sure how that works...

- **Number-vs-int preservation.** If the input is an int and
  the bounds are ints, should the output be an int? Almost
  certainly no — sigmoid's image is dense in `[0, 1]` and
  rounding to int collapses it to `{0, 1}`. Output is always
  double. Worth being explicit so a downstream consumer
  expecting an int sees a clean type-coercion error rather
  than silent truncation.

-- we should output a double.

- **String parsing strictness.** `strtod` accepts `"3.14abc"`
  as 3.14 (and the project elsewhere has chosen strict over
  permissive). Strict parse — reject any non-whitespace after
  the number — keeps the kind aligned with the project's
  preference for errors over fallbacks.

-- what if we accept the first number found in a string, and strip out the rest,
   both before and after that number? It'll make it a bit easier if an LLM or
   something outputs something like: "sure, I can multiply 2x2! The answer is 4."
   wait actually that won't help at all, because it'd output 2. Nuts. I guess
   that problem should be solved on the user's end with better prompting...

- **Per-worker vs shared min/max.** Auto-mode bounds are
  best shared across all workers (so the normalisation is
  consistent regardless of which worker handled which value),
  which means atomic CAS on a shared slot. Per-worker bounds
  would be faster but produce per-worker output streams that
  drift — likely the wrong shape.

-- neither per-worker nor per-thread. The value should be local to the box. If
   there are multiple instantiations of the same box, then only the box that is
   being run will have the value. If many different instantiations of the same
   box are being run, like if we supply 500 input values at once, then they'd
   all contest the same min/max values - atomic spinlocks on them will keep
   everyone honest, so long as we don't lock it when reading values.

- **Reset across runs.** Auto bounds reset to "unseen" at the
  start of each run. Persisting them across runs (so the
  second run starts from where the first left off) is a
  separate feature; flag it for a follow-up if anyone asks.

-- we don't want to persist between runs. If the user wants that, they will provide
   fixed values as instinct.

- **Where the transform lives.** This is the first routing
  kind that mutates the wire value. The dispatch layer
  currently treats routing-kind logic as branch selection
  only — generalising it to "may also rewrite the value
  being forwarded" is a small but real architectural shift.
  Worth thinking about whether **value-transforming routing**
  is a category we want more of (square, log, abs,
  quantise…) — if so, factor the rewrite-the-value path as
  a first-class concept rather than a special case for this
  one kind.

-- I think the fact that it mutates the value will be obvious by the fact that
   there's only one output port. Though, now that I think of it, the iterator
   function only has one output port as well... Hmmmm, maybe we could just
   have a warning at the top that says "beware, this box mutates the output value"
   or something. Or we could just play hard and loose with the rules and tell
   the users that if they mess up, they get fucked, sorry kid but that's how it
   goes. you're gonna go far but that's because the dump truck that picks up the
   trash (aka you) has to drive really far to get to the dump which is way on 
   the other side of town with the raccoons and the hillbillies.
