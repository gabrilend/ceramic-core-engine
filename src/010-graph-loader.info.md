# 010-graph-loader.c — public surface

C graph loader (phase 3). Reads a map directory, parses every
JSON file, and produces an in-memory `graph_t` the dispatch layer
and pool runner consume. Replaces `src/003-loader.lua`.

## Lifecycle

- `graph_t *graph_load(const char *map_dir, char **err)` — read +
  parse + validate. Returns NULL on any failure and sets `*err` to
  a malloc'd `"<file>:<line>: <message>"` diagnostic. The caller
  frees `*err`.
- `void graph_destroy(graph_t *g)` — frees every byte the graph
  owns (the box and connection arrays, the input declarations, the
  underlying JSON arena holding every string).

## Accessors

- `const char *graph_name(g)`
- `const char *graph_description(g)`
- `const char *graph_entry_box_id(g)`
- `int          graph_n_boxes(g)`
- `const box_t *graph_box(g, i)`
- `const box_t *graph_box_by_id(g, id)`

## Public types

- `box_kind_t` — `BOX_CALL` / `BOX_READ` / `BOX_WRITE`.
- `routing_kind_t` — `ROUTING_PLAIN` / `ROUTING_COMPARATOR` /
  `ROUTING_ITERATOR`; randomizer / weighted / distributor enum
  values exist but the parser doesn't accept them yet.
- `routing_t` — `kind` plus per-kind params (`n_outputs`,
  `comparand`).
- `input_decl_t` — `name`, `type`, `literal`, `optional`.
- `connection_t` — outgoing connection. Stores string refs
  (`to_box`, `to_input`); resolved-to-integer-index forms come in
  the topology iteration.
- `box_t` — all of the above per box.

## Schema currently enforced

- `meta.json` must be a JSON object containing at least `name`.
- Every file under `boxes/` ending in `.json` is a box file with a
  top-level object, `id` (string), `kind` ∈ `{call, read, write}`.
- `call` boxes: require `ref` and `routing`. `fn` is optional (a
  language without function-name dispatch is fine). `lang` is
  optional too — falls back to inference from `ref` extension once
  the spec registry lands.
- `read` boxes (issue 229): an inline `value` literal OR a `path`
  field; no `ref` / `fn` / `routing`. With `value` set, the box
  emits the literal directly and the `path` port hides on the canvas.
- `write` boxes (issue 229): no kind-specific required fields beyond
  inputs `path` and `value`. Pushes the boolean string `"true"`
  downstream after a successful write.
- `routing.kind` ∈ `{plain, comparator, iterator}` (issue 233's
  shipped surface). `comparator` requires `comparand` (number or
  string-form number per the issue's example shape).
  `iterator` requires `n_outputs` (>= 1).
- Box ids must be unique across the map. O(n²) scan; n is small.

## What's NOT here (deferred to follow-on iterations within 305)

- **Phase 4 — topology validation.** Connection endpoints aren't
  resolved to integer indices yet; we don't verify that every
  `to_box` refers to a real box, nor that the `to_input` matches a
  declared port. Cycle detection (excluding iterator-rooted
  cycles) lands here too.
- **Phase 5 — slot size class enumeration.** The slot store's
  `slot_alloc` consumes a `cell_capacity`; the loader's job is to
  scan box outputs and enumerate the distinct sizes the graph
  needs.
- **Phase 6 — entry-box detection.** Boxes with no inputs (or
  inputs fed only by `data` boxes) get auto-detected as entry
  points.
- **Phase 7 — language enumeration.** Walk every call box,
  collect distinct `lang` values, hand the set to the spec
  registry.
- The remaining routing kinds (`randomizer`, `weighted`,
  `distributor`) — issue 233 deferred them to their own
  follow-ons; the parser rejects them with a precise message.

None of these block issues 301 / 303 / 306 from starting.

## Related

- Issue 305 — design.
- Issue 233 — unified routing schema this loader implements.
- Issue 229 — data / file_write box kinds.
- Issue 314 — the JSON parser this depends on.
- Issue 302 — slot store; the loader will eventually hand size
  class enumeration to it.
