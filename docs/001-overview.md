# SoraMech — Overview & quickstart

## What SoraMech is

SoraMech is a **visual dataflow runtime**. You build a *map* — a
graph of boxes connected by wires — in the browser-based editor.
Each box runs a function (in Lua, C, or Bash). When you run the
map, the runtime fires boxes whose inputs are ready and threads
the outputs along wires to downstream boxes. Multiple boxes run
in parallel across a thread pool.

Most maps are **long-running by design** — that is SoraMech's
primary purpose, not a special case. Because a box fires whenever
its inputs arrive, a map can be wired to feed itself: a loop of
boxes that re-arm each other, turning indefinitely — frame after
frame, request after request, thought after thought. A game's
render tick, an LLM reasoning loop that feeds its own next
prompt, a server or control system polling its inputs — none of
these "finish." They run until a quit signal reaches them. (The
loop must be built from the runtime's cycle-safe constructs — an
iterator-routing box, or a re-arming heartbeat — not a raw
back-edge, which the loader rejects as a deadlock risk; see the
[runtime doc](004-runtime.md).) A map that *does* run dry — a
batch pipeline with no feedback loop — simply hits quiescence and
exits. Both are normal; the circular, always-on shape is the one
the runtime is built around.

The runtime is structural. It guarantees that wires deliver
values from producers to consumers; that boxes fire when their
inputs are ready; that values cross language boundaries cleanly.
It does not know what your boxes *do* — that's your code. The
runtime is the apartment building's steel and concrete; box
authors and library authors design the rooms.

## When to reach for it

SoraMech fits well when:

- The system is **long-running and cyclic** — a loop of boxes
  that feed each other and never quiesce, kept alive until a quit
  signal (game loops, inference loops, servers, sensor pollers).
  This is the shape it's built around.
- The problem is naturally a dataflow: inputs flow through a
  sequence of transformations to outputs.
- You want to mix languages — a Lua planner driving C boxes that
  feed a Bash post-processor.
- You want to inspect the wiring visually instead of reading code.
- The work is naturally parallel — independent boxes should fire
  on different threads without you orchestrating threads yourself.
- You want a **JSONL audit trail** of what happened during a run,
  for debugging, replay, or feeding to a downstream tool.

It's less useful when the problem isn't dataflow-shaped (long
single-threaded computations, tight tensor math, anything where
the box overhead would dominate the work).

## Quickstart

### Install

SoraMech is a small set of C sources, shell scripts, and Lua /
JavaScript assets. You build it from source.

Prerequisites:

- A C compiler with C11 support (`gcc` or `clang`)
- `make`
- `luajit` 2.1 (or compatible) with its headers
- `bash`
- `node` (for the editor's JavaScript tests; optional otherwise)

Build:

```bash
make            # builds the pool runner + the three language specs
make test       # full ship-it suite (unit tests + integration fixtures)
make quicktest  # cheap iteration: just the C unit tests, no rebuild
```

### Your first map

A map is a directory of JSON files. Boxes live in `boxes/`,
source code in `src/`, metadata in `meta.json`. The simplest
map is one Lua box wired to one read box.

```
my-map/
  meta.json
  boxes/
    seed.json
    greet.json
  src/
    greet.lua
```

`meta.json`:

```json
{
  "name": "my-map",
  "description": "Tiny example",
  "entry_box_id": "seed",
  "src_dirs": ["my-map/src"]
}
```

`boxes/seed.json` (a value source — emits the literal `"world"`):

```json
{
  "id":    "seed",
  "kind":  "read",
  "value": "world",
  "connections": [
    { "from_box": "seed", "to_box": "greet", "to_input": "name" }
  ]
}
```

`boxes/greet.json` (a Lua call box):

```json
{
  "id":    "greet",
  "kind":  "call",
  "lang":  "lua",
  "ref":   "src/greet.lua",
  "fn":    "greet",
  "inputs":  [ { "name": "name", "type": "string" } ],
  "routing": { "kind": "plain" }
}
```

`src/greet.lua`:

```lua
local M = {}
function M.greet(name)
    return "Hello, " .. name .. "!"
end
return M
```

Run it:

```bash
./soramech-pool my-map
```

Output:

```
soramech-pool: 'my-map' — 2 box(es), entry seed, N worker(s)
soramech-pool: run log → /tmp/soramech-last-run.jsonl
soramech-pool: run complete (1 task(s) in 234 µs).
soramech-pool: outputs:
  greet → Hello, world!
```

The seed box pushed `"world"` along its wire to `greet.name`.
The Lua function ran and returned `"Hello, world!"`, which is
the output captured for `greet`. The full per-event run log
sits at `/tmp/soramech-last-run.jsonl` — one JSON event per
line, ready to feed to whatever downstream tool you like.

### Editing in the browser

For anything more than a toy, you'll want the editor:

```bash
./scripts/start-server.sh
# open http://localhost:7700/ in your browser
```

The editor's a canvas-and-inspector view of the same map
directory. Drag to create boxes, click-drag between port dots
to wire them, click a box to edit its fields in the right
sidebar. Saves go straight to the JSON files on disk; you can
run the map from another terminal at any time.

## What's next

- [`docs/002-map-model.md`](002-map-model.md) — the box kinds,
  routing kinds, wire semantics, and how encapsulated sub-maps
  let you compose larger graphs from smaller ones.
- [`docs/003-editor.md`](003-editor.md) — the canvas + inspector
  UI walkthrough.
- [`docs/004-runtime.md`](004-runtime.md) — how runs work, what
  the JSONL log contains, the compile pipeline, the
  reference-counted artifact system.
- [`docs/005-writing-boxes.md`](005-writing-boxes.md) — per-
  language quickstarts for Lua, C, and Bash box authors.
- [`docs/006-test-coverage-map.md`](006-test-coverage-map.md) —
  the project's test inventory.
