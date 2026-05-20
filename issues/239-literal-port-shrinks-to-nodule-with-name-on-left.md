# 239 — Literal-bound input ports shrink to a nodule, port name moves to the left

## Status
open

## Current behavior

Per issue 235 (complete), an input port whose literal value is
set renders with the value in place of the port name. The dot
on the box's left edge stays full-size, and the port name
disappears from the canvas — only the value is visible. The
inspector still owns the name, and the dot still works as a
wire-grab target even though wiring into a port with a literal
value would severs that literal on connect.

What the user reading the map can see at a glance:

```
●  "hello world"
```

The port's variable name (`text`, `prompt`, whatever it is in
the underlying function signature) is invisible on the canvas.
A reader has to open the inspector to recover the binding.

## Intended behavior

A port with a literal value rendered in three pieces, in three
positions, in this order left-to-right:

```
text   ·   "hello world"
```

- **Port name** (`text`) — moved to the LEFT of the box's edge,
  in the gutter just outside the box body. This is the
  variable's identity, and 235's value-takes-over rendering
  hid it. Pulling it out to the left puts the name back in
  plain sight WITHOUT giving up the value display.
- **Nodule** — a smaller, denser dot sitting where the regular
  port dot would. About half the radius of a wired port's dot.
  Visual marker only — it advertises "this port is bound" the
  same way the regular dot advertises "this port can be wired."
  Color match its kind, same as the regular dot, so the eye
  reads it as a port without confusion.
- **Literal value** (`"hello world"`) — to the RIGHT of the
  nodule, same position the regular port name occupies on a
  wired port. Truncation rules from 235 still apply (24-char
  limit with ellipsis, tooltip for the full value).

A wired port stays exactly as it is today — full-size dot,
name to its right, no left-of-box-edge label.

### Wire-connect targeting disabled

A literal-bound port is no longer a valid drag-target for
wires. `Boxes.hit_test_port` excludes literal-bound input ports
from its hit search, so:

- The cursor over a nodule does NOT highlight the port for a
  wire-grab.
- The user cannot start drawing a wire from / to a literal-bound
  port. To rebind, they clear the literal value in the inspector
  first; the port then renders as a wirable dot again.

Today, dropping a wire onto a port with a literal value silently
clears the literal (per the existing inspector code, line ~656
in `004-inspector.js`). That behavior was a defensible
compromise pre-239 because the value was the only visible
state — losing it to a wire was reversible by re-typing.
Post-239 the user is explicitly indicating "this is bound, no
wires," so we honor that instead of dropping a value on accident.

### Why this is better than the 235-only rendering

- The user reading a map sees BOTH the variable identity and
  the bound value, on one line, without inspector lookup.
- The nodule is unambiguously "not a wire target," so the user
  doesn't waste a drag attempt only to learn the literal was
  silently cleared.
- The left-of-edge name uses canvas gutter space that is
  currently empty, so the box body's horizontal budget is
  unaffected.

### Edge cases

- **Multiple ports with literals**: each gets its own left-of-
  edge name. Boxes with several literal-bound ports show a
  cleanly-stacked column of names in the gutter.
- **Variadic slot with a literal**: same rule as any other
  port. The variadic group's auto-grow / hide-empty-trailing
  rules from issue 217 are unaffected.
- **Port name long enough to collide with the left-of-edge
  gutter of an adjacent box**: clip with ellipsis at the box's
  max gutter width (TBD constant; ~12 monospace chars at
  10px gives ~72 world-px gutter, which is fine for typical
  identifiers). Tooltip on hover for the full name, same as
  235's value tooltip.

## Suggested implementation steps

1. `assets/js/002-boxes.js`:
   - Add a `port_is_literal_bound(box, i)` helper. Returns true
     when the port has a value AND no incoming wire. (The same
     three-state check `port_label_text` already does — pull it
     into a shared helper.)
   - In `draw_box`'s input-port loop: when literal-bound, draw
     the nodule (half-radius, same fill/stroke style as a normal
     dot) instead of the full dot; render the port name to the
     LEFT of the dot position (textAlign right, with the box's
     world x as the right edge); the value label drawing
     continues unchanged (235 path).
   - `hit_test_port`: skip ports that are literal-bound on
     inputs. The output side is unaffected (outputs don't carry
     literal values).
2. `assets/js/006-wires.js::create_connection`: the
   sever-literal-on-connect path in 004-inspector.js becomes
   unreachable because the literal port can't be a drag target.
   It can stay as a defensive guard in case some other code
   path forces a connection.
3. Tests: extend the 235 case set with two new cases:
   - "wire attached, literal present" → still renders as wired
     (wire wins, no nodule).
   - "literal only" → renders nodule + left-name + right-value.

## Open questions

- **Nodule radius**: half (3px world) is a starting guess. Visual
  weight will tell us if it should be smaller; refine after
  shipping. Constant in 002-boxes.js so it's a one-line change.
- **Gutter clip width**: 12 chars at 10px is a guess. Could be
  expressed as a fraction of `BOX_W` to scale if BOX_W ever
  changes. Probably not worth parameterizing today.
- **Inspector indication of "this port is literal-bound"**: the
  inspector already shows the value field; that's the only
  binding state. No additional UI needed there — the canvas is
  the place where the visual rule applies.

## Relevant files

- `assets/js/002-boxes.js` — `draw_box` input-port path,
  `hit_test_port`, helpers
- `assets/js/006-wires.js` — sever-on-connect guard
- `assets/js/004-inspector.js` — sever-on-set guard (still
  fires when the user sets a literal through the inspector
  while a wire is attached; the wire-into-literal direction is
  the one this issue closes off)
- `issues/completed/235-literal-value-replaces-port-name-on-canvas.md`
  — the predecessor; the value-on-canvas rule this issue
  retains, the dot-stays-the-same-size rule this issue
  supersedes
