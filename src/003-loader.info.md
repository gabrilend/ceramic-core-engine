# 003-loader.lua — Public API

Loads a map directory into a graph structure and validates it.

## M.load_map(map_dir: string) -> graph, errors: string[]

Reads meta.json, all box files, and driver config. Returns a graph table:
  { meta, boxes={[id]=box}, entry=id, drivers={[ext]=path}, map_dir }
Returns nil + error list on parse/schema failures.

## M.validate_graph(graph: table) -> ok: bool, errors: string[]

Validates a loaded graph: connection reciprocity, driver existence, branch
"else" port presence, and cycle detection (cycles through branch boxes are
allowed as the retry pattern; others are hard errors).
