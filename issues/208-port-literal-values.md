# 208 — Port literal values

## Status

open

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

## Suggested implementation steps

1. **Data model** — `box.inputs` port objects gain an optional `value` string
   field. `box.outputs` is removed from call box schema (not needed; see
   issue 210 for the single-wire model).

2. **Inspector (`004-inspector.js`)**:
   - `mk_port_display()` renders input port names as non-editable labels
     (remove the name input). Each row shows the type label and a value input.
   - On value change, update `ports[i].value` and call `save()`.
   - Remove the output port section for call boxes entirely.

3. **Executor (`src/004-executor.lua`)** — add a pre-seeding pass before the
   main execution loop: for every box, for every input port with a non-empty
   `value`, write `store[box_id.."."..port.name]` with the parsed value.
   Existing `inputs_satisfied` and `collect_inputs` logic requires no changes
   because they only read from the store.

## Related documents

- assets/js/004-inspector.js — mk_port_display, show
- src/004-executor.lua — execute, inputs_satisfied
- docs/001-architecture.md — store model, box.inputs schema
- issues/210 — single output wire and comparator (replaces box.outputs)
- issues/207 — file browser sets input names from function signature
