# 106 — Web editor: canvas and box rendering

## Status

completed

## Blockers

- 105 (server must be running for the editor to load map files)

## Current behavior

No web editor exists. Map files can only be viewed and edited as raw JSON.

## Intended behavior

index.html is a single-file web application (no build step, no framework,
no dependencies beyond the fetch API and the Canvas 2D API). When opened
in a browser and pointed at a running soramech-server, it:

  - Renders an infinite-scroll canvas with pan (drag) and zoom (scroll)
  - Loads all boxes for the current map and draws each as a rectangle
  - Each box displays: label, kind badge, input port list on the left,
    output port list on the right
  - Clicking a box opens an inspector panel on the right side showing all
    editable fields (label, ref, fn, inputs, outputs)
  - Editing any field in the inspector sends a PUT to the server immediately
    (autosave — no explicit save button)
  - Double-clicking the canvas background creates a new box at that position
  - Boxes are draggable; dragging updates the ui.x/ui.y fields in the box
    file via PUT

The canvas coordinate system uses a camera transform (pan offset + zoom
scale) applied before drawing. All box positions are in "world" coordinates
stored in the box file; the camera transform maps world to screen.

## Suggested implementation steps

1. Write assets/index.html with a <canvas> element full-viewport and a
   sidebar div for the inspector panel.
2. Write assets/js/001-canvas.js — camera state (pan x/y, zoom), input
   handlers for drag-to-pan and scroll-to-zoom, a render loop using
   requestAnimationFrame, coordinate conversion functions world_to_screen
   and screen_to_world.
3. Write assets/js/002-boxes.js — box rendering (rectangle, label,
   kind badge, port dots on left/right edges), hit testing for click and
   drag.
4. Write assets/js/003-api.js — thin wrapper around fetch for each server
   endpoint. Returns parsed JSON or throws with the server error message.
5. Write assets/js/004-inspector.js — sidebar panel that populates a form
   from the selected box's fields and fires PUT on each input change.
6. Write assets/js/005-app.js — top-level: on load, ask the user for the
   server URL and map name, load all boxes, enter the render loop.
7. All JS files are loaded with <script> tags in index.html in numbered
   order. No module bundler.

## UI design notes

Port dots: input ports are drawn on the left edge of the box, one dot per
input, evenly spaced. Output ports on the right edge. Port dot size should
be large enough to click (at minimum 8px radius in screen space). Port
labels appear on hover.

Kind badge: a small colored label in the top-right corner of the box.
  "call" → blue, "branch" → orange, "data" → green.

Box size: calculated from the number of ports. Minimum height ensures the
label and badge fit. Width is fixed (e.g., 180px world units).

Inspector panel: slides in from the right when a box is selected. Fields
rendered as labeled inputs. Input/output port lists are rendered as
add/remove lists (click + to add a port, click x to remove). Predicate
fields on branch box ports are shown as a comparator dropdown + value input.

## Implementation notes

`assets/index.html` is the single-file editor shell with no build step or framework. `assets/js/002-boxes.js` renders boxes on an HTML5 canvas with port dots, kind badges, and drag support. `assets/js/005-app.js` is the top-level controller: on load it reads server URL and map name from localStorage, fetches all boxes, and drives the render loop. The inspector panel is in `assets/js/004-inspector.js` with autosave on each field change.

## Related documents

- docs/007-architecture.md — box file format (ui.x, ui.y fields)
- issues/105 — server endpoints used by api.js
- issues/107 — wire drawing added on top of this canvas

## Notes

The canvas render loop should only redraw when state changes (box moved,
box selected, camera moved) — not every frame unconditionally. A dirty
flag is sufficient for v1; no need for a scene graph.

The server URL and map name should be stored in localStorage so the user
doesn't have to re-enter them on reload.
