# 204 — Visible interaction hints

## Status

complete

## Current behavior

The editor has no visible instructions. A new user sees a blank canvas with
toolbar buttons and no indication of what to do. All interactions are
undiscoverable:
- Right-click to bring up the canvas context menu (add box, etc.)
- Drag an output port to draw a wire
- Middle-click or space+drag to pan
- Scroll to zoom

## Intended behavior

The canvas shows a short hint overlay when no boxes exist. The hint
disappears once a box is created. The status bar at the bottom-left
reflects the current interaction state.

## Implementation notes

`draw_empty_hint(ctx)` in `assets/js/005-app.js`:
- Called from the render loop when `Object.keys(Boxes.boxes).length === 0`.
- Resets the canvas transform (`setTransform(1,0,0,1,0,0)`) so the text is
  drawn in pure screen pixels — pan and zoom don't move or resize it.
- Two-column layout, centered on the canvas, monospace, dim. Key in the
  left column (slightly brighter), description in the right column (dimmer).

Hint contents:

```
right-click       add box · context menu
drag port →       wire boxes together
middle / space    pan canvas
scroll            zoom
```

Status-bar updates added in `005-app.js`:
- On clicking an output port to begin a wire: `drawing wire — release on
  an input port`
- On clicking a box: `selected: <box-id>`
- On clicking empty canvas (deselect): cleared

Wire-selected state isn't a separate UI mode in the current editor —
wires are erased via the toolbar `✕ wire` mode or the right-click context
menu. The original spec mentioned a "wire selected — Delete to remove"
state that doesn't exist; left out.

The `load_map` status message stays as `loaded N boxes` since it's
informational rather than interaction-state.

## Related documents

- assets/js/005-app.js — render loop, draw_empty_hint, mousedown handler
- assets/js/001-canvas.js — viewport (start_frame/end_frame)
