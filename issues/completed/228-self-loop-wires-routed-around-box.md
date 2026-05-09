# 228 — Self-loop wires: connect a box's output to its own input, routed around the body

## Status
complete

## Implementation notes

Went with option 1 (cubic Bezier, off-axis control points). A single
`endpoint_controls(src, dst, self_loop_box)` helper returns the two
control points; `null` for the third arg gives the default
straight-out controls, a box reference gives the around-the-bottom
loop. Both `draw_all` and `hit_test_wire` route through it, so the
visual wire and the click-detection curve always agree.

Two surprises I hadn't called out in the original write-up:

1. **`src_box === dst_box` for self-loops.** `create_connection` and
   `delete_connection` were pushing/filtering both sides of the
   connection independently; with a self-loop, both sides are the
   same array, so a wire would land in the array twice. Added a
   `self_loop = src_box === dst_box` guard so the push and the PUT
   happen once.
2. **Wire-release blocked self-loops.** `005-app.js` had an explicit
   `target_port.box_id !== drawing_wire.from_box` guard preventing
   the user from ever releasing a wire on the same box. Removed.

### Touches

- `assets/js/006-wires.js` — `endpoint_controls`, `draw_bezier_cp`,
  `LOOP_OFFSET`, self-loop guards in create/delete
- `assets/js/005-app.js` — drop the wire-release self-loop block

### Edge cases verified by inspection

- **Box height changes** with a self-loop attached (e.g., variadic
  slot added growing the box): control points use `box_height(box)`
  per-frame, so the curve adapts.
- **Multiple self-loops on a comparator box** (e.g., `lt` and `eq`
  both routed back): each renders independently against its own
  branch dot, with branch coloring preserved.
- **Hit-test under the box body**: samples follow the looped curve,
  so clicks near the visible wire register; clicks on the box body
  itself are still consumed by `hit_test_box` first.

## Current behavior

The connection model permits a wire whose `from_box` and `to_box`
are the same — `create_connection` checks for exact duplicates but
does not reject self-loops. The wire renderer in
`assets/js/006-wires.js::draw_all` then draws a cubic Bézier from
the output dot (right edge of the box) to the input dot (left edge
of the box), with control points fixed at
`±BEZIER_CTRL_OFFSET = 80px` horizontally. For a self-loop, both
endpoints sit on opposite sides of the same body and the curve cuts
straight through the middle of the box — visually unreadable, and
clipped by the box rectangle.

Hit-testing samples the same Bézier path and inherits the same
problem: clicks land in the wrong place because the wire sits
underneath the box.

## Intended behavior

Self-loops are allowed and render as a wire that loops *around*
the box body, not through it. The user can wire a box's output to
its own input the same way as any other connection (drag from
output port, release on input port).

### Why this matters

Self-loops are useful with iterator boxes (issue 221): an
iterator's output feeds back into its own queued input, building
an accumulator or polling-state pattern. The comparator (issue
210) routes one branch to the input on a self-loop, the other
branch downstream. With queued inputs (phase 3 issue 302) the
loop terminates naturally when the queue empties.

The runtime semantics differ between phase 2 and phase 3:
- **Phase 2 (current)**: the synchronous executor walks dependency
  order. A self-loop on a non-iterator box never resolves and
  hangs the run. The user is responsible for not shipping such
  maps. The editor allows constructing them; the runner errors
  cleanly (or hangs, which is also a clear signal).
- **Phase 3**: the C graph loader (issue 305) rejects non-iterator
  cycles at load time and accepts iterator cycles. The editor
  doesn't need to know which is which; the loader catches misuse.

Either way, the editor's job is just to draw the wire correctly.

### Routing

The wire path for a self-loop goes out the right side from the
output dot, around either the top or the bottom of the box body
(pick deterministically — say, always the bottom), and back to
the input dot on the left side.

Two reasonable shapes:

