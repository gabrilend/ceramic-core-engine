# 210 — Comparator wire branching

## Status

complete

## Supersedes

The original design of this issue described an `llm-route` box kind that used an
Ollama call to route between named output ports. That design has been removed:
the `llm-route` box kind, the executor's `llm_route_call` helper, and all
related inspector and wire rendering code should be reverted. The problem it was
solving — conditional routing — is handled instead by the comparator mechanism
described here, which works on any box.

Similarly, the `branch` box kind (issue 108) is removed in favour of this mechanism.

## Current behavior

Every box has named output ports. Routing is done either by:
- `branch` boxes with named ports and hard-coded predicates, or
- `llm-route` boxes with named ports and an Ollama routing call.

Both approaches require a dedicated box kind. The connection schema uses
`from_port` for these boxes and `from_output` for call boxes — two parallel
fields, only one ever set.

## Intended behavior

Every box has exactly **one output wire** carrying whatever the function
returned (raw, as a single value). There are no named output ports.

A box may optionally have a **comparator** configured. When a comparator is
active, the single output wire splits into three wires: `lt`, `eq`, and `gt`.
The executor compares the output value against a literal number. If the output
is not a strict number, the executor halts with an error — there is no fallback.

The comparator is configured per box via a `comparand` string field (the
literal number to compare against) stored in the box file. The inspector shows:
- A toggle button labelled **compare** next to the output section.
- When active, a text input for the comparand value.

Connection schema changes:
- Old: `{from_box, from_output|from_port, to_box, to_input}`
- New: `{from_box, from_branch, to_box, to_input}`
  where `from_branch` is `null` (no comparator, one wire) or `"lt"`/`"eq"`/`"gt"`.

The canvas renders one output dot per box by default. When a comparator is
active, it renders three stacked dots labelled lt / eq / gt.

## Implementation notes

The whole pipeline is in place across schema, loader, executor,
canvas rendering, wires, inspector, and file browser:

### Box schema — `src/001-schema.lua`
`comparand` is an optional string field validated by `validate_box`.
The `outputs` array is not part of the schema (no validation, not
required, ignored if present in legacy box JSONs). Branch box kind
and predicate-port fields are gone.

### Connection schema — `src/001-schema.lua` + `src/003-loader.lua`
Connection records use `from_branch`, validated by `validate_box`
against `valid_branches = { lt, eq, gt }` plus `nil` (single-wire
mode). The loader loads connections without any `from_port` /
`from_output` handling.

### Executor — `src/004-executor.lua`
After a box runs, `fire_connections` checks `box.comparand`:
- If set: parse as number (halt with error if it isn't), compare
  against the output (also must be number-coercible), classify as
  `lt` / `eq` / `gt`, fire only the connection whose `from_branch`
  matches.
- If unset: fire every connection whose `from_branch` is nil.

There's no remaining `branch` kind handling or `llm_route_call`
helper.

### Canvas rendering — `assets/js/002-boxes.js`
`port_positions` returns three output dots (`lt` / `eq` / `gt`)
when `box.comparand` is set, otherwise a single centered dot.
`draw_box` uses `BRANCH_COLOR = { lt, eq, gt }` to color each
comparator dot and labels them next to the dots.

### Wire drawing — `assets/js/006-wires.js`
Connections use `c.from_branch` exclusively. Plain wires use a dim
default color; comparator branches get distinct colors per branch
via `BRANCH_COLOR`. `create_connection` takes a `from_branch`
parameter (null for plain).

### Inspector — `assets/js/004-inspector.js`
- Read-only port names rendered via `mk_port_display`.
- Compare toggle button and comparand input field in the output
  section. No output port list, no branch/llm-route kinds.

### File browser — `assets/js/007-filebrowser.js` (called from
`004-inspector.js::open_browser`)
On function select: sets `current_box.ref`, `current_box.fn`, and
`current_box.inputs` from the parsed signature. Does not set
`current_box.outputs`.

## Related documents

- assets/js/002-boxes.js — canvas dot rendering
- assets/js/004-inspector.js — compare toggle, comparand field
- assets/js/006-wires.js — connection drawing, from_branch field
- src/001-schema.lua — box and connection validation
- src/003-loader.lua — connection loading
- src/004-executor.lua — fire_connections, comparator evaluation
- docs/007-architecture.md — box and connection file format
