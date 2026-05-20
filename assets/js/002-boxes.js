// Box rendering, layout, and hit testing.
// Each box knows its world-space position; the camera transform is applied by the canvas.

const Boxes = (() => {
  const BOX_W         = 180;
  const BOX_HEADER    = 36;
  const PORT_ROW_H    = 22;
  const PORT_R        = 6;   // port dot radius in world units
  // Issue 239: a literal-bound input port renders as a smaller
  // "nodule" with the port name pulled into the gutter to its
  // left. Half the radius reads visually as "this is a port, but
  // it's bound — don't drag a wire to it." NODULE_R and the
  // 12-char gutter clip are the two visual constants of that
  // rendering.
  const NODULE_R      = 3;
  const GUTTER_MAX_CHARS = 12;
  const MIN_BOX_H     = 80;

  // KIND_COLOR drives the box header colour on the canvas. `read`
  // keeps the green that data boxes used to wear; `write` takes a
  // warm cheddar so source-of-value and sink-to-disk read as
  // clearly different at a glance (issue 229).
  const KIND_COLOR = {
    call:  '#4a9eff',
    read:  '#4caf7d',
    write: '#d4762a',
  };

  // Issue 235: input ports with a literal value show the value on the
  // canvas in place of the port name. ~24 chars fits comfortably in
  // BOX_W=180 at 10px monospace; anything longer truncates with an
  // ellipsis. Multi-line values collapse to their first line.
  const MAX_LABEL_CHARS = 24;

  // Issue 236: default header text per box kind, used when the user
  // has not set an explicit label. `call` is the special case —
  // basename of its `ref` is the answer, so we resolve it at draw
  // time. Add a row when a new kind ships.
  const KIND_HEADER = {
    branch:     'branch',
    read:       'read',
    write:      'write',
    comparator: 'comparator',
    iterator:   'iterator',
  };

  // Comparator branch colors: lt=orange, eq=blue, gt=green
  const BRANCH_COLOR = { lt: '#ff8c42', eq: '#4a9eff', gt: '#4caf7d' };

  // boxes: { [id]: box_data }
  let boxes = {};

  // {{{ default_header
  // The box header text used when `box.label` is unset (issue 236).
  // Three cases:
  //   call    — basename of `ref`, the source file the box calls
  //   other   — the kind name, plain
  //   unknown — fall back to the box id so nothing renders blank
  // Exported so the inspector can show it as the label-field
  // placeholder, keeping the inspector and canvas in sync about
  // "what would render if I left this empty".
  function default_header(box) {
    if (box.kind === 'call' && box.ref) {
      const parts = String(box.ref).split('/');
      return parts[parts.length - 1] || box.ref;
    }
    return KIND_HEADER[box.kind] || box.kind || box.id;
  }
  // }}}

  // {{{ output_label_text
  // Picks the string drawn to the LEFT of an output port dot
  // (issue 236). Two paths:
  //   p.name set → render it (comparator branches, iterator slots)
  //   p.name null, box is a call → render `fn()` so the wire reads
  //     as "the return value of calling fn"
  //   otherwise → null, draw no label
  function output_label_text(box, p) {
    if (p.name) return p.name;
    if (box.kind === 'call' && box.fn) return box.fn + '()';
    return null;
  }
  // }}}

  // {{{ port_has_incoming_wire
  // A wire on a port is the source of truth for that port's value at
  // runtime; any literal value sitting on the same port is dormant.
  // Issue 235 surfaces this by deciding the canvas label — port name
  // when wired (so the reader doesn't mistake the dormant value for
  // what's flowing through), value otherwise.
  function port_has_incoming_wire(box, port_name) {
    if (!box.connections) return false;
    for (const c of box.connections) {
      if (c.to_box === box.id && c.to_input === port_name) return true;
    }
    return false;
  }
  // }}}

  // {{{ port_is_literal_bound
  // A port is "literal-bound" when the inspector has a value set
  // and no wire is feeding the port. That's the state issue 239
  // renders specially — small nodule, name in the gutter, value
  // on the right — to make it obvious at a glance that the value
  // is the binding, not a default.
  //
  // The three-state check (value set / wired / both) also lives
  // in port_label_text; the helper exists so both renderers and
  // hit-testing share one truth.
  function port_is_literal_bound(box, port_name) {
    const p = (box.inputs || []).find(q => q.name === port_name);
    if (!p) return false;
    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
    if (!has_value) return false;
    return !port_has_incoming_wire(box, p.name);
  }
  // }}}

  // {{{ clip_gutter_name
  function clip_gutter_name(name) {
    const s = String(name || '');
    if (s.length <= GUTTER_MAX_CHARS) return s;
    return s.slice(0, GUTTER_MAX_CHARS - 1) + '…';
  }
  // }}}

  // {{{ port_label_text
  // Picks the string drawn next to an input port dot. Three cases:
  //   - wire attached, or no value → port name (parameter identity)
  //   - value set, no wire         → the value (what the box does)
  //   - value too long / multiline → first line, truncated with …
  // The two-branch decision is the visible heart of issue 235: a
  // reader scanning the map should see the data the box operates on,
  // not the parameter slot it sits in.
  function port_label_text(box, p) {
    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
    if (!has_value)                              return p.name;
    if (port_has_incoming_wire(box, p.name))     return p.name;
    const raw        = String(p.value);
    const first_line = raw.split('\n')[0];
    const multiline  = first_line.length < raw.length;
    if (first_line.length > MAX_LABEL_CHARS) {
      return first_line.slice(0, MAX_LABEL_CHARS - 1) + '…';
    }
    if (multiline) return first_line + '…';
    return first_line;
  }
  // }}}

  // {{{ visible_inputs
  // Issue 229: a `read` box with an inline literal `value` set has no
  // file to read, so its `path` input port doesn't belong on the
  // canvas — showing one would imply "wire me to a path" and confuse
  // the reader. Inspector still owns the path field; the canvas just
  // hides the port. Every other layout path (height, port positions,
  // hit testing) goes through this helper so the box reads
  // consistently.
  function visible_inputs(box) {
    const all = box.inputs || [];
    if (box.kind === 'read' && box.value !== undefined
        && box.value !== null && box.value !== '') {
      return all.filter(p => p.name !== 'path');
    }
    return all;
  }
  // }}}

  // {{{ box_height
  function box_height(box) {
    const n_in  = visible_inputs(box).length;
    // Output rows: iterator boxes have N named slots (issue 221);
    // comparator active: 3 dots (lt/eq/gt); sink: 0; otherwise 1.
    let n_out;
    if (box.iterator_outputs) {
      n_out = box.iterator_outputs.length;
    } else if (box.has_output === false) {
      n_out = 0;
    } else if (box.comparand && box.comparand !== '') {
      n_out = 3;
    } else {
      n_out = 1;
    }
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
    const inputs = visible_inputs(box);

    const in_pts = inputs.map((p, i) => ({
      name: p.name, side: 'input',
      x: bx,
      y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
    }));

    let out_pts;
    if (box.has_output === false) {
      // sink box — function has no return values (issue 226). No output
      // port, no wire-grab target, nothing for the inspector to render.
      out_pts = [];
    } else if (box.iterator_outputs) {
      // iterator box (issue 221) — N named output dots stacked down
      // the right edge, one per declared slot. Slot names land in the
      // `name` field, so the wire's `from_branch` will pick them up
      // exactly like a comparator branch name.
      out_pts = box.iterator_outputs.map((slot, i) => ({
        name: slot, side: 'output',
        x: bx + BOX_W,
        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
      }));
    } else if (box.comparand && box.comparand !== '') {
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

    // label — user override wins; otherwise the kind-driven default
    // (issue 236): basename of ref for call boxes, plain kind name
    // for everything else.
    ctx.fillStyle  = '#e8eaf6';
    ctx.font       = 'bold 12px monospace';
    ctx.textAlign  = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(box.label || default_header(box), x + 10, y + BOX_HEADER / 2);

    // kind badge (top right)
    ctx.fillStyle  = color;
    ctx.font       = '9px monospace';
    ctx.textAlign  = 'right';
    ctx.fillText(kind.toUpperCase(), x + w - 8, y + BOX_HEADER / 2);

    // port dots and labels
    const { inputs: in_pts, outputs: out_pts } = port_positions(box);

    ctx.font      = '10px monospace';
    ctx.fillStyle = '#9ea3c0';

    in_pts.forEach((p, i) => {
      // Issue 239 split: a literal-bound port renders as a nodule
      // (smaller dot) with the port name pulled into the gutter
      // outside the box's left edge. Plain wired-or-empty ports
      // keep the full dot. The two rendering shapes communicate at
      // a glance whether the port accepts a wire (full dot) or is
      // committed to its literal (nodule).
      const literal_bound = port_is_literal_bound(box, p.name);
      const radius = literal_bound ? NODULE_R : PORT_R;

      ctx.beginPath();
      ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
      ctx.fillStyle   = '#2d3250';
      ctx.strokeStyle = '#6c72a0';
      ctx.lineWidth   = 1.5;
      ctx.fill();
      ctx.stroke();

      // Gutter name for literal-bound ports (issue 239). The port's
      // parameter identity goes to the LEFT of the box edge,
      // textAlign right so the right edge of the text sits just
      // shy of where the dot would be — the reader sees both the
      // name and the value on one line. Clipped at GUTTER_MAX_CHARS.
      if (literal_bound) {
        ctx.fillStyle  = '#6c72a0';
        ctx.font       = '10px monospace';
        ctx.textAlign  = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText(clip_gutter_name(p.name), p.x - PORT_R - 4, p.y);
      }

      // Right-side label. For literal-bound ports this is the
      // value (235's rule, unchanged); for plain ports it's the
      // port name (or value-without-wire — 235 already handles
      // the dispatch). Drawn in world space so it scales with
      // zoom. Editing still happens in the inspector.
      ctx.fillStyle  = '#9ea3c0';
      ctx.font       = '10px monospace';
      ctx.textAlign  = 'left';
      ctx.textBaseline = 'middle';
      ctx.fillText(port_label_text(box, box.inputs.find(q => q.name === p.name)),
                   p.x + PORT_R + 4, p.y);
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

      // Output label rendered to the LEFT of the dot. Branch names
      // ('lt'/'eq'/'gt') and iterator slot names come straight from
      // p.name; a call box's single output gets `fn()` synthesized
      // (issue 236) so the wire reads as the return value of
      // calling the function.
      const out_label = output_label_text(box, p);
      if (out_label) {
        ctx.fillStyle  = dot_color;
        ctx.font       = '9px monospace';
        ctx.textAlign  = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText(out_label, p.x - PORT_R - 4, p.y);
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
  //
  // Issue 239: literal-bound INPUT ports are excluded from hit
  // testing. The user can't start or land a wire on a port whose
  // value is already committed by a literal — the visual nodule
  // declares "this isn't a wire target," and the hit-test honors
  // that. To rebind, clear the literal in the inspector first;
  // the port returns to a full dot and becomes targetable.
  function hit_test_port(wx, wy) {
    for (const id in boxes) {
      const box = boxes[id];
      const { inputs, outputs } = port_positions(box);
      for (const p of inputs) {
        if (port_is_literal_bound(box, p.name)) continue;
        const dx = wx - p.x, dy = wy - p.y;
        if (dx*dx + dy*dy <= (PORT_R + 4) * (PORT_R + 4)) {
          return { box_id: id, port_name: p.name, side: p.side, x: p.x, y: p.y };
        }
      }
      for (const p of outputs) {
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

  // {{{ port_full_value_if_truncated
  // For the canvas tooltip (issue 235): if a port is showing a
  // shortened version of its literal value (multi-line collapse or
  // ellipsis), return the raw value so the tooltip can present it
  // in full. Returns null when the displayed label is already the
  // whole value, or when the label is the port name (wired / no
  // value). Looking up the index by name keeps callers honest —
  // they can ask about a port without knowing its row.
  function port_full_value_if_truncated(box, port_name) {
    if (!box.inputs) return null;
    const i = box.inputs.findIndex(p => p.name === port_name);
    if (i < 0) return null;
    const p = box.inputs[i];
    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
    if (!has_value) return null;
    if (port_has_incoming_wire(box, p.name)) return null;
    const raw = String(p.value);
    return raw === port_label_text(box, p) ? null : raw;
  }
  // }}}

  return {
    boxes, BOX_W, BOX_HEADER, PORT_R,
    box_height, port_positions,
    draw_all, draw_box, hit_test_port, hit_test_box, get_port_world_pos,
    port_full_value_if_truncated, default_header,
  };
})();
