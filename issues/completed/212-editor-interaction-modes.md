# 212 — Editor interaction modes

## Status

complete

## Current behavior

Creating a box requires a double-click on the canvas. There is no dedicated
wire-erase tool; wires must be clicked individually to select them and then
deleted with the keyboard. Deleting a box that has incoming connections fails
with a server 409 error because the server checks for references before allowing
the delete.

## Intended behavior

Two tool-mode buttons appear in the top toolbar:

**+ box**: clicking the button enters add-box mode (button highlights). The
next click on the canvas places a new box at the cursor position and returns
to select mode automatically.

**✕ wire**: clicking the button enters erase-wire mode (button highlights).
While the mouse button is held down and the cursor is dragged across the canvas,
any wire the cursor passes over is immediately erased. Releasing the mouse button
exits erase-wire mode and returns to select mode.

Double-click to create a box is removed; the + box tool is the only creation
path.

Box deletion is unconditional: before calling the server DELETE, the client
strips all connection records referencing the box from every other box (PUT each
affected box), then issues the DELETE. The server's 409 reference guard is no
longer triggered. No confirmation dialog is shown.

## Implementation notes

The shipped behavior diverges from the original spec in one place:

- **Box creation** is via the canvas right-click context menu ("add
  box here") rather than a dedicated `+ box` toolbar mode. One click
  instead of two, and consistent with how other context-driven
  actions in the editor work.
- **Wire erasure** is the dedicated `✕ wire` toolbar tool as
  originally specced — click the button, hold-and-drag across the
  canvas, wires under the cursor are erased.
- `tool_mode` has values `'select' | 'erase-wire'` (no `'add-box'`).
- **Box deletion** strips connection references from every other box
  client-side before issuing the server `DELETE`, avoiding the 409
  reference-guard error.

`set_tool_mode(mode)` (in `005-app.js`) manages the active button
class and canvas cursor (`'cell'` for erase-wire). Erase-wire tracks
a Set of already-queued wire signatures per drag to avoid
double-deletes. Connections are removed from the local Boxes cache
immediately (for instant visual feedback) and then deleted from the
server asynchronously.

## Suggested implementation steps

1. `005-app.js` — add `tool_mode`, `set_tool_mode`, `wire_sig` helpers.
   Update mousedown/mousemove/mouseup handlers for new modes.
   Rewrite `delete_box` to strip references before deleting.
   Remove dblclick handler.
2. `index.html` — add `#tool-add-box` and `#tool-erase-wire` buttons to toolbar.
   Add `.toolbar-btn.active` CSS.

## Related documents

- assets/js/005-app.js — App closure, event handlers
- assets/index.html — toolbar HTML and CSS
