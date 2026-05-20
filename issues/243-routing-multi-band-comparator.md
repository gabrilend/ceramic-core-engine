# 243 — Multi-band comparator routing

## Status
open · design carried forward from issue 233

## Current behavior

Issue 233 ships the comparator routing kind with a single
threshold and three fixed branches (`lt` / `eq` / `gt`). The
generalisation — N thresholds defining N+1 numeric bands — is
the natural next step but ships separately so the schema-and-
inspector work it carries doesn't bloat 233.

## Intended behavior

A comparator with a list of thresholds defines N+1 output
branches, each named for the band it fires on:

```json
"routing": {
  "kind":       "comparator",
  "thresholds": [3, 7, 11, 17]
}
```

Five output ports get named `below_3`, `between_3_7`,
`between_7_11`, `between_11_17`, `above_17`. The single-
threshold case (`thresholds: [c]`) reduces to the band `[below_c,
above_c]` — different shape from today's three-branch lt/eq/gt.
To recover the lt/eq/gt semantics, **doubled thresholds** mark
zero-width equality bands:

```
thresholds: [3, 3]         → < 3      / == 3   / > 3
thresholds: [3, 3, 7, 7]   → < 3 / == 3 / 3<x<7 / == 7 / > 7
```

This means the comparator shipped in 233 is exactly
`thresholds: [c, c]` under the multi-band shape, and the editor
can present the single-comparand UI while storing the doubled
form so the dispatch layer sees one consistent representation.

### Schema

```json
"routing": {
  "kind":       "comparator",
  "thresholds": [3, 7, 11, 17]
}
```

`thresholds` is a non-empty array of numbers. Non-decreasing
(doubled values are valid, decreasing values are rejected). The
schema also accepts the legacy single-comparand form for one
release as a transition aid:

```json
"routing": { "kind": "comparator", "comparand": 5 }
```

The editor normalises this to `thresholds: [5, 5]` on next save.

### Branch picker (dispatch layer)

Linear scan over thresholds; zero-width bands require equality
checks. For `thresholds: [t0, t1, …, t_{n-1}]`:

```
value <  t0          → below_<t0>
value == t0 == t1    → == <t0>          (when t0 == t1)
t_i < value < t_{i+1}→ between_<t_i>_<t_{i+1}>
value > t_{n-1}      → above_<t_{n-1}>
```

Doubled thresholds match equality; non-doubled boundaries match
the open interval. The picker is straight-line code; profiling
might justify a binary search if thresholds gets long, but
typical use is N ≤ 8.

### Inspector UI

Two control modes share the same dropdown selection:
- **Simple comparator** (the 233 UI): a single number input.
  Stores `thresholds: [c, c]`.
- **Multi-band**: a sortable list of number inputs, with `+
  threshold` and `× threshold` buttons. Reordering enforces non-
  decreasing order on save.

A toggle between the two presentations sits next to the
threshold list. The "Simple" mode is the default for new
comparator boxes; the user opts into multi-band when they need
it.

## Suggested implementation steps

1. `src/001-schema.lua` — accept `thresholds` array as a
   comparator alternative to the legacy `comparand`. Validate
   non-decreasing.
2. `src/010-graph-loader.c` — read the thresholds array into the
   box's `routing` struct.
3. `src/012-dispatch.c` — replace the lt/eq/gt picker with a
   threshold-band lookup. The single-threshold doubled case
   reduces to the same code path.
4. `assets/js/004-inspector.js` — add the multi-band UI under
   the comparator dropdown choice. Keep the simple-comparand UI
   as the default visible to the user.
5. `assets/js/002-boxes.js` — render output ports named
   `below_<t>` / `between_<a>_<b>` / `above_<t>` based on the
   thresholds array.
6. Migration: the editor reads legacy `comparand` and emits
   `thresholds: [c, c]` on the next save. Hand-migrate any
   maps not opened in the editor.
7. Fixture: `tests/maps/multi-band-comparator/` — four
   thresholds, sample inputs hitting every band including
   doubled-equality.

## Relevant files

- `issues/completed/233-unified-routing-schema.md` — parent.
- `src/001-schema.lua`, `src/010-graph-loader.c`,
  `src/012-dispatch.c` — runtime side.
- `assets/js/004-inspector.js`, `assets/js/002-boxes.js` —
  editor side.

## Open questions

- **Edge inclusion**: `x == threshold` (non-doubled) — which
  side does the value land on? Lean toward
  `less-than-or-equal` for consistency. Document the choice in
  the dispatch code's comment so a reader doesn't have to guess.
- **Port-name length**: long threshold values produce long
  port names (`between_-1234567_2345678`). The canvas already
  truncates with ellipsis (issue 235's MAX_LABEL_CHARS rule).
  Probably fine; flag if it becomes ugly.
- **Threshold mutation invalidates wires**: changing a
  threshold renames a port (e.g. `between_3_7` → `between_3_8`).
  The editor should rewrite outgoing wire `from_branch` to
  match — same machinery as the iterator slot rename used to
  use under the legacy schema.
