# 224 — Editable port names and canvas-side value editing

## Status
complete (scope adjusted: name-edit moved, value-edit stays)

## Scope adjustment

The value field stays in the inspector. Only the port-name editing
moved to the canvas overlay — the rest of issue 208's read-only-name
decision stays reversed (names are now editable), but the overlay
implementation only carries the name input. Values on the canvas
were the more disruptive half of the change and the user opted out;
keeping values in the inspector also keeps the variadic auto-grow
flow exactly where it already lives.

## Implementation notes

New module `assets/js/009-box-overlays.js` floats an editable
`<input>` per input port next to its dot. Lifecycle:
- `Overlays.redraw()` runs in the main render loop after
  `Boxes.draw_all`, gated by `Canvas.start_frame()` so the cost is
  tied to actual canvas changes (camera, drag, create/delete).
- Reconciles overlays with `Boxes.boxes` — creates new ones, tears
  down orphans.
- Per-port rows rebuild when port count changes (variadic
  add/remove); otherwise the `<input>` elements persist so user
  typing isn't clobbered between frames.
- Updates skip any input that currently has focus.
- Each input commits on blur or Enter (Escape reverts). Typing
  doesn't fire the rename — only commit does — so the wire-rewrite
  cost is paid once per change.

Inspector exposes `rename_port(box, port_idx, new_name)`:
- Validation: non-empty, identifier-shaped (`^[A-Za-z_]\w*$`),
  unique within the box.
- On rename, rewrites `to_input` on every wire targeting the port
  (both endpoints, mutating in place per the stale-cache lesson
  from 217).
- Re-renders the inspector if the renamed box is the one on
  display, so the read-only name label in the sidebar tracks the
  new name without the user having to re-click.

`002-boxes.js::draw_box` no longer renders input port name text —
the overlay does. Output port labels (comparator branches, iterator
slot names) still draw on the canvas; those aren't covered by this
issue.

CSS additions in `assets/index.html`: `.box-overlay-row`,
`.box-overlay-name` — overlays don't scale with zoom (consistent
with most node editors so labels stay legible at any zoom).

### Edge cases

- **Variadic slot rename**: rename_port accepts `base_N`-shaped
  names, so the user can technically rename a non-variadic port to
  match a variadic-slot pattern. The variadic helpers detect the
  shape on subsequent operations; if the user creates one by
  accident the recovery is to rename it back.
- **Renamed port had a wire**: the wire's `to_input` rewrites on
  both endpoints; the canvas re-renders with the new label without
  losing the connection.
- **Typo'd a name to something the function doesn't expect**: this
  is the intentional tradeoff from issue 208. The runtime error
  surfaces clearly rather than silently skipping.

## Current behavior

Input port names are read-only labels in the inspector — derived from
the function's parameter names when the file browser populates the
box (issue 207, 208). Each port row in the inspector also carries a
literal value input. With variadic inputs (issue 217), a box with
several slots produces a tall, narrow list of name+type+value rows
that crowds the sidebar.

The canvas already renders port names next to their dots, so the
name appears twice — once on the canvas (read-only label) and once
in the inspector (read-only label).

## Intended behavior

Move both the port name and the literal value input out of the
inspector and onto the canvas next to the port dot. The inspector
keeps only the variadic toggle / remove-slot buttons and any
non-port box-level fields (ref / fn / comparator).

### Why

- The inspector is crowded with redundant data (the canvas already
  shows port names).
- Literal values often need more horizontal room than a 72px input
  field allows, especially for prompts, paths, or JSON snippets.
- Reading the box's full configuration without clicking through the
  inspector becomes possible — every input's name and value is
  visible directly on the canvas.
- 208's "read-only port names" decision was about preventing typo
  drift between source code and box JSON; the file browser still
  populates names from the parsed signature, but the user should be
  free to override them after the fact.

### Reverses 208's read-only-name decision

Issue 208 made input port names non-editable on the principle that
they should match the parsed function signature exactly. This issue
flips that — names become editable again, with the trade-off that a
typo can desynchronize the editor's record from the function the box
calls. The cost is a per-box runtime error rather than a hidden
silent skip, so it surfaces clearly. The benefit is that users can
rename inputs freely (renaming a Lua function's parameter and then
updating the box does not require re-browsing).

