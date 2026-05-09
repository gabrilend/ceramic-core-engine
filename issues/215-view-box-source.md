# 215 — View source code of a box in a draggable canvas window

## Status
open

## Current behavior
When a box isn't working as expected, the user has no way to inspect
its source file from within the editor. They must find the file path
in the inspector's `ref` field and open it manually in a separate
editor.

## Intended behavior

A floating, draggable, resizable, scrollable read-only source-code
window that appears over the canvas. Two entry points open it:

1. **Inspector** — a "view source" button next to the `ref` field.
2. **Canvas right-click context menu** — a "view source" item when
   a box is selected.

Both call into the same window-creation helper. Multiple source
windows can be open at once (one per box the user opens), each
independently positioned.

### Window behavior

- **Floating**: rendered as an absolutely-positioned DOM element
  layered above the canvas (not drawn into the canvas 2D context).
  This makes scroll, text selection, and resize the browser's
  native behavior — no per-frame redraw needed.
- **Draggable**: a header bar at the top of the window is the drag
  handle. Mousedown on the header initiates drag; mousemove updates
  the window's `left` / `top`; mouseup releases.
- **Confined to the canvas area**: the window cannot be dragged
  outside the canvas's bounding box. The drag handler clamps
  position so the window stays fully inside (drag stops at edges).
  Resize is similarly clamped — the window cannot extend past the
  canvas boundary.
- **Resizable**: standard CSS `resize: both` on the inner pre or a
  custom resize grip in the bottom-right corner. Either way, the
  resize is constrained to keep the window inside the canvas.
- **Scrollable**: the inner content area scrolls when the file
  exceeds the window's dimensions. Standard browser scrolling, both
  axes if needed.
- **Closeable**: an `×` button in the header closes the window.

### Default size

Computed from the file content when the window opens:

- **Width**: the average line length of the file (in characters)
  plus three, multiplied by the monospace character width. Three
  extra characters of breathing room past the average. Lines longer
  than the window scroll horizontally.
- **Height**: half the canvas's current height. The user resizes if
  they want more or less.

If the file is empty, fall back to a sensible default (e.g. 40
characters wide, half-canvas tall).

### Default position

Centered over the box that the user opened the source for, when
opened from the canvas context menu. Centered on the canvas when
opened from the inspector. If the default position would push the
window outside the canvas, clamp to the nearest edge.

### Window structure

```
┌─ classify.lua ──────────────────────────── × ┐  ← drag handle
│ local M = {}                                  │
│                                               │
│ function M.classify(text)                     │  ← <pre> with file content
│   if text:match("^%d+$") then ...             │
│   ...                                         │
│ end                                           │
│                                               │
│ return M                                      │
└───────────────────────────────────────────────┘
                                              ↘ ← resize grip
```

## Implementation notes

### File text fetch
The same API call as the existing file-browser preview (issue 207's
`API.get_src_file`). The `ref` field on the box is the file path
relative to the map. Some refs may be in `libs/` (vendored or
shipped), some in the map's `src/` — the fetch routes appropriately
(via `API.get_src_file` for the map's source, `API.get_extra_src_file`
for libs/extra dirs). The existing browser does this distinction
already; the source viewer reuses it.

### Average line length

```js
const lines = text.split('\n');
const avg = lines.reduce((a, l) => a + l.length, 0) / lines.length;
const width_chars = Math.ceil(avg) + 3;
```

Multiply by the monospace character width (computed once via a
hidden `<span>` measurement at load, or hard-coded as ~7.2px for the
font we use).

### Drag clamping
On mousemove during drag:

```js
const canvas_rect = Canvas.el.getBoundingClientRect();
const win_w = win.offsetWidth;
const win_h = win.offsetHeight;
const new_x = clamp(mouse_x - drag_offset_x, canvas_rect.left, canvas_rect.right - win_w);
const new_y = clamp(mouse_y - drag_offset_y, canvas_rect.top,  canvas_rect.bottom - win_h);
```

Same idea for resize: the new size cannot push the window past the
canvas edges given its current position.

### Multiple windows

Each window is a separately-tracked DOM node. A small registry
(`open_source_windows[]`) tracks them so they can all be closed when
the map is switched, and so a click on a window brings it to the
front (z-index increment).

### CSS

Floating window styles are scoped under a class like
`.source-view-window`. Header bar uses the project's existing accent
color. The `<pre>` body uses the same monospace font as the
inspector's existing port labels.

## Suggested implementation sequence

1. `assets/js/008-source-view.js` (new) — module exposing
   `open_source_view(box, ref, anchor_box?)`. Handles fetch, default
   sizing, window creation, drag/resize bindings, close.
2. `assets/index.html` — add `.source-view-window`,
   `.source-view-header`, `.source-view-body` CSS.
3. `assets/js/004-inspector.js` — add a "view source" button next
   to the ref field; clicks call into the new module.
4. `assets/js/005-app.js` — add "view source" item to the canvas
   right-click context menu when a box is selected; clicks call the
   same module with the selected box as `anchor_box`.
5. Test: open the same source from two boxes (or twice from one
   box) — verify two independent windows. Drag both around. Resize
   both. Confirm clamping at canvas edges.

## Open questions

- Bring-to-front on click: standard expectation, easy to implement
  via a global z-counter. Worth doing.
- Edit support: explicitly out of scope for now. Read-only. Editing
  source files happens in the user's preferred editor on disk; the
  viewer just shows what's there. (A future "edit in editor" issue
  could add a "save back" capability with conflict warnings against
  on-disk mtime.)

## Relevant files

- `assets/js/008-source-view.js` — new module (to be created)
- `assets/js/004-inspector.js` — ref field rendering, inspector panel
- `assets/js/005-app.js` — canvas context menu, show_ctx_menu
- `assets/js/003-api.js` — get_src_file, get_extra_src_file
- `assets/index.html` — CSS for the floating window
- `assets/js/001-canvas.js` — canvas bounding rect for clamping
