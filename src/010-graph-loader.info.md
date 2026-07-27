# 010-graph-loader.c — public surface

C graph loader (phase 3). Reads a map directory, parses every
JSON file, and produces an in-memory `graph_t` the dispatch layer
and pool runner consume. Replaces `src/003-loader.lua`.

Loading is two calls, deliberately split. `graph_load` is pure
parse-and-validate and touches no runtime state; `graph_attach_runtime`
is the second pass that binds the parsed graph to a slot store and
a spec registry. A tool that only wants to inspect or validate a
map calls the first and never the second.

## Lifecycle

- `graph_t *graph_load(const char *map_dir, char **err)` — read +
  parse + validate. Returns NULL on any failure and sets `*err` to
  a malloc'd `"<file>:<line>: <message>"` diagnostic. The caller
  frees `*err`.
- `int graph_attach_runtime(g, slots, specs, default_cell_bytes, err)`
  — allocate a slot per input port, pick each port's input method,
  resolve each call box's language spec. Returns 0 on success.
- `void graph_destroy(graph_t *g)` — frees every byte the graph
  owns (the box and connection arrays, the input declarations, the
  underlying JSON arena holding every string).

## Accessors

- `graph_name(g)` / `graph_description(g)` / `graph_map_dir(g)`
- `graph_n_boxes(g)` / `graph_box(g, i)` / `graph_box_by_id(g, id)`
- `graph_box_index(g, id)` — id to integer index.
- `graph_n_entry_boxes(g)` / `graph_entry_box(g, i)` — **the real
  entry set**, derived from topology. This is what the pool runner
  submits.
- `graph_entry_box_id(g)` — the raw `meta.json` string. Consumed
  only by the runner's startup banner; it does not select
  anything. Issue 206 (entry box designation) removes it.
- `graph_n_languages(g)` / `graph_language(g, i)` — the distinct
  languages this map uses, so pool init can skip specs no box
  needs.
- `graph_n_size_classes(g)` / `graph_size_class(g, i)` — the
  distinct output sizes, used to pre-warm the allocator's
  free-lists.

## Public types

- `box_kind_t` — `BOX_CALL` / `BOX_READ` / `BOX_WRITE` / `BOX_MAP`.
- `routing_kind_t` — all seven ship and all seven parse:
  `ROUTING_PLAIN`, `ROUTING_COMPARATOR`, `ROUTING_ITERATOR`,
  `ROUTING_RANDOMIZER`, `ROUTING_WEIGHTED`, `ROUTING_DISTRIBUTOR`,
  `ROUTING_NONLINEARITY`.
- `routing_t` — `kind` plus per-kind params (`n_outputs`,
  `comparand`, `thresholds`, `weights`, `range` / `memory` / `k`).
- `input_decl_t` — `name`, `type`, `literal`, `optional`.
- `connection_t` — one outgoing wire; endpoints resolved to
  integer indices during topology resolution.
- `box_t` — all of the above per box, plus the runtime state
  attached by the second pass.

## What the load actually does

All seven phases ship:

1. **Directory walk** — every `.json` under `boxes/`.
2. **JSON parse** — through `libs/json`.
3. **Schema validation** — per box kind, below.
4. **Topology resolution** — every `to_box` / `to_input` resolved
   to integer indices, every endpoint verified to exist, and
   cycles rejected unless they pass through an iterator.
5. **Slot size class enumeration** — scan declared output
   capacities and collect the distinct sizes.
6. **Entry-box detection** — a box qualifies when it is `BOX_CALL`
   or `BOX_WRITE` and every non-optional input port has zero
   non-read feeders. A zero-input call box qualifies vacuously.
   `BOX_READ` never qualifies: read boxes are pull-on-demand value
   sources and never run as tasks.
7. **Language enumeration** — collect the distinct `lang` values
   and hand the set to the spec registry.

Two cross-cutting passes run alongside: per-edge same-language
classification (which decides whether a wire carries native bytes
or JSON), and encapsulated sub-map splicing, which recursively
loads a `BOX_MAP`'s referenced directory, prefix-renames its box
ids into the parent, and rewires through its externally-marked
read and write boxes. After the splice the `BOX_MAP` record is
inert and never dispatches.

`scan_input_feeders` is the single source of truth for "what feeds
`(box, port)`" — native-edge classification, large-value
detection, per-port slot sizing, size-class enumeration, and
entry-box detection all read from that one walk, so a change to
the definition of "feeder" has one place to land.

## Schema currently enforced

- `meta.json` must be a JSON object containing at least `name`.
- Every `boxes/*.json` is an object with `id` (string, unique
  across the map) and `kind` ∈ `{call, read, write, map}`.
- `call` boxes: require `ref` and `routing`. `fn` is optional (a
  language without function-name dispatch is fine). `lang` is
  optional — inferred from the `ref` extension via the registry.
- `read` boxes: an inline `value` literal OR a `path`; no `ref` /
  `fn` / `routing`.
- `write` boxes: inputs `path` and `value`; pushes the string
  `"true"` downstream after a successful write.
- `map` boxes: require `ref` naming the sub-map directory, plus
  the `inputs` / `outputs` port shape it exposes.
- `routing.kind` must be one of the seven. `comparator` requires
  either `comparand` or a non-decreasing `thresholds` array;
  `iterator` / `randomizer` / `distributor` require `n_outputs`
  (>= 1); `weighted` requires `weights`.

## Known non-conformance

`propagate_multi_spawn` still runs, marking boxes reachable from
an iterator and branching three slot decisions on the result. The
spawn category it computes was retired — every box is multi-spawn
— so the walk and the `multi_spawn` field are being deleted, and
the peek/pop choice moves entirely onto the port. Issue 305 (C
graph loader) carries the change; until it lands, this file
computes a fact nothing should consult.

## Related

- Issue 305 — design.
- Issue 233 — the unified routing schema this loader implements.
- Issue 229 — read / write box kinds.
- Issue 248 — encapsulated map as box.
- Issue 314 — the JSON parser underneath.
- `docs/007-architecture.md` — where this sits in the stack.
