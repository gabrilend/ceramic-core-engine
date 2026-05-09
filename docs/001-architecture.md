# SoraMech — Architecture

## Three programs, one data format

SoraMech is three independent programs that share a common on-disk
format: the map directory. None of the three knows about the others
at runtime.

```
  Browser (assets/index.html)
       |  HTTP (file CRUD)
       v
  src/006-server-main.lua       [map directory]
                                maps/<name>/
                                  meta.json
                                  boxes/
                                  data/
                                  src/
                                  compiled/   (after compile button)
                                  tmp/ -> /tmp/<name>/
       ^
       | reads & writes
  Runner (one of):
    src/007-runner-main.lua     ← phase 2, synchronous Lua interpreter
    soramech-pool (binary)      ← phase 3, C thread pool runner
```

### src/006-server-main.lua (the editor's HTTP backend)

Thin file CRUD proxy. Receives JSON requests from the browser and
translates them into reads and writes on the map directory. No logic
beyond path validation and basic consistency checks (e.g. reject a
delete if the box is still referenced by another box's connection
list).

Started with: `luajit src/006-server-main.lua [port]` (default 7700).
The `run` script at the project root starts the server and opens the
editor.

The editor is editor-only — no run button, no run results display.
The runner is invoked separately, by command line. `last-run.jsonl`
(issue 311) is the runner's output, consumed by external tooling, not
by the editor.

### Phase 2 runner — `src/007-runner-main.lua`

Synchronous Lua interpreter. Loads a map directory, validates the
graph, walks boxes in dependency order, invokes language drivers per
box (`drivers/lua.sh`, `drivers/c.sh`, `drivers/bash.sh`). Each box
invocation is wrapped in a `task_fn(inputs) -> outputs` boundary that
matches the phase 3 thread pool's action signature.

This is the development target while the editor is being built out.

### Phase 3 runner — `soramech-pool` (planned, issues 301–311)

C binary that owns a 3d-rts thread pool. Each box invocation is a
pool task; worker threads run them concurrently. Language code is
called through **language specs** (issue 303), shared libraries
(`langs/<name>/spec.so`) loaded via `dlopen` at startup. Lua and C
specs are in-process; the Bash spec talks to a persistent subprocess
over a Unix domain socket. Wire values live in **slots** in process
heap memory (issue 302) — fixed-size with reference counting, plus a
large-value heap for variable-size payloads.

Phase 3 replaces the phase 2 runner wholesale. The synchronous path
does not survive into phase 3; the editor and the map directory
format are unchanged across the cutover.

### assets/index.html (the editor)

Static HTML + vanilla JS. Infinite-scroll canvas. Box diagram editor.
Talks to `src/006-server-main.lua` for all file operations. No run
button, no compile button (yet — issue 222 adds a placeholder
"Compile" button as part of phase 2). The runner is invoked
separately.

## Map directory format

```
maps/<name>/
  meta.json          — { name, description, entry_box_id, src_dirs?, lang? }
  boxes/
    <id>.json        — one file per box (see Box file format below)
  data/
    <name>.json      — Lua table as JSON; sections have "constant": bool
  src/
    *.lua, *.c, *.sh — box function source files
  compiled/          — produced by the compile button (issue 222)
    pool-runner      — the compiled C entry point for this map
    src/             — copy of every source file the map uses
    bin/             — compiled .so files (C boxes via the C language spec)
    manifest.json    — every box, its language, its compiled artifact path
  tmp/               — symlink to /tmp/<name>/
    last-run.json    — phase 2 run snapshot (single document)
    last-run.jsonl   — phase 3 run log (JSON Lines, issue 311)
    logs/            — additional run logs
```

Note: the phase 2 `drivers.json` file is gone. Language selection in
phase 2 is by file extension via the `drivers/` directory; in phase 3
the spec registry resolves languages by `box.lang` against
`langs/<name>/spec.so` — no map-local config file needed.

## Box file format

```json
{
  "id": "trim",
  "label": "Trim whitespace",
  "kind": "call",
  "lang": "lua",
  "ref": "src/strings.lua",
  "fn": "trim",
  "inputs": [
    { "name": "text", "type": "string" }
  ],
  "output_capacity": 4096,
  "connections": [
    {
      "from_box": "trim",
      "from_branch": null,
      "to_box": "next-box-id",
      "to_input": "text"
    }
  ],
  "ui": { "x": 120, "y": 340 }
}
```

Every box has exactly **one output wire**. There is no `outputs`
array. Whatever the function returns travels down the single output
wire as one value (issue 218). Multiple values that need to travel
together are encoded as JSON or a struct and decoded downstream.

`output_capacity` declares the maximum number of bytes the slot
allocator (issue 302) reserves for the output. `0` (or omitted) means
variable-size — the slot stores a handle into the large-value heap.

