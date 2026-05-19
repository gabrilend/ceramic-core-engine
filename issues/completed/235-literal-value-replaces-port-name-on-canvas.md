# 235 — Literal value replaces port name on the canvas (when set)

## Status
complete

## Implementation notes

Two pure helpers in `assets/js/002-boxes.js`:

- `port_has_incoming_wire(box, port_name)` — scans `box.connections`
  for a record targeting this port. Cheap; the connections array is
  already in cache.
- `port_label_text(box, i)` — the three-way decision (port name when
  unwired and value-less, value when set without a wire, port name
  when a wire occupies the port even if a stale literal value sits
  on it). First-line collapse for multi-line values; ellipsis when
  the displayed string exceeds `MAX_LABEL_CHARS` (24).

`draw_box` calls `port_label_text(box, i)` in the input-port loop.
The redraw on inspector edit comes free — `Inspector.show` is
launched with `() => Canvas.mark_dirty()` as its `on_change_cb`, so
every value-input change already invalidates the canvas.

Tooltip is a single absolutely-positioned `#canvas-tooltip` div in
`assets/index.html`, kept hidden by default. `005-app.js` exposes
`show_tooltip` / `hide_tooltip` and wires them into the mousemove
handler: hide on every tick, then re-show when the cursor sits on
an input port whose `port_full_value_if_truncated` returns a string
(i.e. the canvas label is a shortened form of the raw value).
`mouseleave` on the canvas hides it too, so a stale tooltip never
strands when the cursor leaves the canvas area.

The "wire wins over literal value" rule deserves a note: the
inspector also severs wires when a literal is typed in
(`004-inspector.js`, line ~656), so in practice only one of the two
can be set at a time. The defense in `port_has_incoming_wire`
covers the case where stale on-disk data has both — older maps
showed exactly this pattern (e.g. `branch-test/write-high`).

Tested with the 8-case node script (no value, wired-over-value,
multi-line, long single-line, empty string, numeric, escape-encoded
separator, etc.). User-verified visually against the running editor.

## Current behavior

Each input port renders as a dot on the left edge of a box, with
the port's name (e.g. `text`, `sep`, `prompt`) drawn next to it on
the canvas. The literal value of that port — when one has been
typed into the inspector (issue 208 / 224) — does not appear on the
canvas at all. To see what value is bound, the user opens the
inspector and reads the value field.

For a box like `concat(sep, text_0, text_1)` where `sep` is a
literal `"\n\n"`, the canvas shows the parameter name `sep` but
not the actual separator. A user skimming the map sees the
structure but not the content.

## Intended behavior

If a port has a literal value set in the inspector, **the canvas
label for that port shows the value instead of the port's name**.
The inspector still shows the original port name in its name field
(unchanged) and the value in its value field (unchanged) — only the
canvas rendering changes.

Examples:

| port name | literal value     | canvas label |
|-----------|-------------------|--------------|
| `sep`     | `"\n\n"`          | `"\n\n"`     |
| `prompt`  | (none)            | `prompt`     |
| `path`    | `"./input.txt"`   | `"./input.txt"` |
| `count`   | `5`               | `5`          |

The value is the same string the user typed; quotes are part of
how the user wrote it. Long values get truncated with an ellipsis
at a reasonable width (~24 chars?) and the full value shows in a
tooltip on hover.

### Why

This is the same idea as a spreadsheet cell showing the rendered
value while the formula bar shows the formula. A user wants to see
*what the program does* — the configured value — without having to
inspect each box one at a time. The port name is meta-information
(useful when wiring, useful in the inspector); the value is the
information (useful when reading the map).

### Wires override values

If a wire is connected to a port, the wire is the source of truth
and any literal value is ignored at runtime. The canvas label in
that case shows the **port name**, not the value — the value isn't
what's flowing through, and showing it would mislead. Three states:

- No wire, no value → show port name.
- No wire, literal value set → show the value.
- Wire connected → show the port name (value, if any, is dormant).

### Edge cases

- **Multi-line value** (e.g. a prompt with `\n` in it): render the
  first line only on the canvas, ellipsis if there's more, full
  value in the tooltip.
- **Empty string value** (`""`): treat as "no value" — show the
  port name. Otherwise the user types accidentally and the port
  silently disappears from view.
- **Variadic slots** (`text_0`, `text_1`, …): the same rule
  applies per slot. A variadic group with each slot's value shown
  is one of the strongest cases for this feature.

## Suggested implementation steps

1. `assets/js/002-boxes.js::draw_box` — the loop that draws
   input port labels. Replace the unconditional `port.name` with:
   ```
   const wire_attached = wire_targets_port(box, port);
   const has_value = port.value != null && port.value !== "";
   const label = (!wire_attached && has_value) ? truncate(port.value) : port.name;
   ```
2. Hover / tooltip: if the canvas label is a truncated value,
   register a small hit-region so the existing tooltip mechanism
   (or a fresh one) can show the full value on mouseover.
3. Re-render on inspector value edit: the existing autosave path
   already invalidates the canvas; verify it still triggers a
   redraw after this change.

## Relevant files

- `assets/js/002-boxes.js` — port label rendering
- `assets/js/004-inspector.js` — value field that drives the data
- `assets/js/006-wires.js` — `wire_targets_port` helper (or write
  one if not present)
- `issues/completed/208-port-literal-values.md` — established
  literal values; this issue surfaces them visually
- `issues/completed/224-editable-port-names-and-canvas-value-edit.md`
  — explicitly deferred canvas-value-edit; this is a related but
  smaller display-only change, not the editing UI from 224
