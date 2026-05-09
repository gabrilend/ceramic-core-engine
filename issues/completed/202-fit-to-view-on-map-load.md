# 202 — Fit-to-view on map load

## Status

complete

## Current behavior

When a map loads, the camera stays at its default position (origin, zoom 1).
If boxes were placed at coordinates far from (0,0), they are off-screen and
the user has no way to know where to look.

## Intended behavior

After `load_map()` completes, the camera is moved and zoomed so that all
loaded boxes are visible with a small margin. If the map is empty, the camera
resets to the origin.

## Implementation notes

`App.fit_to_view()` in `assets/js/005-app.js`:

1. Walks `Boxes.boxes` to compute the bounding box (using `box.ui.x`,
   `box.ui.y`, `Boxes.BOX_W`, `Boxes.box_height`).
2. Picks the largest zoom that fits the bounding box with a 60px margin
   on each side, clamped to `[0.25, 1.5]`.
3. Sets `Canvas.cam.zoom`, `Canvas.cam.x`, `Canvas.cam.y` directly. The
   new canvas viewport (post the rewrite at commit 8fbcbcb) uses
   screen-pixel pan offsets, so the centering math is one line:
   `pan = canvas_center - bb_center * zoom`.

`load_map()` calls `fit_to_view()` after populating boxes. A toolbar
"⊡ fit" button calls it directly so the user can re-center at any time.

Empty-map case (no boxes): resets to origin / zoom 1.

## Related documents

- assets/js/001-canvas.js — viewport, cam getters, mark_dirty
- assets/js/002-boxes.js — BOX_W, box_height
- assets/js/005-app.js — load_map, fit_to_view
- assets/index.html — toolbar button
