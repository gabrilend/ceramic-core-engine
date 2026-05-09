# 215 — View source code of a box in a draggable canvas window

## Status
complete

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

- **Width**: the file's longest line clamped to `[80, 120]`
  characters, multiplied by the monospace character width. Tiny
  files don't get pointlessly small windows; very long lines don't
  get pointlessly wide ones. Lines past 120 chars scroll
  horizontally; the user resizes the window if they want more.
  *(Earlier draft used average line length, but the average is
  pulled way down by blank lines and short single-keyword lines, so
  most files opened too narrow.)*
- **Height**: half the canvas's current height.

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

The viewer is `assets/js/008-source-view.js`, a new module exposing
one public function — `SourceView.open_source_view(ref, anchor_box?)`.
Each call creates an absolutely-positioned DOM window, populates it
with the fetched file content, and lets the user drag it around.

### File text fetch
`fetch_source(ref)` strips a leading `src/` prefix and tries
`API.get_src_file` first; on failure it walks every directory
returned by `API.list_extra_src` and tries `API.get_extra_src_file`
in order. Returns the first hit; throws if nothing matches. Plain
text — no JSON parsing.

### Default size
Width derived from the file's longest line, clamped to
`[80, 120]` characters and converted to pixels via a one-time
monospace-character measurement (`measure_char_w` runs at module
load using a hidden probe span). Height is `canvas_rect().height /
2`. Both rounded to integers.

### Drag clamping
The header captures `mousedown` and records anchor positions for
the mouse and the window. `mousemove` computes a candidate
`(left, top)` from the anchor delta and runs it through
`clamp_position(left, top, w, h)` — which restricts the window to
`canvas_rect()`'s bounds — before applying. Anchor-and-delta math
(rather than incremental) avoids accumulating error over a long
drag.

### Resize clamping
Native CSS `resize: both` provides the resize handle in the
bottom-right. A `ResizeObserver` re-applies `clamp_position` after
each size change so growing the window can't push it off-canvas,
and caps `width` / `height` at `canvas_rect()` if the user drags
past those bounds.

### Multiple windows + z-order
`open_windows[]` tracks every live window. A `z_top` counter
increments on every `bring_to_front` call (header drag, anywhere
click) so the just-touched window is always on top.

### Default position
If `anchor_box` is provided, the window is centered over that
box's screen position (via `Canvas.world_to_screen`). Otherwise
centered on the canvas. Either way `clamp_position` runs so the
default never starts off-canvas.

### CSS
`.source-view-window`, `.source-view-header`, `.source-view-title`,
`.source-view-close`, `.source-view-body` in `assets/index.html`.
Header uses the editor accent color; body uses the editor's
monospace font and dark background.

### Entry points
- Inspector "view" button next to the ref field in
  `assets/js/004-inspector.js`. Disabled when ref is empty (status
  bar reports the issue).
- Canvas right-click context menu in `assets/js/005-app.js`. The
  "view source" item appears for boxes that have a ref;
  the window is anchored to the right-clicked box.

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

## Open questions / future work

- Bring-to-front on click: implemented via a global z-counter.
- Edit support: explicitly out of scope. The viewer is read-only.
  Editing source files happens in the user's preferred editor on
  disk; the viewer just shows what's there. (A future "edit in
  editor" issue could add a "save back" capability with conflict
  warnings against on-disk mtime.)
- **Syntax highlighting**: tracked separately as issue 223.

## Relevant files

- `assets/js/008-source-view.js` — new module (to be created)
- `assets/js/004-inspector.js` — ref field rendering, inspector panel
- `assets/js/005-app.js` — canvas context menu, show_ctx_menu
- `assets/js/003-api.js` — get_src_file, get_extra_src_file
- `assets/index.html` — CSS for the floating window
- `assets/js/001-canvas.js` — canvas bounding rect for clamping
