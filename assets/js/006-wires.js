// Wire rendering, hit testing, and connection management.
// Wires are derived from box connection data — no separate wire store.

const Wires = (() => {
  const BEZIER_CTRL_OFFSET = 80;
  // LOOP_OFFSET pulls a self-loop's control points further out and
  // below the box body, so the curve sweeps around the bottom rather
  // than cutting straight through the rectangle (issue 228).
  const LOOP_OFFSET        = 80;
  const HIT_SAMPLES        = 20;
  const HIT_THRESHOLD_PX   = 8;

  // Colors for comparator branch wires; plain wire uses default
  const BRANCH_COLOR = { lt: '#ff8c42', eq: '#4a9eff', gt: '#4caf7d' };

  let _selected_wire = null;  // { from_box, from_branch, to_box, to_input }

  // {{{ selected_wire
  function selected_wire() { return _selected_wire; }
  // }}}

  // {{{ bezier_point
  function bezier_point(t, x0, y0, cx0, cy0, cx1, cy1, x1, y1) {
    const mt = 1 - t;
    return {
      x: mt*mt*mt*x0 + 3*mt*mt*t*cx0 + 3*mt*t*t*cx1 + t*t*t*x1,
      y: mt*mt*mt*y0 + 3*mt*mt*t*cy0 + 3*mt*t*t*cy1 + t*t*t*y1,
    };
  }
  // }}}

  // {{{ endpoint_controls
  // Returns the two cubic-Bezier control points for a wire from src to
  // dst. For a normal wire the controls jut horizontally out of each
  // port. For a self-loop (output and input on the same box body), the
  // controls swing out and below so the curve routes around the
  // rectangle instead of slicing through it (issue 228).
  function endpoint_controls(src, dst, self_loop_box) {
    if (self_loop_box) {
      const h = Boxes.box_height(self_loop_box);
      return {
        cx0: src.x + LOOP_OFFSET, cy0: src.y + h * 0.7,
        cx1: dst.x - LOOP_OFFSET, cy1: dst.y + h * 0.7,
      };
    }
    return {
      cx0: src.x + BEZIER_CTRL_OFFSET, cy0: src.y,
      cx1: dst.x - BEZIER_CTRL_OFFSET, cy1: dst.y,
    };
  }
  // }}}

  // {{{ draw_bezier_cp
  // Draws a cubic Bezier with explicit control points. The wrapper
  // draw_bezier (kept for the in-progress wire-drag preview) calls
  // this with the default straight-out controls.
  function draw_bezier_cp(ctx, x0, y0, cx0, cy0, cx1, cy1, x1, y1, color, selected) {
    ctx.beginPath();
    ctx.moveTo(x0, y0);
    ctx.bezierCurveTo(cx0, cy0, cx1, cy1, x1, y1);
    ctx.strokeStyle = selected ? '#ffffff' : color;
    ctx.lineWidth   = selected ? 2.5 : 1.5;
    ctx.setLineDash([]);
    ctx.stroke();
  }
  // }}}

  // {{{ draw_bezier
  function draw_bezier(ctx, x0, y0, x1, y1, color, selected) {
    const cx0 = x0 + BEZIER_CTRL_OFFSET;
    const cy0 = y0;
    const cx1 = x1 - BEZIER_CTRL_OFFSET;
    const cy1 = y1;
    draw_bezier_cp(ctx, x0, y0, cx0, cy0, cx1, cy1, x1, y1, color, selected);
  }
  // }}}

  // {{{ hit_test_wire
  // Returns the connection record if cursor (wx,wy) is within HIT_THRESHOLD pixels
  // of any wire, or null. Operates in world space; threshold is adjusted for zoom.
  function hit_test_wire(wx, wy, zoom) {
    const threshold = HIT_THRESHOLD_PX / zoom;
    for (const id in Boxes.boxes) {
      const box = Boxes.boxes[id];
      for (const c of box.connections || []) {
        if (c.from_box !== id) continue;

        const src  = Boxes.get_port_world_pos(c.from_box, c.from_branch ?? null, 'output');
        const dst  = Boxes.get_port_world_pos(c.to_box,   c.to_input,            'input');
        if (!src || !dst) continue;

        // Self-loop wires use the looped control points, so sample
        // along that same curve — clicks register on the visual wire
        // path rather than on a phantom straight-through path.
        const self_box = (c.from_box === c.to_box) ? Boxes.boxes[c.from_box] : null;
        const { cx0, cy0, cx1, cy1 } = endpoint_controls(src, dst, self_box);

        for (let i = 0; i <= HIT_SAMPLES; i++) {
          const t = i / HIT_SAMPLES;
          const p = bezier_point(t, src.x, src.y, cx0, cy0, cx1, cy1, dst.x, dst.y);
          const dx = wx - p.x, dy = wy - p.y;
          if (dx*dx + dy*dy <= threshold*threshold) return c;
        }
      }
    }
    return null;
  }
  // }}}

  // {{{ draw_all
  function draw_all(ctx, selected_box_id) {
    for (const id in Boxes.boxes) {
      const box = Boxes.boxes[id];
      for (const c of box.connections || []) {
        if (c.from_box !== id) continue;

        const src = Boxes.get_port_world_pos(c.from_box, c.from_branch ?? null, 'output');
        const dst = Boxes.get_port_world_pos(c.to_box,   c.to_input,            'input');
        if (!src || !dst) continue;

        // comparator branches get distinct colors; plain wire uses dim default
        const color = c.from_branch ? (BRANCH_COLOR[c.from_branch] || '#4a6080') : '#4a6080';

        const is_selected = _selected_wire &&
          _selected_wire.from_box    === c.from_box &&
          _selected_wire.from_branch === c.from_branch &&
          _selected_wire.to_box      === c.to_box &&
          _selected_wire.to_input    === c.to_input;

        // self-loop wires need control points that route around the
        // box body; everything else uses the default straight-out controls
        const self_box = (c.from_box === c.to_box) ? box : null;
        const { cx0, cy0, cx1, cy1 } = endpoint_controls(src, dst, self_box);
        draw_bezier_cp(ctx, src.x, src.y, cx0, cy0, cx1, cy1, dst.x, dst.y,
                       color, is_selected);
      }
    }
  }
  // }}}

  // Handle wire selection via click
  Canvas.el.addEventListener('click', e => {
    if (e.button !== 0) return;
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    // only try wire hit if no box was hit
    if (Boxes.hit_test_box(w.x, w.y)) { _selected_wire = null; return; }
    if (Boxes.hit_test_port(w.x, w.y)) return;

    const wire = hit_test_wire(w.x, w.y, Canvas.cam.zoom);
    if (wire) {
      _selected_wire = wire;
      const branch_str = wire.from_branch ? '.' + wire.from_branch : '';
      status_msg('wire ' + wire.from_box + branch_str + ' → ' + wire.to_box + '.' +
        wire.to_input + ' — press Delete to remove');
    } else {
      _selected_wire = null;
    }
    Canvas.mark_dirty();
  });

  // {{{ create_connection
  // from_branch: null (no comparator) or 'lt'/'eq'/'gt'.
  //
  // Mutates the cached box objects in place. Replacing the reference
  // (e.g. with a freshly-fetched copy from API.get_box) used to break
  // any other module holding a reference to the box — most painfully
  // the inspector, whose current_box reference would silently go
  // stale after a wire was created, causing subsequent variadic
  // operations to operate on out-of-date data and clobber the just-
  // added wire on save. Mutating in place keeps every reference
  // valid.
  async function create_connection(from_box_id, from_branch, to_box_id, to_input) {
    const src_box = Boxes.boxes[from_box_id];
    const dst_box = Boxes.boxes[to_box_id];
    if (!src_box || !dst_box) {
      status_msg('connect error: box not in cache', 'error');
      return;
    }
    // Defensive: a sink box has no output port (issue 226), so the
    // canvas wouldn't hand us a wire start from it. Refuse anyway in
    // case some other path tries to fabricate one.
    if (src_box.has_output === false) {
      status_msg('connect error: source box is a sink', 'error');
      return;
    }

    const conn = {
      from_box:    from_box_id,
      from_branch: from_branch ?? null,
      to_box:      to_box_id,
      to_input:    to_input,
    };

    src_box.connections = src_box.connections || [];
    dst_box.connections = dst_box.connections || [];

    // avoid exact duplicates (same source, branch, dest, input)
    const dup = src_box.connections.some(c =>
      c.from_box    === conn.from_box    &&
      (c.from_branch ?? null) === conn.from_branch &&
      c.to_box      === conn.to_box      &&
      c.to_input    === conn.to_input
    );
    if (dup) return;

    // For self-loops src_box === dst_box, so push only once; otherwise
    // both ends record the connection in their respective arrays
    // (issue 228).
    const self_loop = src_box === dst_box;
    src_box.connections.push(conn);
    if (!self_loop) dst_box.connections.push(conn);

    try {
      if (self_loop) {
        await API.put_box(from_box_id, src_box);
      } else {
        await Promise.all([
          API.put_box(from_box_id, src_box),
          API.put_box(to_box_id,   dst_box),
        ]);
      }
    } catch (e) {
      // roll back the push so the in-memory state matches the server
      src_box.connections.pop();
      if (!self_loop) dst_box.connections.pop();
      status_msg('connect error: ' + e.message, 'error');
      return;
    }

    const branch_str = from_branch ? '.' + from_branch : '';
    status_msg('connected ' + from_box_id + branch_str + ' → ' + to_box_id + '.' + to_input);
    Canvas.mark_dirty();

    // Auto-grow the destination box's variadic group when the wire
    // landed on its last slot (issue 217 part B). Idempotent — the
    // helper checks first whether to_input is the last slot of its
    // group and exits if not.
    if (Inspector.auto_grow_after_set) {
      await Inspector.auto_grow_after_set(dst_box, to_input);
    }
  }
  // }}}

  // {{{ delete_connection
  // Same in-place mutation pattern as create_connection — never
  // replace the cached references.
  async function delete_connection(conn) {
    const src_box = Boxes.boxes[conn.from_box];
    const dst_box = Boxes.boxes[conn.to_box];
    if (!src_box || !dst_box) {
      status_msg('delete wire error: box not in cache', 'error');
      return;
    }

    const match = c =>
      c.from_box    === conn.from_box    &&
      (c.from_branch ?? null) === (conn.from_branch ?? null) &&
      c.to_box      === conn.to_box      &&
      c.to_input    === conn.to_input;

    // Self-loops: src_box === dst_box, so a single filter+PUT covers
    // both ends of the conceptual connection (issue 228).
    const self_loop   = src_box === dst_box;
    const src_before  = src_box.connections || [];
    const dst_before  = dst_box.connections || [];
    src_box.connections = src_before.filter(c => !match(c));
    if (!self_loop) dst_box.connections = dst_before.filter(c => !match(c));

    try {
      if (self_loop) {
        await API.put_box(conn.from_box, src_box);
      } else {
        await Promise.all([
          API.put_box(conn.from_box, src_box),
          API.put_box(conn.to_box,   dst_box),
        ]);
      }
    } catch (e) {
      // roll back
      src_box.connections = src_before;
      if (!self_loop) dst_box.connections = dst_before;
      status_msg('delete wire error: ' + e.message, 'error');
      return;
    }

    _selected_wire = null;
    status_msg('deleted connection');
    Canvas.mark_dirty();
  }
  // }}}

  return { draw_all, draw_bezier, hit_test_wire, create_connection,
           delete_connection, selected_wire };
})();
