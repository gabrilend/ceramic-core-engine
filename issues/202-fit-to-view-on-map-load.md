# 202 — Fit-to-view on map load

## Status

open

## Blockers

- 201 (boxes must render before fit-to-view can be tested)

## Current behavior

When a map loads, the camera stays at its default position (origin, zoom 1).
If boxes were placed at coordinates far from (0,0), they are off-screen and
the user has no way to know where to look.

## Intended behavior

After `load_map()` completes, the camera is moved and zoomed so that all
loaded boxes are visible with a small margin. If the map is empty, the camera
resets to the origin.

## Suggested implementation steps

1. In `005-app.js`, after populating `Boxes.boxes`, compute the bounding box
   of all box positions (using `box.ui.x`, `box.ui.y`, and `Boxes.box_height`).
2. Compute the center of the bounding box and set `cam.x` / `cam.y` to center
   it in the canvas.
3. Compute zoom so the bounding box fits within the canvas with ~60px margin
   on each side. Clamp zoom to [0.25, 1.5].
4. Call `Canvas.mark_dirty()`.
5. Expose a `fit_to_view()` function from App and wire it to a toolbar button
   so the user can re-center at any time.

## Related documents

- assets/js/001-canvas.js — cam, mark_dirty
- assets/js/002-boxes.js — box_height, BOX_W
- assets/js/005-app.js — load_map
