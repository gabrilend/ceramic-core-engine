# 234 — Middle-click duplicates the selected box (with wires)

## Status
open

## Current behavior

A box on the canvas can be selected (single click) and edited via
the inspector, but there is no shortcut for "make me another one
just like this." A user who wants to repeat a configured box has
to:

1. Right-click → "add box here" to spawn a fresh empty box.
2. Reopen the file browser, re-pick the same ref / fn.
3. Re-type all the literal input values.
4. Re-wire every upstream connection by hand.

For a box with several literal values and several incoming wires
this is many clicks of pure repetition, and any one of them is a
chance to typo a value or miss a wire.

Middle-mouse currently has no editor behavior other than the
browser's default scroll-anchor (which is itself disruptive on a
canvas app).

## Intended behavior

**When a box is selected and the user does a short middle-mouse
click (not a drag) anywhere on the canvas**, the editor creates a
duplicate of the selected box at the cursor position. The duplicate
carries over:

- `ref`, `fn`, `kind`, `outputs`, `variadic_inputs` — same function
  bound to the new box.
- Every input port, including its literal value (if any).
- Every wire whose destination is the original box: a matching wire
  is created from the same source box / output to the new box's
  same-named input. Outgoing wires (where the original is the
  source) are NOT duplicated — multiple boxes fanning out from one
  source is the normal case; multiple boxes converging on the same
  downstream port is not.

The duplicate gets a fresh `id` and a `ui.x / ui.y` at the cursor
location. The label is the original's label suffixed `(copy)` (or
`(copy 2)`, `(copy 3)` if the suffix is already present) — same
naming pattern most desktop UIs use.

A middle-drag (the cursor moved more than a few pixels between
press and release) is not a duplicate. Reserve that gesture for
future pan / other use; for now it is a no-op rather than an
accidental copy.

If no box is selected, middle-click is a no-op (with maybe a
toast / status hint, "select a box to duplicate it").

### Why middle-click

Right-click already opens the context menu (issue 212). Left-click
selects / deselects. Middle-click is the only unused primary
button, and the "click on empty canvas to place" muscle memory from
add-box mode transfers cleanly: select source → middle-click target.

### Why duplicate incoming wires by default

The whole reason to duplicate is "I want another node fed by the
same data." Forcing the user to re-wire defeats the gesture.
Outgoing wires are skipped because the user usually wants the copy
to feed something *new* downstream — that's the half they're about
to wire up.

### Open question — duplicate-with-no-wires variant?

Hold a modifier (`Shift+middle-click`?) to duplicate without
wires? Defer until someone asks; the wireless version is just "add
a fresh box and browse to the same fn," which the right-click menu
already covers.

## Suggested implementation steps

1. `assets/js/005-app.js` — middle-mouse handlers:
   - `mousedown` (button 1): record press position + selected box id.
   - `mouseup` (button 1): if movement ≤ threshold and a box is
     selected, call `duplicate_box(selected_id, cursor_world_pos)`.
   - `preventDefault` on the mousedown to suppress browser
     scroll-anchor behavior.
2. New module function `Boxes.duplicate(src_box, x, y)`:
   - Clone the box JSON, assign a new id, set `ui.x/ui.y`.
   - Suffix the label with `(copy)` / `(copy N)`.
   - POST to the server (existing create-box endpoint).
3. Wire duplication: iterate every connection in the map whose
   `to_box == src_box.id`. For each, POST a matching connection
   with `to_box = new_box.id` (same `to_input`, same
   `from_box` / `from_output`).
4. Update local Boxes cache and re-render. Select the new box so
   the user can immediately edit / nudge it.

## Relevant files

- `assets/js/005-app.js` — pointer event handlers, current
  right-click context menu lives here.
- `assets/js/002-boxes.js` — local box cache + create flow.
- `assets/js/006-wires.js` — `create_connection`, wire data model.
- `src/005-http-server.lua` — existing POST endpoints for boxes
  and connections.
- `issues/completed/212-editor-interaction-modes.md` — established
  the right-click and tool-mode patterns this issue extends.
