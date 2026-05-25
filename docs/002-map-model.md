# SoraMech — Map model

A map is a directed graph of **boxes** connected by **wires**.
Boxes have **input ports** (left side, where wires arrive) and a
single **output side** (right side, where wires leave). Wires
carry values from one box's output to another box's input port.

## Box kinds

Every box has a `kind` field that tells the runtime what it does.

### `call` — runs a function

The general-purpose box. Names a function in some language
(`lua` / `c` / `bash`) and runs it when the inputs are ready.
The function's return value becomes the value on the output wire.

```json
{
  "id":   "double",
  "kind": "call",
  "lang": "lua",
  "ref":  "src/math.lua",
  "fn":   "double",
  "inputs":  [ { "name": "x", "type": "string" } ],
  "routing": { "kind": "plain" }
}
```

### `read` — emits a value

A pull-on-demand value source. Either a literal (`value` field)
or the bytes of a file (`path` field). When a downstream box
needs a value, the read box's bytes get pulled in.

```json
{
  "id":    "config",
  "kind":  "read",
  "value": "production",
  "connections": [
    { "from_box": "config", "to_box": "router", "to_input": "env" }
  ]
}
```

### `write` — writes to disk

The sink for "land this value on the filesystem." Reads its
`value` input and writes it to the `path` input. By default
emits the string `"true"` downstream as a success signal so
later boxes can chain off the write.

```json
{
  "id":   "save",
  "kind": "write",
  "inputs": [
    { "name": "path",  "value": "/tmp/result.txt" },
    { "name": "value", "type": "string" }
  ]
}
```

### `map` — encapsulated sub-map

An entire sub-graph appearing as one box on the parent canvas.
See "Encapsulation" below for the full story.

### `create_box` / `connect` — runtime self-construction

A running box can spawn new boxes and wire them up mid-run. The
`create_box` and `connect` kinds are the language-agnostic
dispatch primitives; per-language wrappers like Lua's
`soramech.create_box{...}` are convenience over the same path.
See [`docs/005-writing-boxes.md`](005-writing-boxes.md) for the
calling shape.

## Wires (connections)

A wire lives on the **producer** box's `connections[]` array. Each
entry names a destination and (when the routing kind has named
branches) which branch the wire leaves from.

```json
{
  "from_box":    "router",
  "from_branch": "lt",          // optional; depends on routing kind
  "to_box":      "low_handler",
  "to_input":    "value"
}
```

When a producer fires, the runtime pushes its output value into
the destination box's input slot. The destination fires when all
its required input slots have values.

A producer can fan out — multiple wires leaving the same output
port deliver the same value to each consumer. The single-output-
per-box rule (every kind except `map` has one output port)
keeps the wiring discipline simple.

## Routing kinds

The `routing` field on a `call` box decides *where* the output
goes. Seven kinds ship:

### `plain` — fan to every wire

Single output port (unnamed). Every wire on `connections[]`
receives the value. The default for new boxes.

```json
"routing": { "kind": "plain" }
```

### `comparator` — pick by numeric threshold

Two flavours. The single-threshold form names three output
branches `lt` / `eq` / `gt` and picks the matching one based on
the producer's numeric output:

```json
"routing": { "kind": "comparator", "comparand": 5 }
```

The multi-band form takes a non-decreasing `thresholds` array
and carves the number line into N+1 named bands:
`below_<t0>` / `between_<a>_<b>` / `above_<tN-1>`. Doubled
adjacent thresholds carve zero-width equality bands named
`eq_<t>`:

```json
"routing": { "kind": "comparator", "thresholds": [3, 7] }
// → branches: below_3, between_3_7, above_7
```

### `iterator` — round-robin over N ports

No function attached — pure routing. Each fire rotates the
output among `n_outputs` ports named `out_0` ... `out_<n-1>`.
Multi-spawn: a single arrival to an iterator's input slot drains
queued values one per fire.

```json
"routing": { "kind": "iterator", "n_outputs": 3 }
```

### `randomizer` — hashed pseudo-random pick

