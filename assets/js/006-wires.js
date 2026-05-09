// Wire rendering, hit testing, and connection management.
// Wires are derived from box connection data — no separate wire store.

const Wires = (() => {
  const BEZIER_CTRL_OFFSET = 80;
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

  // {{{ draw_bezier
  function draw_bezier(ctx, x0, y0, x1, y1, color, selected) {
    const cx0 = x0 + BEZIER_CTRL_OFFSET;
    const cy0 = y0;
    const cx1 = x1 - BEZIER_CTRL_OFFSET;
    const cy1 = y1;

    ctx.beginPath();
    ctx.moveTo(x0, y0);
    ctx.bezierCurveTo(cx0, cy0, cx1, cy1, x1, y1);
    ctx.strokeStyle = selected ? '#ffffff' : color;
    ctx.lineWidth   = selected ? 2.5 : 1.5;
    ctx.setLineDash([]);
    ctx.stroke();
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

        const cx0 = src.x + BEZIER_CTRL_OFFSET, cy0 = src.y;
        const cx1 = dst.x - BEZIER_CTRL_OFFSET, cy1 = dst.y;

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

        draw_bezier(ctx, src.x, src.y, dst.x, dst.y, color, is_selected);
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
  // Fetch-before-PUT: read both boxes, add the connection record, write both.
  async function create_connection(from_box_id, from_branch, to_box_id, to_input) {
    try {
      const [src_box, dst_box] = await Promise.all([
        API.get_box(from_box_id),
        API.get_box(to_box_id),
      ]);

      const conn = {
        from_box:    from_box_id,
        from_branch: from_branch ?? null,
        to_box:      to_box_id,
        to_input:    to_input,
      };

      src_box.connections = src_box.connections || [];
      dst_box.connections = dst_box.connections || [];

      // avoid duplicates
      const dup = src_box.connections.some(c =>
        c.from_box    === conn.from_box    &&
        (c.from_branch ?? null) === conn.from_branch &&
        c.to_box      === conn.to_box      &&
        c.to_input    === conn.to_input
      );
      if (dup) return;

      src_box.connections.push(conn);
      dst_box.connections.push(conn);

      await Promise.all([
        API.put_box(from_box_id, src_box),
        API.put_box(to_box_id,   dst_box),
      ]);

      // update local cache
      Boxes.boxes[from_box_id] = src_box;
      Boxes.boxes[to_box_id]   = dst_box;

      const branch_str = from_branch ? '.' + from_branch : '';
      status_msg('connected ' + from_box_id + branch_str + ' → ' + to_box_id + '.' + to_input);
      Canvas.mark_dirty();
    } catch(e) {
      status_msg('connect error: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ delete_connection
  async function delete_connection(conn) {
    try {
      const [src_box, dst_box] = await Promise.all([
        API.get_box(conn.from_box),
        API.get_box(conn.to_box),
      ]);

      const match = c =>
        c.from_box    === conn.from_box    &&
        (c.from_branch ?? null) === (conn.from_branch ?? null) &&
        c.to_box      === conn.to_box      &&
        c.to_input    === conn.to_input;

      src_box.connections = (src_box.connections || []).filter(c => !match(c));
      dst_box.connections = (dst_box.connections || []).filter(c => !match(c));

      await Promise.all([
        API.put_box(conn.from_box, src_box),
        API.put_box(conn.to_box,   dst_box),
      ]);

      Boxes.boxes[conn.from_box] = src_box;
      Boxes.boxes[conn.to_box]   = dst_box;

      _selected_wire = null;
      status_msg('deleted connection');
      Canvas.mark_dirty();
    } catch(e) {
      status_msg('delete wire error: ' + e.message, 'error');
    }
  }
  // }}}

  return { draw_all, draw_bezier, hit_test_wire, create_connection,
           delete_connection, selected_wire };
})();
