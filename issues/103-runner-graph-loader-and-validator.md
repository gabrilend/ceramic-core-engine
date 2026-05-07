# 103 — Runner: graph loader and validator

## Status

open

## Blockers

- 101 (map format must be defined before loading can be implemented)
- 102 (driver list loaded here; drivers must exist to validate references)

## Current behavior

No runner exists. There is no mechanism to load a map directory into
memory as a graph structure or to validate its integrity before execution.

## Intended behavior

soramech-runner.lua, when given a map directory, performs a load and
validate phase before any execution begins:

  1. Reads meta.json — confirms entry_box_id is present
  2. Reads all files in boxes/ — parses each as a box table
  3. Reads drivers.json (map-local, then global fallback)
  4. Validates:
     a. Every connection's to_box references an existing box id
     b. Every connection is reciprocally written in the target box's file
        (both ends agree) — disagreement is a hard error with a message
        naming both files and the conflicting field
     c. Every ref's file extension has a driver defined
     d. Branch boxes: every non-"else" port has a predicate defined;
        "else" port exists; if the map has any LLM-type boxes connected
        to the branch, "else" is permitted to be unwired (retry semantics);
        otherwise "else" must be wired or the runner refuses to start
     e. No cycles that don't pass through a branch box (direct loops are
        an error; loops through branches are allowed — they are the retry
        pattern)
  5. Builds an in-memory graph: table of box tables, adjacency lists
     keyed by box id, driver table keyed by extension

The runner prints a structured error report and exits non-zero if any
validation fails. No fallbacks.

## Suggested implementation steps

1. Write src/003-loader.lua — public functions:
   - load_map(map_dir) -> graph, err
   - validate_graph(graph) -> ok, errors[]
   Each error in errors[] is a table: {box_id, field, message}.
2. The graph structure: { boxes={[id]=box}, entry=id, drivers={[ext]=path} }
   Adjacency is derived from boxes' connections lists; no separate edge table.
3. Cycle detection: DFS from entry box, mark visited. A cycle that reaches
   a branch box is allowed (log a warning, not an error). A cycle that
   doesn't pass through a branch box is an error.
4. Write tests/001-loader-test.lua — loads maps/hello/ and maps/driver-test/,
   asserts no errors. Loads a deliberately broken map (missing connection
   endpoint) and asserts the correct error is reported.

## Related documents

- docs/001-architecture.md — graph structure and validation rules
- issues/101 — map format and example map
- issues/104 — executor calls into the graph loaded here

## Notes

The reciprocal connection check (both box files must agree) is the most
important invariant to enforce. If this check is ever skipped or weakened,
the server's partial writes (e.g., browser tab closed mid-edit) will
produce silently corrupt maps. Always hard-error here.

Cycle detection through branch boxes: the retry pattern (else wired back
to an upstream box) forms a cycle. This is valid. The detector must not
reject it. Marking branch boxes as "cycle-break" nodes in the DFS is
sufficient.