Same `out_<i>` port shape as iterator. Each fire hashes the
per-box counter and takes mod `n_outputs`, so consecutive fires
spread across branches rather than going to consecutive indices.

```json
"routing": { "kind": "randomizer", "n_outputs": 4 }
```

### `weighted` — probability-weighted pick

Same `out_<i>` ports. Each fire picks a branch by cumulative-
band lookup over the user's `weights` array. The dispatch
normalises automatically — the weights don't have to sum to 1.0.

```json
"routing": { "kind": "weighted", "weights": [0.8, 0.2] }
```

### `distributor` — least-busy pick

Same `out_<i>` ports. Each fire samples the downstream slot
fill on every branch and picks the least-busy one. Ties resolve
via a per-box counter so a steady stream rotates across
equally-empty branches.

```json
"routing": { "kind": "distributor", "n_outputs": 3 }
```

### `nonlinearity` — value-transforming auto-calibrated gate

Single output port. Reads the producer's numeric output,
auto-calibrates bounds from a per-box ring buffer of the last
`memory` observed values, normalises against those bounds,
applies an S-curve, and emits `input × score` (gated linear
unit shape) on the wire.

```json
"routing": {
  "kind":   "nonlinearity",
  "range":  "signed",   // "signed" → tanh, output [-1, 1]
                        // "unit"   → sigmoid, output [0, 1]
  "memory": 16,         // ring-buffer size for auto-calibration
  "k":      1           // steepness
}
```

Useful for decision-lattice composition: chain several into a
downstream aggregator and the soft-AND property of multiplying
scored values bubbles up "strong evidence reinforces, weak
evidence stays weak" behaviour.

## Input port semantics

A `call` box's `inputs[]` array declares its port set. Each port
has a name, an optional `type` (`"string"` is the convention),
an optional inline `value` (a literal that supplies the port if
no wire is attached), and an optional `optional` flag (when
true, the box fires even if this port has no value).

A port can be in one of three states at compile time:
- **Wired** — a producer's `connections[]` entry targets it.
- **Literal** — the port carries an inline `value`.
- **Optional** — neither wired nor literal, but `optional: true`.

Anything else is a hard error at graph load. SoraMech doesn't
silently fire a box with a missing required input.

## Encapsulation — sub-maps as boxes

A whole map can appear as a single box on another map's canvas.
This is `kind: "map"`. The parent's canvas sees one box; the
runtime expands the sub-map at graph-load time into the parent's
flat graph, with the sub-map's box ids prefix-renamed.

How the sub-map exposes ports:

- A **read** box inside the sub-map marked with an `external`
  block becomes one of the encap box's INPUT ports. Parent
  wires landing on that port get spliced through to whatever
  the read box was feeding inside the sub-map.

```json
{ "id": "msg_in", "kind": "read",
  "external": { "kind": "named", "name": "msg" } }
```

- A **write** box marked externally-consumed becomes an OUTPUT
  port on the encap box. The write still happens to its `path`
  (when set); additionally the value gets surfaced back to the
  parent on the wire.

The encap box itself declares the port shape:

```json
{
  "id":   "stats_pipeline",
  "kind": "map",
  "ref":  "../shared-maps/stats-pipeline",
  "inputs":  [ { "name": "msg",    "type": "string" } ],
  "outputs": [ { "name": "result", "type": "string" } ],
  "connections": [
    { "from_box": "stats_pipeline", "from_branch": "result",
      "to_box":   "downstream",     "to_input":    "value" }
  ]
}
```

Encapsulation is the documented exception to the single-output-
per-box rule — an encap can declare any number of output ports
(one per externally-consumed write box). The runtime treats them
as independent value sources.

Encaps nest. A sub-map can itself contain a `kind: "map"` box,
and the load-time splice iterates until no encaps remain. Cycle
detection runs after the flattening.

## What's next

- [`docs/003-editor.md`](003-editor.md) — using the canvas to
  build maps visually.
- [`docs/004-runtime.md`](004-runtime.md) — how the runtime
  executes maps, the JSONL transcript, the compile pipeline.
- [`docs/005-writing-boxes.md`](005-writing-boxes.md) — per-
  language box-author guides.
