# 003-loader.lua — Public API

> **Phase 2 path, superseded.** The runtime that ships is the C
> pool runner, whose loader is `src/010-graph-loader.c`. This Lua
> loader still works and still serves the synchronous executor,
> but it is not what runs when you invoke `soramech-pool`, and it
> does not implement everything the C loader does (per-edge
> language classification, size-class enumeration, encapsulation
> splicing). Retiring it is an open item on the phase 3 progress
> page.

Loads a map directory into a graph structure and validates it.

## M.load_map(map_dir: string) -> graph, errors: string[]

Reads meta.json, all box files, and driver config. Returns a graph table:
  { meta, boxes={[id]=box}, entry=id, drivers={[ext]=path}, map_dir }
Returns nil + error list on parse/schema failures.

## M.validate_graph(graph: table) -> ok: bool, errors: string[]

Validates a loaded graph: connection reciprocity, driver existence, branch
"else" port presence, and cycle detection (cycles through branch boxes are
allowed as the retry pattern; others are hard errors).
