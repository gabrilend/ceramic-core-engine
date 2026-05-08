# 211a — Unify src/ into the same directory pipeline as extra dirs

## Status

completed

## Current behavior
The file browser treats `src/` as a special case:
- `src/` is always shown (implicit, not stored in meta.json)
- Extra dirs are stored in `meta.json` → `extra_src_dirs`
- The server has two separate endpoints: `/maps/:name/src` and `/maps/:name/extrasrc`
- The client calls both and merges them
- The context menu "hide directory" only works for extra dirs (`if (group.path)`)

The implicit exception violates the principle that all dirs go through the same pipeline.

## Intended behavior
All browseable directories, including `src/`, flow through the same data path:
- `meta.json` gains a `src_dirs` field: an array of absolute paths
- `src/` is just the first entry; extra dirs are appended as before
- One server endpoint returns all of them
- One client call fetches all of them
- "Hide" removes from `src_dirs` for any entry, including `src/`
- No `if (group.path)` exception in the UI — all groups are treated identically

Migration: if `src_dirs` is absent in meta.json, the server synthesizes it at read-time
from the map's `src/` path + any existing `extra_src_dirs` entries.

## Suggested implementation steps

1. `src/005-http-server.lua` — update `handle_list_extrasrc` and `handle_get_extrasrc`
   to read from `src_dirs` (with migration from `extra_src_dirs` + implicit `src/`).
2. `assets/js/007-filebrowser.js` — remove `list_src_files()` call; only use
   `list_extra_src()`; remove separate `src/` group construction; remove `if (group.path)`
   exception; update hide/add actions to use `src_dirs` field.
3. `scripts/create-map.sh` — initialize `src_dirs` in meta.json with the map's absolute
   `src/` path at creation time.

## Implementation notes

The `src_dirs` field in meta.json now holds all browseable directories including the map's own `src/`. The server's `resolve_src_dirs()` migrates old maps on read (synthesizes from `extra_src_dirs` + implicit `src/`). The client in `assets/js/007-filebrowser.js` uses only the `groups` array as its source of truth for add/hide actions. Three bugs were found and fixed during testing: add/hide reading stale `meta.src_dirs` instead of resolved `groups`, the dir picker always opening at `/home`, and a collapse-key collision that used label instead of path.

## Bugs found during testing

**Bug: add/hide handlers read `meta.src_dirs` instead of `groups`.**
The server migrates old maps on read (synthesizes `src_dirs` from `extra_src_dirs` + implicit
`src/`), but the client's add and hide handlers read raw `meta.src_dirs || []`, which is
`[]` on old maps. So adding a library dir saves `src_dirs: [new_path]` with no prior entries,
discarding whatever the server just migrated. Fix: both handlers must derive the current list
from the already-resolved `groups` array (the live server view), not from `meta.src_dirs`.

**Bug: dir picker opens at `/home` every time.**
Should open at the parent of the last directory in `groups`, so the user lands near where they
last browsed. Fixed by passing a computed `initial_path` to `show_dir_picker`.

## Relevant files
- `src/005-http-server.lua` — handle_list_extrasrc, handle_get_extrasrc
- `assets/js/007-filebrowser.js` — render, show_file_list, show_dir_picker
- `scripts/create-map.sh` — meta.json template
- `maps/*/meta.json` — needs `src_dirs` field (server migrates on first read)
