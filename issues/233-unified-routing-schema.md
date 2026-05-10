# 233 — Unified `routing` schema for branching boxes

## Status
open

## Current behavior

Two different ways to express "this box has multiple output
branches and only fires one per invocation," depending on which
flavor:

- **Comparator**: per-box `comparand: "<number>"` field. Output
  branches are fixed `lt` / `eq` / `gt`. The function still runs;
  the dispatch layer compares the function's output against
  `comparand` to pick a branch.
- **Iterator**: per-box `iterator_outputs: ["a","b","c"]` field.
  Output branches are user-named. No function runs; the dispatch
  layer routes the input to `outputs[counter % N]` (counter from a
  `SLOT_ATOMIC_COUNTER` slot, issue 302).

Both are "branching boxes" with different routing rules. The
schema makes them look like unrelated features.

## Intended behavior

A single `routing` field on the box, with kind-specific
parameters:

```json
{ "id": "score-router",
  "kind": "call",
  "ref": "src/score.lua", "fn": "score",
  "inputs": [...],
  "routing": { "kind": "comparator", "comparand": 4 } }

{ "id": "round-robin",
  "kind": "call",
  "routing": { "kind": "iterator", "outputs": ["worker_a","worker_b","worker_c"] } }

{ "id": "plain-call",
  "kind": "call",
  "ref": "...", "fn": "...",
  "inputs": [...] }
  // no routing field — single output wire, fires unconditionally
```

Absence of `routing` means plain call (single unconditional output
wire). Presence means branched output, with the routing kind
deciding how the branch is picked.

### Comparator: kind="comparator"

```json
"routing": { "kind": "comparator", "comparand": "<number-as-string>" }
```

Function runs. Dispatch compares the function's output against
`comparand`; fires `lt` / `eq` / `gt` branch.

### Iterator: kind="iterator"

```json
"routing": { "kind": "iterator", "outputs": ["a","b","c"] }
```

No function runs (or function runs and its output passes through
— design decision below). Dispatch reads a `SLOT_ATOMIC_COUNTER`
slot via `slot_read_inc(counter_slot, len(outputs))`; fires the
named branch at that index.

### Future kinds

- `kind: "function_decided"`: function returns `(value, branch_tag)`;
  dispatch fires the branch tagged. Reserved name; not implemented
  by phase 3 unless we need it.
- `kind: "user_router"`: spec callback decides the branch given
  inputs and current state. Reserved.

The `routing.kind` field is the one place new routing types plug
in.

## Iterator + function: open question

Currently iterator boxes have no `ref` / `fn` — they're routing-
only. The user has expressed interest in "functions iterating
their own outputs" (a function-backed iterator). Two possible
semantics under the unified schema:

1. **Iterator passes input through unchanged** (current). The
   function would be a no-op; not allowed. Box has no `ref`/`fn`.
2. **Iterator runs function, routes function's output** (new).
   Box has `ref`/`fn`; function runs; output goes to
   `outputs[counter % N]`. Counter still drives routing, but the
   value being routed is the function's return.

(2) is more general and includes (1) as a special case (identity
function). It composes with comparator's structure (function runs,
output gets routed). The dispatch action becomes:

```
1. Read inputs.
2. If box has ref/fn: invoke spec, take output.
   Else (iterator-only): take the popped input value.
3. Pick branch via routing.kind:
     comparator → compare(value, comparand) → lt/eq/gt
     iterator   → slot_read_inc(counter_slot, N)
4. Push value to outputs[picked branch].
```

This unifies both paths and makes iterator-with-function natural.

Recommendation: ship (2). The editor can default new iterator
boxes to no-function (the simple case), but allow attaching a
function via the existing `ref`/`fn` controls.

## Schema migration

Existing maps:
- Boxes with `comparand` and no `routing` field → migrate to
  `routing: { kind: "comparator", comparand }` at load time.
- Boxes with `iterator_outputs` and no `routing` field → migrate
  to `routing: { kind: "iterator", outputs: iterator_outputs }`.
- Plain call boxes (no `comparand`, no `iterator_outputs`) → no
  routing field, no migration needed.

Migration runs in the loader, in memory. Editor saves migrated
boxes back to disk on next edit. Old `comparand` / `iterator_outputs`
fields are accepted on read (legacy) but written under `routing`.

## Inspector UI

A `routing` selector in the inspector replaces the separate
`compare` toggle and `iterator` toggle:

```
mode:    [plain ▾ | comparator | iterator]
```

When `comparator` selected: show `comparand` text input + lt/eq/gt
output dots.
When `iterator` selected: show editable `outputs` slot list with
auto-grow (current iterator UI). Optionally allow ref/fn (for the
function-backed iterator semantics if option (2) ships).

## Suggested implementation sequence

1. `src/001-schema.lua`: accept `routing` field; legacy `comparand`
   and `iterator_outputs` accepted on read, validated as
   equivalent.
2. `src/003-loader.lua` (phase 2 path): migrate legacy fields into
   `routing` on load.
3. `src/005-http-server.lua`: PUT handler accepts both shapes
   (canonical and legacy) and stores canonical.
4. Phase 3 graph loader (issue 305): same migration.
5. `assets/js/004-inspector.js`: new mode dropdown replacing the
   two separate toggles. Conditional UI per `routing.kind`.
6. `assets/js/002-boxes.js`: render branches based on
   `routing.kind`.
7. `assets/js/006-wires.js`: connection's `from_branch` matches
   `routing.outputs[i]` or `lt`/`eq`/`gt`.
8. Phase 3 dispatch (issue 304): branch on `routing.kind` for the
   branch-selection step. Routing logic consolidates from two
   paths to one.

## Why now

- Editor inspector keeps gaining toggles per routing kind. A
  unified mode dropdown scales linearly with kinds; separate
  toggles don't.
- Phase 3 dispatch will naturally have a "select branch" step;
  routing it through `routing.kind` is the same code regardless
  of whether we ship Level B now or refactor later.
- Schema migration is cheap to do once, expensive to leave for
  later when more maps exist.

## Relevant files

- `src/001-schema.lua` — schema accepts new field
- `src/003-loader.lua` — legacy migration on load
- `src/005-http-server.lua` — PUT handler accepts both shapes
- `assets/js/004-inspector.js` — mode dropdown
- `assets/js/002-boxes.js` — branch rendering
- `assets/js/006-wires.js` — branch matching
- `issues/302-wire-value-slot-store.md` — counter slot mechanism
- `issues/304-task-dispatch-layer.md` — dispatch layer reads
  `routing.kind`
- `issues/completed/210-comparator-wire-branching.md` — original
  comparator design (likely missing/named differently; dig the
  history)
- `issues/completed/221-iterator-box.md` — original iterator
  design

## Open questions

- **Function-backed iterator** semantics — (1) or (2) above.
  Recommendation: (2). Defer to implementation; a simple flag
  `routing.passthrough: true` could opt out per box if needed.
- **Output port order** for comparator — currently fixed
  lt/eq/gt. With unified schema, could `routing.outputs` be
  user-named for comparators too? E.g., `["small","exact","big"]`.
  Probably yes — the comparator's "lt/eq/gt" names are
  conventions, not load-bearing. Editor still defaults to those.
- **Level C deferred**: a full unified router (one dispatch path,
  routing rules as data) is the natural next step. Not blocked by
  this issue; just bigger. Open separately when needed.
