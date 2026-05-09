// Box rendering, layout, and hit testing.
// Each box knows its world-space position; the camera transform is applied by the canvas.

const Boxes = (() => {
  const BOX_W         = 180;
  const BOX_HEADER    = 36;
  const PORT_ROW_H    = 22;
  const PORT_R        = 6;   // port dot radius in world units
  const MIN_BOX_H     = 80;

  const KIND_COLOR = { call: '#4a9eff', data: '#4caf7d' };

  // Comparator branch colors: lt=orange, eq=blue, gt=green
  const BRANCH_COLOR = { lt: '#ff8c42', eq: '#4a9eff', gt: '#4caf7d' };

  // boxes: { [id]: box_data }
  let boxes = {};

  // {{{ box_height
  function box_height(box) {
    const n_in  = (box.inputs || []).length;
    // comparator active: 3 output dots (lt/eq/gt); otherwise: 1 centered dot
    const n_out = (box.comparand && box.comparand !== '') ? 3 : 1;
    return Math.max(MIN_BOX_H, BOX_HEADER + Math.max(n_in, n_out) * PORT_ROW_H + 8);
  }
  // }}}

  // {{{ port_positions
  // Returns world-space {x,y} for each port dot on a box.
  // Output side: null-named single dot when no comparand; 'lt'/'eq'/'gt' dots when comparand set.
  function port_positions(box) {
    const bx = box.ui?.x ?? 0;
    const by = box.ui?.y ?? 0;
    const h  = box_height(box);
    const inputs = box.inputs || [];

    const in_pts = inputs.map((p, i) => ({
      name: p.name, side: 'input',
      x: bx,
      y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
    }));

    let out_pts;
    if (box.comparand && box.comparand !== '') {
      // three comparator dots
      out_pts = ['lt', 'eq', 'gt'].map((branch, i) => ({
        name: branch, side: 'output',
        x: bx + BOX_W,
        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
      }));
    } else {
      // single output dot, vertically centered in the box
      out_pts = [{
        name: null, side: 'output',
        x: bx + BOX_W,
        y: by + h / 2,
      }];
    }

    return { inputs: in_pts, outputs: out_pts };
  }
  // }}}

  // {{{ draw_box
  function draw_box(ctx, box, selected) {
    const x = box.ui?.x ?? 0;
    const y = box.ui?.y ?? 0;
    const w = BOX_W;
    const h = box_height(box);
    const kind  = box.kind || 'call';
    const color = KIND_COLOR[kind] || '#888';

    // shadow when selected
    if (selected) {
      ctx.shadowColor = color;
      ctx.shadowBlur  = 12;
    }

    // body
    ctx.fillStyle   = '#1e2130';
    ctx.strokeStyle = selected ? color : '#3a3f55';
    ctx.lineWidth   = selected ? 2 : 1;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, 6);
    ctx.fill();
    ctx.stroke();

    ctx.shadowBlur = 0;

    // header bar
    ctx.fillStyle = color + '33';
    ctx.beginPath();
    ctx.roundRect(x, y, w, BOX_HEADER, [6, 6, 0, 0]);
    ctx.fill();

    // label
    ctx.fillStyle  = '#e8eaf6';
    ctx.font       = 'bold 12px monospace';
    ctx.textAlign  = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(box.label || box.id, x + 10, y + BOX_HEADER / 2);

    // kind badge (top right)
    ctx.fillStyle  = color;
    ctx.font       = '9px monospace';
    ctx.textAlign  = 'right';
    ctx.fillText(kind.toUpperCase(), x + w - 8, y + BOX_HEADER / 2);

    // port dots and labels
    const { inputs: in_pts, outputs: out_pts } = port_positions(box);

    ctx.font      = '10px monospace';
    ctx.fillStyle = '#9ea3c0';

    in_pts.forEach(p => {
      ctx.beginPath();
      ctx.arc(p.x, p.y, PORT_R, 0, Math.PI * 2);
      ctx.fillStyle   = '#2d3250';
      ctx.strokeStyle = '#6c72a0';
      ctx.lineWidth   = 1.5;
      ctx.fill();
      ctx.stroke();

      ctx.fillStyle  = '#9ea3c0';
      ctx.textAlign  = 'left';
      ctx.textBaseline = 'middle';
      ctx.fillText(p.name, p.x + PORT_R + 4, p.y);
    });

    out_pts.forEach(p => {
      // comparator branches use distinct colors; single output uses kind color
      const dot_color = p.name ? (BRANCH_COLOR[p.name] || color) : color;
      ctx.beginPath();
      ctx.arc(p.x, p.y, PORT_R, 0, Math.PI * 2);
      ctx.fillStyle   = dot_color + '44';
      ctx.strokeStyle = dot_color;
      ctx.lineWidth   = 1.5;
      ctx.fill();
      ctx.stroke();

      // label branch name ('lt'/'eq'/'gt') to the left of the dot; nothing for single wire
      if (p.name) {
        ctx.fillStyle  = dot_color;
        ctx.font       = '9px monospace';
        ctx.textAlign  = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText(p.name, p.x - PORT_R - 4, p.y);
      }
    });
  }
  // }}}

  // {{{ draw_all
  function draw_all(ctx, selected_id) {
    for (const id in boxes) {
      draw_box(ctx, boxes[id], id === selected_id);
    }
  }
  // }}}

  // {{{ hit_test_port
  // Returns {box_id, port_name, side, x, y} for the port under world point (wx,wy), or null.
  // For output dots: port_name is null (single wire) or 'lt'/'eq'/'gt' (comparator).
  function hit_test_port(wx, wy) {
    for (const id in boxes) {
      const { inputs, outputs } = port_positions(boxes[id]);
      for (const p of [...inputs, ...outputs]) {
        const dx = wx - p.x, dy = wy - p.y;
        if (dx*dx + dy*dy <= (PORT_R + 4) * (PORT_R + 4)) {
          return { box_id: id, port_name: p.name, side: p.side, x: p.x, y: p.y };
        }
      }
    }
    return null;
  }
  // }}}

  // {{{ hit_test_box
  // Returns box id if (wx,wy) is inside a box body, or null.
  function hit_test_box(wx, wy) {
    for (const id in boxes) {
      const b = boxes[id];
      const x = b.ui?.x ?? 0, y = b.ui?.y ?? 0;
      if (wx >= x && wx <= x + BOX_W && wy >= y && wy <= y + box_height(b)) return id;
    }
    return null;
  }
  // }}}

  // {{{ get_port_world_pos
  function get_port_world_pos(box_id, port_name, side) {
    const box = boxes[box_id];
    if (!box) return null;
    const { inputs, outputs } = port_positions(box);
    const list = side === 'input' ? inputs : outputs;
    const p = list.find(p => p.name === port_name);
    return p ? { x: p.x, y: p.y } : null;
  }
  // }}}

  return {
    boxes, BOX_W, BOX_HEADER, PORT_R,
    box_height, port_positions,
    draw_all, draw_box, hit_test_port, hit_test_box, get_port_world_pos,
  };
})();
