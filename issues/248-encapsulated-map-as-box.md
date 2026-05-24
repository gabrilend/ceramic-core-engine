# 248 — Encapsulated map as box

## Status
in progress · input-side load-time inlining shipped; output-side
(ext-consumed write boxes), editor UI, and recursion across mixed
ext-consumed wires deferred

## Current behavior

Input-side encapsulation runs at graph load. The loader recognizes
`kind: "map"` with a `ref` to a sub-map directory. A new pass between
`load_boxes` and `resolve_topology` walks every BOX_MAP record,
loads the sub-map's boxes into the parent's arena, prefix-renames
their ids with `<encap_id>__<sub_id>`, splices them into the parent's
flat box list, then rewires each parent producer's connection that
targeted the encapsulating box. The rewire splices THROUGH the
externally-supplied data box: each parent wire to `encap.port` is
replaced with copies pointing to the matching data box's
downstreams, preserving each from_branch tag. Externally-supplied
data boxes are matched to the encapsulating box's input ports by
the issue's three binding kinds (positional / numbered by index,
named by string).

After inlining, the encap box stays in the index but is inert
(connections cleared, `ref` stripped — the loader skips it on
subsequent passes, dispatch's BOX_MAP case is a logged no-op). The
ext-supplied data box is also orphaned (its outgoing connections
were absorbed into the parent producer; the cache pass skips it so
the "neither value nor path" check doesn't reject the value-less
shape).

The end-to-end fixture `tests/maps/248-encap-input-only/` proves
the splice reaches disk: a parent read box wires
`"encapsulated-greeting"` to a BOX_MAP whose sub-map has one
externally-supplied data box feeding a write box; the parent's
bytes land in `/tmp/soramech-248-encap-out.txt`.

Output-side encapsulation (externally-consumed write boxes
surfacing as the encap's output ports) is not yet wired. Wires
the user drew from `encap.port_x` to a parent consumer don't fire
in this slice. The editor toggles, file-browser Encapsulate
action, and the multi-output exception to issue 218 also remain
ahead.

## Concept

A map directory is normally opened via the map-select UI as the
top-level graph being edited. **This issue adds a second way to
open a map:** through the file browser, as an *encapsulated box*
on the currently-edited parent map. The opened map becomes a
single box on the parent's canvas. Its inputs come from `data`
boxes inside the sub-map that have been marked
**externally-supplied**; its outputs come from `write` boxes
inside the sub-map that have been marked **externally-consumed**.
The two markings are symmetric: one redirects an input from "file
on disk" to "wire from parent," the other redirects an output
from "file on disk" to "wire to parent."

Encapsulation is a function-like abstraction over a sub-graph.
The parent doesn't see the sub-map's internals; it sees one box
with declared input ports, declared output ports, and known
semantics. The sub-map can be edited independently, reused across
parent maps, and packaged once for distribution.

**Encapsulated boxes are the documented exception to issue 218's
"single output per box" rule.** Every other box kind has exactly
one output (with fan-out via multiple wires leaving that one
output). An encapsulated map has *N* distinct output ports, one
per `write` box marked externally-consumed. *N* may be zero (the
map is a pure side-effect sink), one (matches the common case),
or many. The exception exists because an encapsulated map's
outputs are independent values produced by independent
`write` boxes — packing them into a single blob (the earlier
design in this issue) would have repurposed file-writing
machinery to emit strings into wires, which is the wrong shape:
`write` boxes should keep their semantics ("terminate a value")
and the destination kind ("file vs. parent wire") should be a
mode toggle, not a substitution.

## Marked input ports inside the sub-map

A **data** box inside the sub-map (kind: `data`, the existing
language-agnostic file-IO source) can be marked as
**externally-supplied** via a small toggle on the box's
inspector. Toggling declares: "this data box's value comes from
the encapsulating context, not from its `path` or `value`
field."

The toggle is intentionally scoped to `data` boxes — those are
the natural entry points for external values into a map. Call
boxes / read boxes / write boxes don't carry the toggle, which
keeps the inspector uncluttered for the common cases. Each
external input becomes a small shared-memory chunk seeded by
the encapsulating call's invoke; the marked data box reads from
that chunk on each fire, treating it identically to a value
loaded from disk.

The toggle records the port's binding kind on the box JSON:

```json
{
    "id": "sum_input",
    "kind": "data",
    "external": {
        "kind": "named",            // or "positional" / "numbered"
        "name": "count",            // for named
        "index": 0                  // for positional / numbered
    },
    "...": "..."
}
```

Three binding kinds capture how the encapsulating box's input
maps to this internal port:

- **`positional`** — bound by argument order. The encapsulating
  box has N input ports; the *k*th port's value flows to the
  internal box whose `external.index` is *k*. Conceptually
  matches positional function arguments.
- **`numbered`** — bound by an explicit integer index. Same shape
  as positional but the user picks the number (so they can leave
  gaps, reorder freely, etc.).
- **`named`** — bound by a string name. The encapsulating box's
  input port has a label; the matching internal box's
  `external.name` is the same string.

A single sub-map can mix kinds across its marked boxes — some
positional, some named — and the parent map's encapsulated box
exposes the union as its input ports.

## Marked output ports inside the sub-map

A **write** box inside the sub-map (kind: `write`, issue 229's
file-write kind) can be marked as **externally-consumed** via a
small toggle on the box's inspector — the mirror of the
externally-supplied toggle on data boxes. Toggling declares:
"this write box's value goes to the encapsulating context, not
to its `path` field."

The toggle is intentionally scoped to `write` boxes — those are
the natural exit points for values out of a map. Each
externally-consumed write box surfaces as one **distinct output
port** on the encapsulating box. The port carries that one
value, on its own wire, to whichever downstream box(es) the
parent wires it to.

The toggle records the port's binding kind on the box JSON, in
the same shape as the input-side toggle:

```json
{
    "id": "result_value",
    "kind": "write",
    "external": {
        "kind": "named",            // or "positional" / "numbered"
        "name": "result",           // for named
        "index": 0                  // for positional / numbered
    },
    "...": "..."
}
```

Three binding kinds — same vocabulary as the input side:

- **`positional`** — bound by argument order. The encapsulating
  box's *k*th output port is the externally-consumed write box
  whose `external.index` is *k*.
- **`numbered`** — bound by an explicit integer index, allowing
  gaps and reordering.
- **`named`** — bound by a string name. The encapsulating box's
  output port carries that label.

A write box marked externally-consumed **also** writes to its
`path` if one is set — the two destinations are independent. The
write box can opt out of the file write (set `path: null`) if it
exists solely to surface a value to the parent.

Each externally-consumed write box's emitted value becomes an
independent output wire on the encapsulating box. The earlier
design in this issue packed all write-box outputs into a single
byte-blob with a leading size index; that design is rejected —
see the "Concept" section for why. No packing, no unpacking
library; the wire format is the same per-port wire format every
other box uses.

## File layout in the parent map

**The encapsulating box is a normal `call` box.** From the
editor's perspective it has the same JSON shape as any other
call box — `kind: "call"`, `lang`, `ref`, `fn`, `inputs`,
`connections`, `routing`. What makes it an encapsulation is
that its `lang` is `map` and its `ref` points to a sub-map
directory rather than a source file. The map-language spec
(`langs/map/spec.so`) is responsible for the "behind the
scenes" processing — running the sub-graph and packing the
outputs.

```json
{
    "id": "stats_pipeline",
    "kind": "call",
    "lang": "map",
    "ref": "../shared-maps/stats-pipeline",
    "fn": null,                                  // unused for map kind
    "inputs": [
        { "name": "count", "type": "int" },
        { "name": "label", "type": "string" }
    ],
    "outputs": [
        { "name": "mean",  "type": "float" },
        { "name": "stdev", "type": "float" },
        { "name": "n",     "type": "int" }
    ],
    "...": "..."
}
```

The `outputs` array on the encapsulating box JSON is the
documented exception to the single-output-per-box rule. The
graph loader treats each entry as a distinct output slot,
allocates one ring buffer per output port (same machinery as
input slots, but on the producer side), and lets the parent
wire each output independently.

The map-language spec is registered in the spec registry the
same way `lua` / `c` / `bash` are. Its `file_ext` is `.map`
(or no extension if the ref points to a directory). The graph
loader resolves the `ref`, the map spec's `init` (per worker)
loads the sub-graph, and its `invoke` runs the sub-graph and
packs the outputs. From the dispatch layer's view, an
encapsulating call is just another spec invocation.

The sub-map's input ports (derived from its marked data boxes)
and output ports (derived from its marked write boxes) are
exposed on the encapsulating box's JSON as `inputs` and `outputs`
arrays. The user doesn't write either array themselves — the
editor generates them from the sub-map on first encapsulation
and re-syncs them when the sub-map changes.

## Editor UI

### File browser

The file browser (issue 207's source-file-browser, or its
descendant) shows map directories with a distinct icon — a
folder-like glyph annotated to suggest "this is a runnable
graph, not a source file." Clicking opens a small menu:

- **Edit this map** — switches the editor's active map (the
  existing map-select behaviour).
- **Encapsulate** — adds the map as a `kind: "map"` box to the
  currently-edited parent map. The new box appears on the
  canvas at the cursor position; the user wires it in like any
  other box.

### External-binding toggle (inputs and outputs)

The inspector for a `data` box's value-input grows a small
toggle that marks it externally-supplied; the inspector for a
`write` box's value-input grows the same toggle that marks it
externally-consumed. Both surface in the same visual idiom:

```
  ┌─────────────────────────────┐
  │ count   [● external]        │ ← toggle on, port is externally-bound
  │   kind:    named            │
  │   name:    "count"          │
  └─────────────────────────────┘
```

When the toggle is on, the inspector reveals the kind selector
(positional / numbered / named) and the name / index field.
When off, the port behaves normally (wires-in / literal value on
the data box; `path` write on the write box).

A map with no marked data boxes encapsulates to a no-input box
(just a source of values). A map with no externally-consumed
write boxes encapsulates to a box with no output (side-effecting
only — legal but unusual; useful for "fire and forget"
sub-pipelines).

### Visual on the canvas

The encapsulating box renders distinctly — different border,
maybe a small "sub-map" badge — so the user sees at a glance
that this isn't a leaf call. Hovering shows the sub-map's name
and a small thumbnail of its graph.

## Runtime model — two paths, needs a decision

Two implementation paths exist. The encapsulating box being a
normal `call` box from the editor's perspective doesn't pick
between them; the choice lives in what the map-language spec
does behind the scenes and what (if anything) the compile
pipeline does at build time.

### Path A — Compile-time inlining

The compile pipeline (issue 309) recognizes encapsulating call
boxes and **expands them into their sub-graphs at build time**.
Steps the compile step takes per encapsulating box:

1. Load the sub-map's graph from `ref`.
2. Rename every internal box with a parent-namespaced id:
   `stats_pipeline__compute`, `stats_pipeline__filter`, etc.
3. Copy every internal wire across, with the renamed ids.
4. For each marked data box inside the sub-map: replace it with
   a wire that brings the encapsulating box's matching input
   value directly to wherever the data box's output was wired.
5. For each externally-consumed write box inside the sub-map:
   route its emitted value to the matching output port on the
   encapsulating box (so downstream consumers in the parent map
   see the value on a distinct wire per write box). The write
   box still performs its disk-write if `path` is set.
6. Remove the encapsulating call box from the parent's graph.

After expansion, the compiled artifact is **one flat graph**.
The runtime never sees `lang: "map"`. The map-language spec
exists only for the editor's sake (validating refs, listing
the sub-map's inputs/outputs for the inspector).

**Pros:**

- No new runtime machinery. The dispatch, slot store, and spec
  registry all operate on flat graphs as today.
- Best per-call performance — no sub-graph startup cost, no
  nested dispatch.
- **Composes cleanly with issue 313** (whole-program same-
  language merge). The inlined sub-graph's same-language regions
  merge into the parent's merge as if they were always one map.
- Debugging stack traces inside the inlined region point to
  flat box ids — readable once you know the naming convention.

**Cons:**

- The sub-map gets baked into every parent that uses it. Edit
  the sub-map → every parent must recompile.
- No hot-reload of sub-maps. The editor would have to
  re-expand on every parent map edit, or accept that the
  parent's last-compiled artifact lags the sub-map source.
- Compile time grows with sub-map size × number of
  encapsulations.
- Deep recursion (sub-maps inside sub-maps inside sub-maps)
  inflates the parent's compiled output proportionally.
- The multi-output exception to issue 218 has to be carried
  through compile-time inlining as well: after expansion, the
  parent's flat graph has wires going from internal sub-graph
  boxes directly to whichever parent consumers were wired to
  the encapsulating box's output ports. The compile-time
  artifact loses the "encapsulated box" identity entirely but
  preserves the per-output routing.

### Path B — Runtime sub-graph dispatch

The encapsulating box stays as one box in the parent's flat
graph. The map-language spec's `invoke` runs the sub-graph at
call time:

1. **`map_init(worker_idx)`** — load every sub-map referenced
   by the running program (or lazily on first invoke). Each
   worker gets its own copy of the sub-graph state (slot store,
   dispatch context).
2. **`map_invoke(handle, sub_map_dir, ..., inputs, ..., outputs, ...)`** —
   for each input, write the bytes into the shared-memory chunk
   that the matching marked data box reads from. Then drive the
   sub-graph to quiescence using the existing pool's dispatch
   loop (the sub-graph's tasks are normal pool tasks distinguished
   by a nested namespace). When quiescent, walk each
   externally-consumed write box and push its emitted value onto
   the matching output port of the encapsulating box. Each output
   port surfaces on its own wire to the parent — no packing.

The sub-graph's parallelism uses the same thread pool as the
parent's. A sub-graph's tasks compete for workers alongside the
parent's tasks. From the pool's perspective, the difference is
just box-id prefix.

**Pros:**

- Sub-map loaded once per worker, reused across many parent
  invocations and across many parent maps.
- Hot-reload friendly: change the sub-map, parent picks up the
  new version on next run (no parent recompile).
- The multi-output boundary is clean — each externally-consumed
  write box pushes onto its own output port wire as it would
  if it were a leaf box in the parent graph, no packing or
  unpacking required.
- Debugging stack traces preserve the sub-map identity (the
  task's namespace says "this came from `stats-pipeline`").
- Recursion is natural — the spec's invoke can nest-invoke its
  own spec for a sub-sub-map.

**Cons:**

- New runtime machinery: the map spec's `invoke` runs a nested
  dispatch loop. Either the dispatch layer grows a re-entrancy
  contract, or the map spec replicates enough of the dispatch
  machinery to drive the sub-graph itself.
- Each encapsulating-box call pays sub-graph startup cost —
  seeding inputs, possibly resetting slot state from a previous
  invocation, collecting outputs at the end. Small but real.
- Does NOT compose with issue 313's same-language merge — the
  sub-map's source can't merge with the parent's, because at
  build time they're separate compilation units. (313 could be
  extended to merge across encapsulation boundaries, but that's
  a separate design.)
- Re-using the sub-graph across invocations requires reset
  semantics. Either the sub-graph's slots and counters get
  reset between invocations (cost), or each invocation gets a
  fresh sub-graph instance (allocation cost).

### What to weigh

The choice turns on whether encapsulation is **a build-time
abstraction (like a C macro or a Lua-function-inlined-by-LuaJIT)**
or **a runtime composition (like calling a function in a shared
library)**.

Path A treats it as a build-time abstraction. Encapsulation is
how the user organizes source; the compiled artifact is one
flat program. Best for raw performance and integration with
the existing machinery. Cost: rebuild propagation.

Path B treats it as a runtime composition. Encapsulation is a
real boundary at execution time, with the packed-blob output
as the wire format across the boundary. Best for modularity,
hot-reload, and clear semantics. Cost: per-call overhead and
loss of cross-boundary optimization.

A third option exists — **support both, picked per encapsulating
box** via a small flag in the box JSON (`encap_mode: "inline" |
"runtime"`). The compile pipeline expands inline-mode boxes;
runtime-mode boxes stay encapsulated. Each user picks per
encapsulation based on their needs. Higher implementation cost
but maximally flexible. Probably not in the first slice; flag it
for later if either A or B alone proves too narrow.

Recursion (a sub-map encapsulates another sub-map) works under
either path. Cycles between maps are a compile-time (or
load-time, under B) error.

## Relationship to existing issues

- **Issue 207 (source-file-browser)** — the file browser is the
  entry point for the "encapsulate" action.
- **Issue 239 (literal port shrinks to nodule)** — the
  external-input slider reuses the same visual idiom on the
  input-port row.
- **Issue 229 (data box as language-agnostic file IO)** —
  encapsulation adds an "externally-supplied" toggle to data
  boxes and an "externally-consumed" toggle to write boxes. The
  boxes' default behaviour (read from / write to disk) is
  unchanged; the toggles route the value through a parent-wire
  instead of (or in addition to) the disk file.

- **Issue 218 (single output per box)** — the encapsulated
  `kind: "map"` box is the documented exception. Every other box
  has exactly one output; an encapsulated box has one per
  externally-consumed write box (zero or more).
- **Issue 309 (build system / compile pipeline)** — the
  compile-time inlining path (option A above) extends the
  pipeline with an "encapsulation expansion" step.
- **Issue 313 (whole-program same-language merge)** — composes
  with compile-time inlining; the inlined sub-graph's same-
  language regions merge into the parent's merge as if they
  were always part of one map.

## Relevant files

- `assets/js/002-boxes.js` — box state, the `kind: "map"` box,
  the per-input-port external-binding state.
- `assets/js/005-app.js` — file browser; menu with Edit /
  Encapsulate actions.
- `assets/index.html` — inspector UI for the external-binding
  toggle.
- `src/006-server-main.lua` — HTTP backend; resolving sub-map
  paths, validating bindings.
- `src/010-graph-loader.{c,h}` — graph loader; recognise
  `kind: "map"`, resolve the sub-map, build the encapsulated
  shape.
- `src/012-dispatch.c` — encapsulating-box dispatch (runtime
  path) or compile-time expansion (compile path).
- `scripts/soramech-compile.sh` — encapsulation expansion if the
  compile-time path wins.

## Suggested implementation sequence

1. **Box JSON schema**: add `kind: "map"`, the `ref` field, and
   the per-input `external` block. Graph loader rejects with a
   clear error if any field is missing where required.
2. **External-binding toggles**: inspector toggle on data boxes
   (externally-supplied) and write boxes (externally-consumed);
   kind / name / index selectors when on. Same visual idiom for
   both.
3. **File browser "Encapsulate" action**: adds a `kind: "map"`
   box to the parent at the cursor position. Reads the sub-map's
   marked data boxes and marked write boxes to populate the new
   box's `inputs` and `outputs` arrays.
4. **Graph loader sub-map resolution**: load the sub-map's
   `meta.json` and `boxes/`; record its marked-input and
   marked-output lists on the parent box record.
5. **Multi-output slot allocation**: the encapsulated box gets
   one output slot per externally-consumed write box (the
   documented exception to issue 218). Graph loader and slot
   store accept this shape only for `lang: "map"` boxes.
6. **Pick A or B**: compile-time inlining or runtime sub-graph
   dispatch. Prototype the simpler of the two end-to-end on a
   trivial sub-map (`maps/encap-trivial`).
7. **Recursion**: nested encapsulation; cycle detection at
   compile (or load) time.
8. **Editor visualization**: distinct canvas rendering for
   encapsulating boxes; tooltip / thumbnail.

## Open questions

- **Sub-map identity for the editor**: when the parent
  encapsulates `maps/<sub>`, what happens if the user later
  renames or moves `<sub>`? The reference path is brittle.
  Options: store a stable id in the sub-map's `meta.json` and
  reference that, plus a path hint; or require the sub-map to
  live inside the parent's map directory tree.
- **Re-sync behaviour when the sub-map changes**: if the user
  edits the sub-map and changes its marked inputs, the parent's
  encapsulating box's `inputs` array drifts out of date. Likely
  resolution: on parent map load, the loader re-derives the
  inputs from the sub-map and warns the user if wiring needs
  fixing.
- **`.map` packaging format**: this issue assumes sub-maps are
  directories. A single-file `.map` archive (tarball / zip)
  would be more portable for distribution. Out of scope here;
  separate ticket if it becomes relevant.
- **Output naming consistency**: outputs use the same
  positional / numbered / named binding kinds as inputs. The
  question is whether a single encapsulated box can mix kinds
  across outputs (e.g. two named, one positional), or whether
  all outputs must share a kind. Inputs already allow mixing —
  preferable to keep outputs symmetric.
- **Side-effect ordering**: the sub-map's write boxes run as
  part of the sub-graph and write to disk. The encapsulating
  box's output emits *after* those side effects complete. If
  the parent wires the encapsulating box's output forward, the
  downstream task can rely on the writes having happened. Worth
  documenting; not a design choice so much as a consequence of
  the existing dispatch model.

## Design history

The original 2026-05-21 design packed all write-box outputs into
a single byte-blob with a leading null-terminated size index, and
the encapsulated box had a single output wire carrying that blob:

> "the write data slots are the outputs, written to a chunk of
> heap memory. At the beginning is an array of sizes — null
> terminated. Then, immediately afterwards, is the first index
> of data..." — 2026-05-21

That design was reconsidered on 2026-05-23 while specifying the
self-construction built-ins (issue 319). The reconsideration:
boxes never "return" values to the outside world under the
existing model — they push values onto wires that other boxes
consume, and `write` boxes specifically terminate values to
files. Repurposing the file-writing destination to emit strings
into wires would have been a substitution of one mechanism for
another. The symmetric design — mark write boxes
externally-consumed, surface each as its own output port —
preserves the `write` box's semantics ("terminate a value") and
treats the destination as a mode toggle. Cost: multi-output boxes
exist for this kind alone; benefit: one mechanism does both jobs
cleanly.
