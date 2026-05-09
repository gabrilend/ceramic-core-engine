# 210 — Comparator wire branching

## Status

open

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

## Suggested implementation steps

1. **Box schema** — add optional `comparand` string to the box file format.
   Remove `outputs` array from call box schema. Remove `ports` array and
   predicate fields.

2. **Connection schema** — replace `from_output`/`from_port` with `from_branch`
   (`null` or `"lt"`/`"eq"`/`"gt"`). Update `src/001-schema.lua` validation
   and `src/003-loader.lua` connection loading.

3. **Executor (`src/004-executor.lua`)**:
   - Remove `branch` and `llm-route` kind handling from the execute loop.
   - Remove `fire_connections` named-port logic for both kinds.
   - After a call box runs: if `box.comparand` is set, parse it as number,
     compare against the output value (`lt`/`eq`/`gt`), and fire only the
     connection whose `from_branch` matches. If value is not a number, halt
     with error. If no comparator, fire the one connection whose `from_branch`
     is null.

4. **Canvas rendering (`assets/js/002-boxes.js`)** — one output dot per box;
   if `box.comparand` is set, render three stacked dots with labels.

5. **Wire drawing (`assets/js/006-wires.js`)**:
   - Remove `from_port`/`from_output` dual-field logic.
   - Use `from_branch` to identify which dot a wire originates from.
   - Color lt/eq/gt wires distinctly if desired.

6. **Inspector (`assets/js/004-inspector.js`)**:
   - Remove `branch` and `llm-route` kind options from dropdown.
   - Remove output port list for call boxes.
   - Add compare toggle button and comparand input field.
   - Input port names are read-only (derived from parsing); only literal values
     are editable (see issue 208).

7. **File browser (`assets/js/007-filebrowser.js`)** — on function select, set
   `box.inputs` from parsed argument names; do NOT set `box.outputs` (no named
   outputs).

## Related documents

- assets/js/002-boxes.js — canvas dot rendering
- assets/js/004-inspector.js — compare toggle, comparand field
- assets/js/006-wires.js — connection drawing, from_branch field
- src/001-schema.lua — box and connection validation
- src/003-loader.lua — connection loading
- src/004-executor.lua — fire_connections, comparator evaluation
- docs/001-architecture.md — box and connection file format
