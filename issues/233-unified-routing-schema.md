# 233 — Unified `routing` schema for branching boxes

## Status
open

## Concept

A "branching box" is a regular call box whose output, after the
function runs, is sent to **one of several** downstream
destinations rather than to all of them. Same function, same
inputs, same output value — only the routing decision differs.
Currently we have two flavors:

- **Comparator**: routes the function's output by comparing it
  to a stored threshold. Three fixed branches (`lt` / `eq` /
  `gt`).
- **Iterator**: routes the function's output to one of N branches
  in round-robin fashion, advancing a counter on every call.

The two are the same shape (function runs → output gets routed)
with different rules for picking the branch. The current schema
has them as separate top-level fields (`comparand` and
`iterator_outputs`), which makes them look like unrelated
features. They aren't.

(For "send the same value to multiple destinations," the user
just pulls multiple wires out of a single output port — fan-out
already works on plain call boxes. No routing decision needed.)

### Branch-level fan-out

Fan-out applies on a per-branch basis even for branched boxes. If
the comparator's `lt` port has three wires going to three
different consumers, then each time the routing decision picks
`lt`, all three wires receive the value (a copy is pushed into
each consumer's input slot). On a subsequent invocation that
picks `eq`, only the wires hanging off the `eq` port fire.

Each branch port acts like its own little plain-call output port
when it fires. The routing decision picks **which branch** fires;
the branch's own fan-out picks **which consumers** receive the
value when it does.

## Intended behavior

Every call box carries a `routing` field. No implicit defaulting:
plain boxes have `routing: { "kind": "plain" }`, the same way
branched boxes carry their own kind. Explicit and consistent —
no "what if I forget the routing field" surprises at validation
time.

```json
{ "id": "plain-call",
  "kind": "call",
  "ref": "...", "fn": "...",
  "inputs": [...],
  "routing": { "kind": "plain" } }

{ "id": "score-router",
  "kind": "call",
  "ref": "src/score.lua", "fn": "score",
  "inputs": [...],
  "routing": { "kind": "comparator", "comparand": 4 } }

{ "id": "round-robin",
  "kind": "call",
  "ref": "src/dispatch_msg.lua", "fn": "dispatch_msg",
  "routing": { "kind": "iterator", "n_outputs": 3 } }
```

Routing kinds defined in this design: `plain`, `comparator`,
`iterator`, `randomizer`, `weighted`, `distributor`. Of those,
**this issue ships `plain`, `comparator`, and `iterator` only**
— that's the surface area already in production, just unified
under one schema. The other three (`randomizer`, `weighted`,
`distributor`) are designed below for context but split into
their own follow-on issue files when this one is implemented, so
each ships independently.

The `routing.kind` field picks which dispatch-layer rule decides
the branch. Schema validates the kind value and the kind-specific
parameters together.

**The function always runs**, regardless of routing kind. The
function produces the value; routing decides where the value
goes.

### kind="plain"

```json
"routing": { "kind": "plain" }
```

One output port. The function's output value fans to every wire
attached to that port unconditionally. No routing decision; the
dispatch layer skips the branch picker and pushes to all wires.

This is the default for new boxes; the editor sets it
automatically when a box is created.

### kind="comparator"

```json
"routing": { "kind": "comparator", "comparand": "<number>" }
```

Three output ports: `lt`, `eq`, `gt` (fixed names; ports aren't
renameable per the editor's port-immutability rule). The
function's output value is compared to `comparand`; the matching
branch fires.

#### Multi-band comparator (planned brainstorm)

The current 3-branch lt/eq/gt design is a special case of "value
falls into a numeric range, fire the corresponding branch." A
generalization:

```json
"routing": {
  "kind": "comparator",
  "thresholds": [3, 7, 11, 17]
}
```

Five output ports get named by the threshold positions —
`below_3`, `between_3_7`, `between_7_11`, `between_11_17`,
`above_17`. With `thresholds: [4]` this becomes the current
3-branch lt/eq/gt (with `eq` being "exactly 4"), so the legacy
shape is one configuration of the new one.

**Equality bands via doubled thresholds.** Encode the current
`eq` semantics by repeating a threshold value: `thresholds: [3, 3]`
means "less than 3, exactly 3, greater than 3" — three bands, the
middle one being a zero-width point that fires only on exact
equality. With `thresholds: [3, 3, 7, 7]` you get five bands:
`< 3`, `== 3`, `3 < x < 7`, `== 7`, `> 7`. The editor sees a
list of thresholds; the runtime sees doubled values as
"point bands."

This means the legacy 3-branch lt/eq/gt is exactly
`thresholds: [c, c]` — the eq case is just a zero-width band at
the comparand. Behind the scenes the editor can present the
single-comparand UI but store `[c, c]` so the dispatch layer has
one consistent representation.

Defer the schema specifics — the single-comparand case ships
first; multi-band is a follow-on once we have a use case that
actually needs more than three.

### kind="iterator"

```json
"routing": { "kind": "iterator", "n_outputs": 3 }
```

`n_outputs` output ports, named conventionally (`out_0`, `out_1`,
…). Every invocation reads the box's `SLOT_ATOMIC_COUNTER` slot
via `slot_read_inc(counter_slot, n_outputs)`; that index picks
the branch.

The function still runs and produces the routed value. Iterator
defaults the function to a passthrough identity if the user
hasn't picked a `ref` / `fn` — but the user is free to attach
any function whose output should be round-robin-distributed.

### kind="randomizer"

```json
"routing": { "kind": "randomizer", "n_outputs": 4 }
```

Picks a branch uniformly at random per invocation.
Implementation: hash the counter-slot value into the branch
index, so the distribution is deterministic-given-seed but
spread across branches.

```c
uint32_t i = slot_read_inc(counter_slot, UINT32_MAX);
uint32_t branch = hash(i) % n_outputs;
```

`hash` is a cheap mixing function (xorshift, FNV); not
cryptographic. The point is breaking up the monotonic counter so
consecutive calls don't go to consecutive branches.

### kind="weighted"

```json
"routing": {
  "kind":    "weighted",
  "weights": [0.8, 0.2]
}
```

Probability-based distribution. With `[0.8, 0.2]` and two output
ports, 80% of invocations fire branch 0, 20% fire branch 1.

Implementation: at compile time, weights are normalized into a
cumulative table on a `0..PRECISION-1` integer scale (e.g.
`PRECISION = 1000`). Per call:

```c
uint32_t r = slot_read_inc(counter_slot, PRECISION);
// pre-computed cumulative: [0..799] → branch 0, [800..999] → branch 1
uint32_t branch = lookup_band(r, cumulative_table);
```

Same counter-slot machinery as iterator and randomizer; the
difference is the mapping from counter to branch.

**Inspector UI for weighted routing**: a horizontal slider with
N knobs dividing it into N+1 segments — except really N segments
since each knob is the boundary between adjacent segments. A
wider segment means a larger fraction of invocations flow down
that path. Below the slider, one line per output port:

```
out_0    [▓▓▓▓▓▓▓▓░░░░░░░░░░░░]    80%
out_1    [░░░░░░░░░░░░░░░░▓▓▓▓]    20%
```

Each line shows the output port name on the left, a visual band
in the middle indicating its slice of the distribution, and the
percentage on the right. Adjusting the slider knobs updates the
percentages live; the percentages are also editable directly
(typing `30` into the right column shifts the relevant knob).

### kind="distributor" (load-aware)

```json
"routing": { "kind": "distributor", "n_outputs": 3 }
```

Sends the value to the **least-busy** downstream consumer:
inspects the fill levels of the input slots wired to each branch
and picks the branch whose downstream slot has the fewest queued
values.

Implementation: dispatch action peeks the `tail - head` count on
each downstream input slot and picks the minimum. Tied branches
fall back to atomic-counter round-robin so ties don't always go
to the same branch.

This is the only routing kind that's not purely
self-contained — it reads downstream slot state — but it doesn't
add any new dispatch-layer concept beyond querying slot fill
(which the slot store already knows).

### Future kinds

The pattern is: any new routing kind plugs in at one place
(`routing.kind` value + dispatch-layer branch picker). All
existing kinds and any future ones live entirely in the dispatch
layer + slot store + editor — never in language specs, never in
user-written functions.

## Iterator + function: ships as "function always runs"

Iterator boxes always run their function. The dispatch action
becomes uniform across routing kinds:

```
1. Read inputs (pop / peek per port mode).
2. Invoke spec (always — function runs regardless of routing kind).
3. Pick branch via routing.kind:
     comparator   → compare(output, comparand)
     iterator     → slot_read_inc(counter_slot, n_outputs)
     randomizer   → hash(slot_read_inc(counter_slot, MAX)) % n_outputs
     weighted     → cumulative lookup
     distributor  → argmin(downstream fill)
     (no routing) → fan to all outgoing wires
4. Push function output to outputs[picked branch] (or all wires if
   no routing).
```

A single dispatch path for all branching boxes; the routing rule
plugs into step 3.

## Schema migration

Few enough existing maps that we can hand-migrate them. The
loader does **not** carry a legacy-shape adapter; once this
ships, all map files are expected to use the `routing` field. If
a map with the old shape is opened, the schema validator rejects
it and the user (or a one-off migration script) updates it.

If we end up wanting a one-off script, it walks `maps/<name>/boxes/*.json`
and rewrites:
- `comparand: X` → `routing: { kind: "comparator", comparand: X }`
- `iterator_outputs: [...]` → `routing: { kind: "iterator", n_outputs: len(...) }`
- everything else → no change

But for the current map count, manual edits are faster than
writing the script.

## Inspector UI

A `routing` selector in the inspector replaces the separate
`compare` toggle and `iterator` toggle. The dropdown's options
expand as new routing kinds ship:

```
mode:    [plain ▾ | comparator | iterator]
```

Per-kind controls below the dropdown — for the kinds shipped in
this issue:
- **plain**: nothing extra.
- **comparator**: numeric `comparand` input. (Multi-band UI is
  the brainstorm above, lands in its own issue.)
- **iterator**: `n_outputs` integer input.

Output ports re-render to match the routing kind: 1 dot for
plain, 3 fixed dots (lt/eq/gt) for comparator, N dots for
iterator. Port names follow the routing kind's convention; not
user-renameable (issue 224's read-only-port-name rule).

## Suggested implementation sequence

This issue ships `plain`, `comparator`, and `iterator` only —
the existing surface area unified under one schema. The other
kinds get follow-on issue files (one per kind: randomizer,
weighted, distributor, multi-band-comparator) opened when this
issue is implemented.

1. `src/001-schema.lua`: accept `routing` field with `kind` ∈
   `{plain, comparator, iterator}`; reject legacy `comparand` /
   `iterator_outputs`. Schema also rejects unknown `kind` values
   so future kinds gate on their own implementation.
2. `assets/js/004-inspector.js`: mode dropdown replacing the two
   toggles; per-kind controls (none for plain, `comparand` input
   for comparator, `n_outputs` for iterator).
3. `assets/js/002-boxes.js`: render branches based on
   `routing.kind`.
4. `assets/js/006-wires.js`: connection's `from_branch` matches
   the routing kind's port naming.
5. Phase 3 dispatch (issue 304): branch on `routing.kind` for the
   branch-selection step. Three rule paths shipped here; new
   kinds add their own paths in their own issues.

Hand-migrate existing maps before merging. Open the follow-on
issues for randomizer / weighted / distributor / multi-band
comparator at merge time so their designs aren't lost.

## Why now

- Editor inspector keeps gaining toggles per routing kind. A
  unified mode dropdown scales linearly with kinds; separate
  toggles don't.
- Phase 3 dispatch will naturally have a "select branch" step;
  routing it through `routing.kind` is the same code regardless
  of whether we ship Level B now or refactor later.
- The new routing kinds (randomizer, weighted, distributor) only
  make sense in the unified schema. Adding them as separate
  top-level fields would be unmaintainable.

## Relevant files

- `src/001-schema.lua` — schema accepts new field
- `assets/js/004-inspector.js` — mode dropdown
- `assets/js/002-boxes.js` — branch rendering
- `assets/js/006-wires.js` — branch matching
- `issues/302-wire-value-slot-store.md` — counter slot mechanism
  used by iterator / randomizer / weighted
- `issues/304-task-dispatch-layer.md` — dispatch layer's branch
  picker dispatches on `routing.kind`

## Open questions

- **Distributor under fan-out**: if a branch wire goes to
  multiple consumers (fan-out), the "fill level" of that branch
  is ambiguous. Use the max of the consumers' fills, or the sum,
  or something else. Probably max — the bottleneck is the
  slowest consumer.
- **Multi-band comparator threshold edges**: `x == threshold`
  goes to which side of the cut? Lean toward "less-than-or-equal"
  for consistency. Need to nail down before implementation.
- **Weighted ties**: floating-point weights summing to `1.0`
  doesn't always discretize cleanly. Compile-time normalization
  picks an integer scale (1000) and rounds; the last band
  absorbs any leftover so the ranges always sum to PRECISION.

## Level C example (deferred)

The big follow-on after this issue ships is a fully data-driven
router: one dispatch path that reads a small rule expression
from the box JSON and evaluates it. Example shape:

```json
"routing": {
  "kind": "rules",
  "rules": [
    { "when": "output < 3",                   "to": "small" },
    { "when": "output >= 3 and output < 7",   "to": "medium" },
    { "when": "output >= 7",                  "to": "large" }
  ]
}
```

The dispatch layer ships a small expression evaluator (a few
dozen lines: variables = `output`, `counter`, `fill[i]`;
operators = comparisons, boolean conjunction, arithmetic). Every
existing routing kind reduces to a "rules" expression:

- comparator → three rules with `when: output < c / output == c / output > c`
- iterator → one rule using `counter % N` as the band index
- randomizer → one rule using `hash(counter) % N`
- weighted → cumulative-band rules

The benefit: one code path in dispatch instead of five. Custom
routing without writing a spec callback or modifying user
functions. The cost: a small evaluator (still entirely in C, no
spec involvement).

Worth pursuing once Level B has shipped and we have real maps
using the kinds — at that point the consolidation is concrete
rather than speculative. Open as a separate issue when ready.
