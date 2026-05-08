# 204 — Visible interaction hints

## Status

open

## Current behavior

The editor has no visible instructions. A new user sees a blank canvas with
two toolbar buttons ("reload", "switch map") and no indication of what to do.
All interactions are undiscoverable:
- Double-click to create a box
- Drag an output port to draw a wire
- Click a wire then Delete to remove it
- Space+drag or middle-mouse to pan
- Scroll to zoom

## Intended behavior

The canvas shows a short hint overlay when the map is empty or when nothing
is selected. The hint disappears once the user has at least one box on the
canvas. It should not reappear after that.

Suggested hint text (monospace, centered, dim):
```
double-click  create box
drag port →   draw wire
space+drag    pan
scroll        zoom
```

Additionally, the status bar at the bottom-left should reflect the current
interaction state:
- Idle with boxes: "N boxes loaded  —  double-click to create"
- Wire drawing in progress: "drawing wire — release on an input port"
- Box selected: "selected: <box-id>"
- Wire selected: "wire selected — Delete to remove"

## Suggested implementation steps

1. In the render loop (`005-app.js`), if `Boxes.boxes` is empty, draw the
   hint text centered on the canvas using the canvas 2D context (not a DOM
   overlay, so it transforms with the camera only for pan/zoom-invariant
   display use `ctx.setTransform(1,0,0,1,0,0)` before drawing).
2. Update `status_msg` calls in mouse handlers to reflect interaction state.
3. No persistent storage needed — the hint is purely runtime state based on
   whether the boxes object is empty.

## Related documents

- assets/js/005-app.js — render loop, mouse handlers, status_msg
- assets/js/001-canvas.js — start_frame / end_frame (canvas transform)
