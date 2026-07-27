# 206 — Entry box designation

## Status

reopened 2026-07-26 — the original resolution called this right and
then only half of it happened. Phase 3 did land, entry detection is
derived from topology exactly as predicted, and `entry_box_id` did
become vestigial. What nobody went back and did was **retire the
field**. It is still mandatory, still parsed, and still printed as
though it meant something.

Three concrete leftovers:

1. `src/001-schema.lua` (~line 443) hard-errors on a `meta.json`
   with no `entry_box_id`. So every map author is still required
   to name an entry box that the runtime will not consult. A
   mandatory field with no effect is worse than an absent one —
   it teaches a model of execution that isn't real.
2. `src/008-pool-runner.c` (~line 348) prints `entry %s` in the
   startup banner from `graph_entry_box_id()`. That is the field's
   only remaining consumer anywhere in the C runtime.
3. Map fixtures and the quickstart in `docs/001-overview.md`
   commonly point `entry_box_id` at a **read** box — the one kind
   `detect_entry_boxes` categorically excludes, since read boxes
   never run as tasks at all. So the banner routinely names, as
   the entry, a box that cannot be one.

`docs/004-runtime.md` now documents the real derivation rule and
flags the field as decorative, so the docs no longer mislead. This
issue is about deciding what the field *is*.

## Intended behavior

**Ruling from the map author, 2026-07-26: there should not be one
single entry box. Drop the field.**

The derived set is the truth and becomes the only answer. A map
starts at every box that cannot be waiting on anything, which is
usually several boxes, and the runtime stops pretending otherwise.
The manual-override door that the original resolution left open
stays shut — nothing has asked to go through it, and an override
would reintroduce exactly the singular-`main` model this rejects.

Concretely:

- `meta.json` no longer carries `entry_box_id`. A map that still
  has one loads fine; the field is ignored, not an error, so no
  existing map breaks.
- The schema checker stops requiring it. This is the change that
  matters most to map authors — right now they are compelled to
  invent a value for a field with no effect.
- The startup banner prints the **derived set**, not a single id.
  Something in the shape of `3 entry boxes: seed, tick, config`,
  and for a map with none — which is legal but always a mistake,
  since nothing can ever fire — a loud warning rather than
  `entry (none)` tucked into a line nobody reads.
- The editor gains nothing here. The original issue's canvas badge
  and "set as entry" button stay unbuilt, and now stay unbuilt on
  purpose rather than pending: there is no entry box to designate.
  Marking the *derived* entries on the canvas is a legitimate but
  separate idea, and would be a read-only visualisation, not a
  control.

## Suggested implementation steps

1. `src/001-schema.lua` — delete the `entry_box_id` requirement
   from the `meta.json` check (~line 443). No replacement check;
   an absent field is now simply normal.
2. `src/010-graph-loader.c` — stop parsing `entry_box_id` into
   `g->entry_box_id` (~line 895), and drop the field and its
   `graph_entry_box_id()` accessor. `detect_entry_boxes` and the
   `graph_n_entry_boxes` / `graph_entry_box` accessors are
   untouched — they were always the real mechanism.
3. `src/008-pool-runner.c` — rewrite the banner (~line 348) to
   report the derived set, and warn loudly when it is empty.
4. Map fixtures under `tests/maps/` and the demo maps carry
   `entry_box_id` in their `meta.json`. They can keep it (it is
   ignored) or be swept; sweeping is tidier and is a pure
   deletion.
5. `docs/001-overview.md` — the quickstart's `meta.json` teaches
   the field. Remove it there once the schema stops demanding it,
   not before, or the quickstart produces a map that fails
   validation.
6. `docs/002-map-model.md` and `docs/007-architecture.md` both
   describe `meta.json`; drop the field from each.

Sequence matters only between 1 and 5 — the doc cannot stop
teaching a mandatory field while it is still mandatory.

## Original resolution (2026, phase 2 — kept for the record)

The reasoning below is still sound and is why the field was left
alone at the time. Note it speaks of `data` boxes, the pre-229
name for what are now `read` boxes.

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
- `docs/004-runtime.md` — "Which boxes start the run", the
  derivation rule as it actually ships
- `docs/007-architecture.md` — components and data flow (this was
  written as `docs/001-architecture.md`, a file that no longer
  exists; 007 replaced it)

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
- docs/001-overview.md — meta.json shown in the quickstart
  (referenced here originally as `docs/001-architecture.md`, which
  no longer exists; the quickstart is where the field is taught)
- src/001-schema.lua — the check that still makes the field
  mandatory
- src/008-pool-runner.c — the banner, the field's last consumer
