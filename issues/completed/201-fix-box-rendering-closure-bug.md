# 201 — Fix box rendering closure bug

## Status

complete

## Fix

`005-app.js` `load_map()` now mutates the existing `Boxes.boxes` object
instead of replacing it, so the `draw_all` closure sees the loaded boxes.

## Current behavior

Boxes load from the server ("loaded N boxes" appears in the status bar) but
nothing is drawn on the canvas. The inspector and all interaction also fail
silently.

## Root cause

`005-app.js` `load_map()` does:
```javascript
Boxes.boxes = {};
all.forEach(b => { Boxes.boxes[b.id] = b; });
```

`Boxes.boxes` is the exported reference to the closure-private `boxes`
variable inside `002-boxes.js`. Assigning `Boxes.boxes = {}` replaces the
property on the module object with a new object, but `draw_all`, `hit_test_box`,
and all other functions inside the closure still read from the original `boxes`
variable. The original object stays empty forever.

## Intended behavior

Boxes render as soon as `load_map()` completes.

## Suggested fix

Mutate the existing object instead of replacing it:
```javascript
// clear
Object.keys(Boxes.boxes).forEach(k => delete Boxes.boxes[k]);
// repopulate
all.forEach(b => { Boxes.boxes[b.id] = b; });
```

## Related documents

- assets/js/002-boxes.js — closure owner
- assets/js/005-app.js — load_map, new_box_at (same pattern)
