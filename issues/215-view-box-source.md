# 215 — View source code of a box from the inspector and canvas context menu

## Status
open

## Current behavior
When a box isn't working as expected, the user has no way to inspect its source file
from within the editor. They must find the file path in the inspector's `ref` field
and open it manually in a separate editor.

## Intended behavior
Two entry points to view a box's source:
- Inspector: a "view source" button next to the `ref` field. Clicking replaces the
  inspector content with a read-only code view of the file.
- Canvas right-click context menu: a "view source" item that does the same.

The code view shows the raw file text in a `<pre>` block with a "← back" button to
return to the normal inspector state. No editing — read-only only for now.

## Suggested implementation steps
1. `assets/js/004-inspector.js` — add "view source" button next to the ref field;
   clicking fetches the file via `API.get_extra_src_file` or `API.get_src_file` (based
   on which group the ref belongs to) and calls a `show_source_view(container, text, on_back)`
   helper.
2. `assets/js/005-app.js` — add "view source" item to the canvas right-click context
   menu when a box is selected; calls the same inspector view.
3. `assets/js/004-inspector.js` — `show_source_view(container, text, on_back)` helper:
   renders a `<pre class="fb-source-view">` block, wraps in the inspector panel, adds
   "← back" button.
4. `assets/index.html` — add `.fb-source-view` CSS: monospace, small font, overflow scroll,
   dark background.

## Relevant files
- `assets/js/004-inspector.js` — ref field rendering, inspector panel
- `assets/js/005-app.js` — canvas context menu, show_ctx_menu
- `assets/js/003-api.js` — get_extra_src_file, get_src_file
- `assets/index.html` — CSS
