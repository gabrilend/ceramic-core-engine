# 211 — File browser library directories

## Status

completed

## Current behavior

The file browser only shows files in the map's own `src/` directory. Shared
libraries in `libs/` or other directories cannot be browsed or selected as a
box ref without manually typing the path.

## Intended behavior

A button at the top of the file browser ("+ library dir") prompts the user
for an absolute directory path and stores it in `meta.json.extra_src_dirs`.
These directories appear as collapsible groups in the browser alongside `src/`.
A `▶/▼` toggle collapses or expands each group. Files in extra dirs are fetched
and parsed identically to src/ files.

Server adds two routes:
- `GET /maps/:name/extrasrc` — returns extra dirs and their file listings
- `GET /maps/:name/extrasrc/:index/:file` — returns a file from extra dir at
  that index as text/plain

Extra dirs are stored as absolute paths in `meta.json.extra_src_dirs`. The
server reads them at request time; no restart needed when dirs are added.

## Suggested implementation steps

1. `src/005-http-server.lua` — add `handle_list_extrasrc` and
   `handle_get_extrasrc`; add both to the dispatch table.
2. `assets/js/003-api.js` — add `list_extra_src()` and `get_extra_src_file(i, f)`.
3. `assets/js/007-filebrowser.js` — refactor `render()` to load both src/ and
   extra dirs; render as collapsible groups with toggle state in a local Set;
   add "+ library dir" button that calls `API.put_meta`.
4. `assets/js/007-filebrowser.js` — right-click on an extra-dir group header
   shows a "hide directory" context menu item that removes the path from
   `meta.extra_src_dirs` and re-renders. Uses a private `show_fb_ctx_menu`
   helper (reuses `.ctx-item` CSS; independent from App's context menu).

## Implementation notes

The initial extra-dirs feature was implemented in `src/005-http-server.lua` (`handle_list_extrasrc`, `handle_get_extrasrc`) and `assets/js/007-filebrowser.js` (collapsible groups, context-menu hide). This issue was then superseded by 211a, which unified `src/` and extra dirs into the same `src_dirs` pipeline, eliminating the special-case handling. The `extra_src_dirs` field in meta.json is migrated transparently by the server on first read.

## Related documents

- src/005-http-server.lua — handle_list_src, dispatch table
- assets/js/003-api.js — get_meta, put_meta
- assets/js/007-filebrowser.js — render, render_fn_list
- maps/*/meta.json — extra_src_dirs field