1. **Cubic Bézier with off-axis control points** — keep the same
   `draw_bezier` machinery but pick control points that pull the
   curve out, down, and around. For a box at world-(x, y) with
   width W and height H, output dot at (x+W, oy) and input dot at
   (x, iy):

   ```
   src = (x+W, oy)
   dst = (x,   iy)
   cp1 = (x+W + LOOP_OFFSET, oy + H)
   cp2 = (x   - LOOP_OFFSET, iy + H)
   ```

   `LOOP_OFFSET` ~ 60–80px gives a smooth horseshoe under the box.

2. **Polyline** — straight segments: right from output dot,
   down past the box bottom, left past the box left edge, up to
   input dot. Visually crisper, easier to hit-test, but stylistically
   inconsistent with every other wire being a Bézier.

Recommendation: option 1. Reuses `draw_bezier` with different
control-point math, no new rendering primitive needed.

### Hit-testing

`hit_test_wire` samples the Bézier path at `HIT_SAMPLES = 20`
points. With the same machinery and routed-around control points,
the samples sit on the curved path under the box, not behind the
body. Clicks register correctly.

The wire-erase pointer-interpolation fix (commit 899f35d) walks
the *cursor* path, not the wire's path. It already works for any
wire shape `hit_test_wire` can match. No additional change needed.

## Suggested implementation

In `assets/js/006-wires.js`:

1. Detect self-loop where the wire is being computed:
   `c.from_box === c.to_box`.
2. When self-loop, compute `cp1` and `cp2` with the looped offsets
   above. The offset is in world units, so it scales with zoom
   correctly.
3. Pass `cp1` / `cp2` into `draw_bezier` (refactor to accept
   explicit control points, or add a sibling `draw_loop_bezier`).
4. `hit_test_wire` does the same self-loop check and uses the
   same `bezier_point` sampling with the looped control points.

```js
function endpoint_controls(c, src, dst, box) {
  if (c.from_box === c.to_box && box) {
    const h = Boxes.box_height(box);
    return {
      cp1: { x: src.x + LOOP_OFFSET,        y: src.y + h * 0.7 },
      cp2: { x: dst.x - LOOP_OFFSET,        y: dst.y + h * 0.7 },
    };
  }
  return {
    cp1: { x: src.x + BEZIER_CTRL_OFFSET, y: src.y },
    cp2: { x: dst.x - BEZIER_CTRL_OFFSET, y: dst.y },
  };
}
```

### Edge cases

- **Box height changes** while a self-loop wire exists (variadic
  slot added → box grows): the wire's control points are derived
  per-frame from current `box_height`, so it adapts automatically.
- **Output is a comparator branch** (`from_branch` is `lt`/`eq`/`gt`):
  the loop renders the same way; the branch coloring still applies.
  A user can wire `lt` back to the same box's input, which is a
  legitimate self-loop pattern for "while less than X, keep going."
- **Multiple self-loops on one box** (different output → different
  inputs, or comparator branches): each renders independently.
  They will overlap; visual cleanup if it ever matters in
  practice.

## Open questions

- **Top vs bottom routing**: always-below is simplest. Always-above
  is also fine. Configurable per-wire (right-click → "route around
  top") is overkill until someone asks for it.
- **Self-loop validation in the schema**: the phase 2 schema
  doesn't reject self-loops, and the phase 3 loader handles cycle
  detection at load time (issue 305). Editor-side validation is
  unnecessary; the wire-draw machinery just needs to render it.

## Relevant files

- `assets/js/006-wires.js` — `draw_all`, `hit_test_wire`,
  `draw_bezier`, control-point math
- `assets/js/002-boxes.js` — `box_height`, `BOX_W`, port positions
- `assets/js/005-app.js` — wire-create flow (no change needed; the
  existing flow already permits self-loop construction)
- `issues/221-iterator-box.md` — iterator self-re-spawn pattern
  that motivates this
- `issues/305-c-graph-loader.md` — phase 3 loader's cycle
  validation policy