### Layout

Each input port row on the canvas becomes:

```
●  text_0    ┌──────────────────┐
             │ "hello world"    │
             └──────────────────┘
```

- The port dot stays on the box's left edge (unchanged).
- The port name is an editable text field, rendered as an HTML
  overlay positioned next to the dot. Pan and zoom adjust the
  overlay's transform to match the canvas.
- The literal value field is another HTML overlay below or to the
  right of the name.

### HTML overlays over the canvas

Canvas 2D doesn't do native text inputs, so the editable fields are
DOM `<input>` elements absolutely positioned over the canvas. A
small per-box overlay container holds them, transforms with the
camera (CSS transform applied on every render frame), and updates
the underlying `box.inputs[i]` on input.

This is the same pattern as the source viewer (issue 215) — DOM
elements layered over the canvas, with positions computed from
`Canvas.world_to_screen`. The complexity is keeping overlay state
in sync with the canvas: they need to be created when a box is
loaded, destroyed when a box is deleted, repositioned every frame
(or when pan/zoom changes), and hidden when off-screen.

### Inspector becomes lighter

After this change the inspector shows:
- Box label (editable)
- ref / fn (with browse / view buttons)
- For each input: the variadic toggle (`var` / `var ×` / `×`) only.
  No name field, no value field — those live on the canvas.
- Output: the comparator toggle and comparand input (unchanged).
- Delete-box button (unchanged).

Roughly half the height the current inspector takes for variadic
boxes.

## Open questions

- **Overlay performance**: dozens of overlays multiplied by frequent
  pan/zoom updates is a real cost. Tests are needed; a bounding-box
  cull (only render overlays for visible boxes) is the obvious
  optimization if it matters.
- **Value-input width**: the field grows with the input's content, or
  is a fixed width with horizontal scroll? Probably grow-on-content
  with a max width clamp, similar to what monospace text does.
- **Editing port names while wires are connected**: renaming a port
  while a wire is attached should rename the wire's `to_input` /
  `from_branch` on both endpoints atomically — same bilateral-update
  pattern the variadic ops use.
- **Name validation**: what's a valid port name? Probably `[A-Za-z_][\w]*`
  (a Lua / C / shell-compatible identifier). Reject names that start
  with digits, contain spaces, etc. Reject duplicates within a box.
- **Backward compat**: existing maps with the current layout work as
  before — no migration needed. The change is purely UI.

## Suggested implementation sequence

1. Create `assets/js/009-box-overlays.js` (new module) responsible
   for per-box DOM overlays. Public API: `update_overlays_for(box_id)`
   to create / refresh overlays for a box, `remove_overlays_for(box_id)`
   on box deletion, `redraw_overlays()` on pan/zoom.
2. Wire the overlays into the render loop: after `Boxes.draw_all`,
   call `redraw_overlays()` to reposition the overlays for the
   current camera. The overlays themselves don't redraw — only their
   `style.left` / `style.top` / `style.transform` updates.
3. Remove the name + value rendering from `mk_port_display` in
   `004-inspector.js`. Leave only the variadic controls.
4. Remove the canvas-side port name labels from `002-boxes.js::draw_box`
   (the overlay's editable input replaces it).
5. Wire validation: name must be a valid identifier and unique
   within the box's `inputs`. Validate in the input's `oninput`
   handler; revert to the previous valid name on rejection.
6. Test against `maps/hello`, `maps/classify-demo`, and a fresh map
   with variadic inputs (e.g. via `libs/text.lua`'s concat).

## Relevant files

- `assets/js/009-box-overlays.js` — new module (to be created)
- `assets/js/002-boxes.js` — port name labels move out
- `assets/js/004-inspector.js` — `mk_port_display` simplified
- `assets/js/001-canvas.js` — pan/zoom triggers overlay reposition
- `assets/js/005-app.js` — render loop calls overlay redraw
- `assets/js/006-wires.js` — wire-connect/rename touches names
- `issues/completed/208-port-literal-values.md` — the read-only-names
  decision that this issue flips
- `issues/217-concat-box-and-dynamic-inputs.md` — variadic UI that
  drove the inspector-crowding observation
