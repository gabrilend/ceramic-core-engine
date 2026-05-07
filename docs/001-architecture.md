# SoraMech — Architecture

## Three programs, one data format

SoraMech is three independent programs that share a common on-disk format:
the map directory. None of the three knows about the others at runtime.

```
  Browser (index.html)
       |  HTTP (file CRUD)
       v
  soramech-server.lua          [map directory]
                               maps/<name>/
                                 boxes/
                                 data/
                                 drivers.json
                                 meta.json
                                 tmp/ -> /tmp/<name>/
       ^
       | reads & writes
  soramech-runner.lua
```

### soramech-server.lua

Thin HTTP proxy. Receives JSON requests from the browser and translates
them into file reads and writes on the map directory. No logic beyond
path validation and basic consistency checks (e.g., reject a delete if
the box is still referenced by another box's connection list).

Started with: `luajit soramech-server.lua <maps-root> [port]`
Default port: 7700

Endpoints (all operate on map files):
  GET  /maps                        list maps
  GET  /maps/<name>/boxes           list box IDs
  GET  /maps/<name>/boxes/<id>      read box file
  PUT  /maps/<name>/boxes/<id>      write box file
  DELETE /maps/<name>/boxes/<id>    remove box (validates no dangling refs)
  GET  /maps/<name>/data/<name>     read data file
  PUT  /maps/<name>/data/<name>     write data file
  GET  /maps/<name>/drivers         read drivers.json
  PUT  /maps/<name>/drivers         write drivers.json
  GET  /maps/<name>/meta            read meta.json

### soramech-runner.lua

FSM execution engine. Loads a map directory, validates the graph, then
executes boxes in dependency order. Each box invocation is wrapped in a
`task_fn(inputs) -> outputs` call — matching the 3d-rts thread pool's
action chain signature for future threading compatibility.

Started with: `luajit soramech-runner.lua <map-dir>`

Execution steps:
  1. Read all box files from boxes/
  2. Validate: all connection endpoints exist, branch "else" handled
  3. Determine entry box from meta.json
  4. Execute: resolve inputs, invoke driver, collect outputs, fire wires
  5. On completion, write last-run snapshot to tmp/last-run.json

### index.html

Static HTML + vanilla JS. Infinite-scroll canvas. Box diagram editor.
Talks to soramech-server.lua for all file operations. No run button —
the user starts the runner separately from the terminal.

## Map directory format

```
maps/<name>/
  meta.json          — { name, description, entry_box_id }
  drivers.json       — { ".lua": "drivers/lua.sh", ".c": "drivers/c.sh", ... }
  boxes/
    <id>.json        — one file per box (see Box file format below)
  data/
    <name>.json      — lua table as JSON; sections have "constant": bool
  drivers/
    lua.sh           — built-in lua driver (shipped with soramech)
    c.sh             — built-in C driver
    bash.sh          — built-in bash driver
  tmp/               — symlink to /tmp/<name>/
    last-run.json    — snapshot of last run (ephemeral)
    logs/            — run logs
    cache/           — compiled binaries (C driver cache)
```

## Box file format

```json
{
  "id": "unique-string",
  "label": "Human name",
  "kind": "call",
  "ref": "src/strings.lua",
  "fn": "trim",
  "inputs": [
    { "name": "text", "type": "string" }
  ],
  "outputs": [
    { "name": "result", "type": "string" }
  ],
  "connections": [
    {
      "from_output": "result",
      "to_box": "next-box-id",
      "to_input": "text"
    }
  ],
  "ui": { "x": 120, "y": 340 }
}
```

For branch boxes (`"kind": "branch"`):
```json
{
  "id": "classify",
  "kind": "branch",
  "inputs": [{ "name": "value", "type": "string" }],
  "ports": [
    { "name": "rogue", "predicate": { "op": "eq", "value": "rogue" } },
    { "name": "wizard", "predicate": { "op": "eq", "value": "wizard" } },
    { "name": "else" }
  ],
  "connections": [...]
}
```

Connections are written to BOTH endpoint box files. The runner validates
at load time that both ends agree. Disagreement is a hard error.

## Language driver interface

Drivers are shell scripts. The engine invokes them as:

  <driver-script> <file-path> <fn-name> <arg-count> [<arg1> <arg2> ...]

Outputs are returned as JSON on stdout. Exit code non-zero = box failure.
See docs/003-driver-system.md for the full driver contract.

## Execution model (v1 — synchronous)

The runner walks the graph depth-first from the entry box. Each box call:

  1. Collect inputs: read wired output values from predecessor boxes
  2. Invoke driver: shell out to the appropriate driver script
  3. Collect outputs: parse JSON from driver stdout
  4. Store outputs: held in runner memory, keyed by box id + output name
  5. Fire connections: enqueue boxes whose input dependencies are now met

Each step is wrapped in a `task_fn` boundary so the runner can be
threaded later by substituting the synchronous executor with one backed
by effil-jit or the custom thread pool from the 3d-rts project.

## Data files

Persistent data lives in data/<name>.json. Format:

```json
{
  "constant": false,
  "fields": {
    "history": [],
    "config": { "constant": true, "value": { "model": "soramind-default" } }
  }
}
```

Top-level `constant: true` makes the whole file read-only. Individual
fields can be flagged constant within an otherwise mutable file.

Ephemeral state (scratch tables, logs, last-run snapshot) goes to tmp/,
which is a symlink to /tmp/<map-name>/. It survives the run but not a
reboot — intentionally.
