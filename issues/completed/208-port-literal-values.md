# 208 — Port literal values

## Status

complete (work landed across 207, 210, 218)

## Current behavior

Input ports on a box can only receive values from wired connections. If no wire
is connected to an input port, the box never becomes "ready" and the runner
skips it silently. There is no way to supply a constant value to an input from
the editor.

Port names on both input and output ports are editable text fields in the
inspector. This is fragile — the names must match the actual function signature
exactly or the box silently misfires at runtime.

## Intended behavior

**Input port names are read-only.** They are derived from the function's
parameter names when the user selects a function via the file browser (issue
207). The inspector displays them as non-editable labels. Only the literal
value field is editable per input port.

**Literal values:** Each input port has an optional literal value editable
directly in the inspector panel. If the value field is non-empty and no wire is
connected to that port, the literal value is used at runtime. If a wire IS
connected, the wire's value overwrites the literal at runtime — wires always win.

The value field accepts JSON notation (numbers, strings with quotes, arrays,
objects) or bare strings for convenience. The executor tries JSON.decode first;
if that fails, the raw string is used.

**Output ports are gone.** Every call box has exactly one output wire carrying
the raw return value (see issue 210). The output section of the inspector is
replaced by the comparator toggle (see issue 210). `box.outputs` is removed
from the call box schema.

This lets a box be fully configured from the inspector without needing a
dedicated "data" box wired to it for every constant, and eliminates the class
of runtime failures caused by mistyped port names.

## Implementation notes

This issue was implemented incrementally as part of the related
work in 207 (file browser sets input names from function
signatures), 210 (comparator-based branching replaces named output
ports), and 218 (single-output driver contract). By the time those
issues finished, every piece of 208 was in place. The current code
is:

### Inspector — `assets/js/004-inspector.js`
`mk_port_display(ports)` renders each input port as a row with:
- A read-only `<span>` for the port name (derived from the function
  signature via the file browser).
- A `:type` label when the type is more specific than `any`.
- A `<input>` for the literal value, placeholder `value…`, persisted
  to `ports[i].value` on input.

Setting a literal value also severs any incoming wires to that port
(walks `box.connections`, removes matches from this box's connections
and from each upstream box's connections, persists via the API).
This keeps "literal vs wire" from having two competing sources of
truth at the editor level.

The output side renders only the comparator toggle (issue 210) — no
output port list. Call-box schema does not include `outputs` and the
inspector never iterates `box.outputs`.

### Executor — `src/004-executor.lua`
Before the main run loop, every input port's `value` (if present
and non-empty) is JSON-decoded and seeded into the wire-value
store at `<box_id>.<port_name>`. Wires fired during the run
overwrite the seed with the producer's output, so wires win at
runtime — exactly the spec in this issue.

### File browser — `assets/js/004-inspector.js::open_browser`
On function selection, sets `current_box.ref`, `current_box.fn`, and
`current_box.inputs` from the parsed signature. Does not set
`current_box.outputs`. Comment in the source explicitly notes the
single-output rule.

### Schema — `src/001-schema.lua`
`validate_box` checks `inputs` (must be a table when present) and
`comparand` (must be a string when present). It does not require,
mention, or reject `outputs`. Existing box JSONs that still carry
an `outputs` field from before the cleanup are tolerated as
harmless leftover data — the runner ignores them.

### Notes
- The "wire wins at runtime" semantics are achieved by the
  pre-seed-then-overwrite ordering in the executor. Editor-side, the
  redundant case of "literal AND wire on the same port" is avoided
  by severing wires on literal entry. Either route to a value works.
- Existing maps may still have `outputs` arrays in their box JSONs.
  Loading and editing them works fine; the leftover is invisible
  unless you `cat` the file. A future cleanup could strip them, but
  that's not required by this issue.

## Related documents

- assets/js/004-inspector.js — mk_port_display, show
- src/004-executor.lua — execute, inputs_satisfied
- docs/001-architecture.md — store model, box.inputs schema
- issues/210 — single output wire and comparator (replaces box.outputs)
- issues/207 — file browser sets input names from function signature
