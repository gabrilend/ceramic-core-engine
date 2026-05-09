# 206 — Entry box designation

## Status

won't implement

## Resolution

The phase 3 runtime (issue 305) auto-detects entry boxes from graph
topology: any box with zero inputs, or whose inputs are all wired
only to `data` boxes, is an entry. A map can have any number of
entries. There is no singular `main`. The `meta.json entry_box_id`
field becomes vestigial when phase 3 lands and replaces the phase 2
synchronous runner.

Implementing the editor UI for `entry_box_id` would be throwaway
work — UI for a field the next runtime ignores. Closing without
implementation. The phase 2 runner continues to read
`meta.json entry_box_id` as it does today; the editor simply does
not surface it.

If a future need surfaces for marking "this box should fire first"
in the phase 3 model — for instance, a manual override of the
auto-detected entry set — a new issue captures that requirement
against the phase 3 design directly.

See:
- `issues/305-c-graph-loader.md` — entry-box detection in phase 3
- `docs/001-architecture.md` — current architecture overview

## Current behavior

The entry box is set in `meta.json` (`entry_box_id` field), but the editor
has no way to show which box is the entry point or to change it. The user
must edit meta.json directly (via the server API or a text editor) to set or
change the entry box.

## Intended behavior

The entry box is visually distinct on the canvas — a small marker or badge
(e.g. a filled triangle or "▶" label) drawn in the top-left corner of the
box header.

Any box can be designated as the entry box from the inspector. When a box is
selected, a "set as entry" button appears at the top of the inspector panel.
Clicking it updates `meta.json` via PUT /maps/:name/meta and redraws the
canvas.

Only one box can be the entry at a time. Changing the entry box immediately
updates the visual marker on the canvas.

## Suggested implementation steps

1. In `005-app.js` `load_map()`, also fetch `meta.json` and store
   `App.meta` (or a module-level variable) with the current entry_box_id.
2. In `002-boxes.js` `draw_box()`, if `box.id === entry_box_id`, draw a
   small "▶" badge in the header area (e.g., left of the label, in the
   accent color for the box kind).
3. In `004-inspector.js` `show()`, add a "set as entry" button at the top
   of the inspector. On click: update the local entry_box_id, call
   `API.put_meta({...meta, entry_box_id: box.id})`, mark canvas dirty.
4. Pass `entry_box_id` into `draw_all` so the renderer knows which box to
   mark.

## Related documents

- assets/js/002-boxes.js — draw_box
- assets/js/004-inspector.js — show
- assets/js/005-app.js — load_map, meta storage
- assets/js/003-api.js — get_meta, put_meta
- docs/001-architecture.md — meta.json format
