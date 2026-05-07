# 107 — Web editor: wiring and connection UI

## Status

open

## Blockers

- 106 (canvas and box rendering must exist before wires can be drawn)

## Current behavior

Boxes can be drawn on the canvas but there is no way to connect them.
Connection data exists in box files but is not visualized.

## Intended behavior

The canvas renders wires between connected boxes. The user can draw new
wires by dragging from an output port dot to an input port dot. Wires
can be deleted by clicking them and pressing delete.

When a wire is created:
  1. Both endpoint box files are updated via PUT (connection added to both)
  2. If the connection creates a cycle that does not pass through a branch
     box, the editor warns but does not prevent the connection (the runner
     will reject it at load time; the editor is not the enforcement point)

When a wire is deleted:
  1. Both endpoint box files are updated via PUT (connection removed from both)

Wire rendering:
  - Bezier curve from output port dot to input port dot
  - Control points offset horizontally by ~80 world units to give a smooth S curve
  - Color: neutral (grey) by default; selected wire highlighted
  - Branch box ports: each port has its own wire color (cycling palette)
    so multi-port wires are visually distinct

## Suggested implementation steps

1. Write assets/js/006-wires.js — wire rendering (bezier) and hit testing
   (sample points along curve, check distance to cursor).
2. Drag-to-wire: on mousedown on a port dot, enter "drawing" state. Render
   a bezier from the source port to the current cursor position. On mouseup
   over a target port dot, create the connection. On mouseup elsewhere,
   cancel.
3. Type compatibility warning: if the source output type and the target
   input type are declared and do not match, show a warning tooltip on the
   wire. Do not prevent the connection — type declarations are hints, not
   enforcement in v1.
4. Connection persistence: after creating or deleting a wire, fetch both
   affected box files, update the connections arrays, and PUT both. The
   fetch-then-update pattern ensures we have the latest file state before
   modifying it (avoids overwriting concurrent edits from another browser).
5. Write assets/js/007-delete.js — delete key handler. If a wire is
   selected, removes it. If a box is selected, removes the box (prompts
   if the box has connections — lists them).

## Related documents

- docs/001-architecture.md — connection format (both-ends invariant)
- issues/106 — canvas and port dot rendering
- issues/108 — branch box port colors and predicate display on wires

## Notes

Wire hit testing on a bezier curve is non-trivial. A practical approach:
sample 20 evenly spaced points along the curve, find the minimum distance
to the cursor. If under a threshold (e.g., 6px screen space), the wire
is hit. This is fast enough for the number of wires expected.

The fetch-before-PUT pattern in step 4 is important. Without it, a
connection added from two different browsers simultaneously can cause one
to overwrite the other's change. In v1, last-write-wins is acceptable
(no locking), but fetching first reduces the window.