### Comparator boxes
A box may carry a `comparand` field (a literal number as a string).
When set, the dispatch layer compares the input value to the
comparand and fires only the connection whose `from_branch` matches
`"lt"` / `"eq"` / `"gt"`. Comparators have no `ref` / `fn` — the
routing logic lives in the dispatch layer (issue 304). The output
slot carries the input value through unchanged.

```json
{
  "id": "classify",
  "kind": "call",
  "comparand": "0",
  "inputs": [{ "name": "value", "type": "number" }],
  "connections": [
    { "from_box": "classify", "from_branch": "lt", "to_box": "negative", "to_input": "n" },
    { "from_box": "classify", "from_branch": "eq", "to_box": "zero",     "to_input": "n" },
    { "from_box": "classify", "from_branch": "gt", "to_box": "positive", "to_input": "n" }
  ]
}
```

The earlier `branch` box kind with named ports is removed (see issue
210). Comparator + `from_branch` is the only branching mechanism.

### Iterator boxes
A box may carry an `iterator_outputs` field — an ordered array of
output slot names. The dispatch layer routes the input value to the
slot named `iterator_outputs[counter]`, advances the counter (mod
length), and re-spawns itself to consume the next queued input. No
function is invoked — iterators are pure routing primitives. Issue
221 covers the model; 213 covers the queued-input semantics.

### Connections
Connections are written to BOTH endpoint box files. The runner
validates at load time that both ends agree. Disagreement is a hard
error.

```json
{ "from_box": "...", "from_branch": null|"lt"|"eq"|"gt"|"<iter-slot>",
  "to_box": "...",   "to_input": "..." }
```

`from_branch` is `null` for plain call boxes (single output wire).

## Language spec system (phase 3)

Each language is a **spec** — a `.so` library at
`langs/<name>/spec.so` that exports a `lang_spec_t` symbol with
`init` / `teardown` / `compile` / `invoke` callbacks. The pool runner
loads every `langs/*/spec.so` at startup via `dlopen`. The spec
contract (issue 303) is uniform across languages; no language is
privileged.

Reference specs ship with SoraMech:
- `langs/lua/spec.so` — in-process via `lua_State`, no compile step (issue 306)
- `langs/c/spec.so`   — in-process via `dlopen`, with a compile step that generates a wrapper (issue 307)
- `langs/bash/spec.so`— out-of-process via Unix domain socket to a persistent subprocess (issue 308)

A user adds a new language (Python, Rust, anything) by writing a new
spec under `langs/<name>/`. SoraMech itself does not transpile or
convert source between languages.

The phase 2 `drivers/*.sh` scripts retire when phase 3 lands. They
implement the same idea (one process per call, stdout as channel)
that the spec system replaces with persistent runtimes and
length-prefix-framed sockets / direct function pointers.

## Execution model — phase 2 (synchronous)

The runner walks the graph depth-first from the entry box. Each box
call:

1. Collect inputs: read wired output values from predecessor boxes.
2. Invoke driver: shell out to the appropriate driver script.
3. Collect output: decode single JSON value from driver stdout.
4. Store output: held in runner memory, keyed by box id.
5. Fire connections: enqueue boxes whose input dependencies are now
   met.

Each step is wrapped in a `task_fn` boundary so the runner can be
swapped for the phase 3 pool runner without changing the box
execution model. Phase 2 is single-threaded.

## Execution model — phase 3 (thread pool)

The C pool runner owns a 3d-rts task pool with N worker threads.
Each box invocation is a task. The dispatch layer (issue 304) is the
worker-side action: read inputs from slots → invoke the language
spec (or route, for comparators / iterators) → write output to a
slot → unref consumed inputs → fire downstream connections.

Slots are per-task ring buffers with reference-counted lifetime
(issue 302). Wires hold references; producers push, consumers peek
or pop. The wait list on each slot is the synchronization primitive
that parks blocked tasks until a producer fills the slot.

The pool runner has no embedded Lua. It is C from `main` down to the
language spec boundary; only inside a Lua spec's `invoke` does Lua
code run, and only inside that worker's `lua_State`. The graph
loader is also pure C (issue 305) — phase 2's
`src/003-loader.lua` retires.

See `docs/004-ipc-and-threading.md` for the full IPC and threading
discussion, including the blocking-semantics constraint on long-
running operations.

## Data files

Persistent data lives in `data/<name>.json`. Format:

```json
{
  "constant": false,
  "fields": {
    "history": [],
    "config": { "constant": true, "value": { "model": "soramind-default" } }
  }
}
```

Top-level `constant: true` makes the whole file read-only.
Individual fields can be flagged constant within an otherwise
mutable file. Accessed via `libs/soramech-data.lua`.

Ephemeral state (scratch tables, run logs, the `last-run.jsonl`
output) goes to `tmp/`, a symlink to `/tmp/<map-name>/`. Survives
the run but not a reboot — intentionally.
