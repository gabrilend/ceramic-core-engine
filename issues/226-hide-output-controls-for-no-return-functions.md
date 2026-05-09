# 226 — Hide output port and inspector output section when the function returns nothing

## Status
open

## Current behavior

When the file browser parses a function that has no return values
(e.g. `function M.write_result(text) print(text) end` — sink, no
returns), the on-select handler sets `box.inputs` from the parsed
signature but doesn't carry through any "this function has no
output" signal. The inspector then renders the output section
(comparator toggle + comparand input) for every call box,
unconditionally, and the canvas draws an output port dot.

The UI implies you can wire from the output, but at runtime the
function returns nil and the wire just propagates a nil value.
Visually noisy; conceptually misleading.

## Intended behavior

A box whose function has no return values:
- Has no output port dot on the canvas.
- Has no output section in the inspector (no compare toggle, no
  comparand input, no output area at all).
- Cannot have outgoing wires drawn from it. The wire-drawing
  interaction in `005-app.js` skips it because there's no output
  port to grab.

The single-output-wire-per-box rule from issue 218 still holds for
boxes with return values; this issue carves out the explicit "no
output" case.

## Where the signal comes from

The Lua and Bash parsers in `assets/js/007-filebrowser.js` already
return a `fn.outputs` array — empty when the function has no
return statements. The on-select handler currently does not store
this on the box. The fix is to persist a boolean flag (or the
empty `outputs` array itself) so the inspector and canvas can
check it.

Two options:

1. **Persist `box.has_output: false`** when the parser detects no
   return values. The inspector and canvas check this field. New
   field, but unambiguous.
2. **Persist `box.outputs: []`** (the empty array). The inspector
   and canvas treat `box.outputs === []` as "no output." Reuses
   the legacy field that was previously used for named output
   ports (issue 210 superseded it; this gives it a new
   single-purpose meaning).

Option 1 is cleaner because the name says exactly what it means.
Option 2 reuses an existing-but-stale field, which has the
advantage of zero schema churn but the disadvantage of overloading
the field's meaning. Lean toward option 1.

## Edge cases

- **Manually-added boxes** that haven't been wired through the
  file browser: no information about return values exists. Default
  to `has_output: true` (current behavior). The user can flip it
  manually if they want a sink box without going through the file
  browser.
- **Functions whose return value depends on a branch** (some
  return, some don't): the parser already lists every distinct
  return shape. As long as ANY return statement exists, treat the
  function as having an output. `has_output: false` only when
  zero return statements.
- **Comparator toggle**: if the user activates the comparator on
  a non-output box, that's nonsensical (there's no value to
  compare). The comparator UI only renders when the box has an
  output anyway, so this falls out naturally.

## Suggested implementation sequence

1. `assets/js/007-filebrowser.js::on_select` (in
   `004-inspector.js`) — when `fn.outputs.length === 0`, set
   `current_box.has_output = false`. Otherwise delete the field
   (treat absence as `true`).
2. `assets/js/004-inspector.js::show` — wrap the entire output
   section render in `if (box.has_output !== false) { ... }`.
3. `assets/js/002-boxes.js::port_positions` — when
   `box.has_output === false`, return an empty `outputs` array.
   `draw_box` already iterates `outputs` and draws nothing for an
   empty array.
4. `assets/js/006-wires.js::create_connection` — early-out if the
   `from_box` has `has_output === false` (defensive — the canvas
   wouldn't have a port to grab from, but cover the edge case).
5. `src/001-schema.lua` — accept `has_output` as an optional
   boolean field. No validation beyond type-check.
6. Test: open the hello map, look at `write-result` (which is a
   sink). After re-browsing it through the file browser, output
   port should disappear. Existing wires from a now-output-less
   box are dropped on save (preferable to dangling).

## Open questions

- Should existing wires from a box that flips to `has_output:
  false` be dropped automatically, or should the flip be rejected
  if any outgoing wires exist? Auto-drop with a status-bar
  warning is the smoother path.
- Should the file browser do this automatically on every re-pick,
  or only the first time a function is selected? Doing it every
  time matches what we do for `box.inputs` (always overwritten on
  re-select), and keeps the box's `has_output` flag in sync with
  the actual function.

## Relevant files

- `assets/js/004-inspector.js` — `show` (output section render),
  `open_browser` (on-select handler that sets the flag)
- `assets/js/007-filebrowser.js` — parsers that detect zero returns
- `assets/js/002-boxes.js` — `port_positions`, `draw_box`
- `assets/js/006-wires.js` — `create_connection` (defensive check)
- `src/001-schema.lua` — schema accepts `has_output`
- `issues/completed/218-enforce-single-output-driver-contract.md`
  — single-wire rule that this issue qualifies
