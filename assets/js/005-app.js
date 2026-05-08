// Top-level app: connects everything, loads the map, drives the render loop.

const App = (() => {
  let selected_id   = null;
  let drag_box_id   = null;
  let drag_offset   = { x: 0, y: 0 };
  let drawing_wire  = null;  // { from_box, from_port, from_side, cur_x, cur_y }
  let hover_port    = null;
  let status_el     = document.getElementById('status');

  // {{{ status_msg
  window.status_msg = function(msg, level = 'info') {
    status_el.textContent = msg;
    status_el.className   = 'status ' + level;
  };
  // }}}

  // {{{ load_map
  async function load_map() {
    status_msg('loading…');
    try {
      const ids = await API.list_boxes();
      const fetches = ids.map(id => API.get_box(id));
      const all = await Promise.all(fetches);
      // mutate the existing object so the draw_all closure sees the new boxes
      Object.keys(Boxes.boxes).forEach(k => delete Boxes.boxes[k]);
      all.forEach(b => { Boxes.boxes[b.id] = b; });
      Canvas.mark_dirty();
      status_msg('loaded ' + ids.length + ' boxes');
    } catch(e) {
      status_msg('load error: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ new_box_at
  async function new_box_at(wx, wy) {
    const id = 'box-' + Date.now().toString(36);
    const box = {
      id, label: 'New Box', kind: 'call',
      ref: '', fn: '', inputs: [], outputs: [], connections: [],
      ui: { x: Math.round(wx), y: Math.round(wy) },
    };
    try {
      await API.put_box(id, box);
      Boxes.boxes[id] = box;
      selected_id = id;
      Inspector.show(box, () => Canvas.mark_dirty());
      Canvas.mark_dirty();
    } catch(e) {
      status_msg('create error: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ render
  function render() {
    if (Canvas.start_frame()) {
      const ctx = Canvas.ctx;
      Wires.draw_all(ctx, selected_id);
      Boxes.draw_all(ctx, selected_id);

      // draw in-progress wire
      if (drawing_wire) {
        const src = Boxes.get_port_world_pos(drawing_wire.from_box, drawing_wire.from_port, 'output');
        if (src) {
          Wires.draw_bezier(ctx, src.x, src.y, drawing_wire.cur_x, drawing_wire.cur_y,
            '#6c72a0', false);
        }
      }
    }
    requestAnimationFrame(render);
  }
  // }}}

  // Mouse interaction
  Canvas.el.addEventListener('mousedown', e => {
    if (e.button !== 0) return;
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    // port hit?
    const port = Boxes.hit_test_port(w.x, w.y);
    if (port && port.side === 'output') {
      drawing_wire = { from_box: port.box_id, from_port: port.port_name,
                       cur_x: w.x, cur_y: w.y };
      return;
    }

    // box hit?
    const box_id = Boxes.hit_test_box(w.x, w.y);
    if (box_id) {
      selected_id = box_id;
      drag_box_id = box_id;
      const b = Boxes.boxes[box_id];
      drag_offset = { x: w.x - (b.ui?.x ?? 0), y: w.y - (b.ui?.y ?? 0) };
      Inspector.show(b, () => Canvas.mark_dirty());
      Canvas.mark_dirty();
      return;
    }

    // click on empty space: deselect
    selected_id = null;
    Inspector.hide();
    Canvas.mark_dirty();
  });

  Canvas.el.addEventListener('mousemove', e => {
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    if (drawing_wire) {
      drawing_wire.cur_x = w.x;
      drawing_wire.cur_y = w.y;
      Canvas.mark_dirty();
      return;
    }

    if (drag_box_id) {
      const b = Boxes.boxes[drag_box_id];
      if (b) {
        b.ui = b.ui || {};
        b.ui.x = Math.round(w.x - drag_offset.x);
        b.ui.y = Math.round(w.y - drag_offset.y);
        Canvas.mark_dirty();
      }
    }
  });

  Canvas.el.addEventListener('mouseup', async e => {
    if (e.button !== 0) return;
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    if (drawing_wire) {
      const target_port = Boxes.hit_test_port(w.x, w.y);
      if (target_port && target_port.side === 'input' &&
          target_port.box_id !== drawing_wire.from_box) {
        await Wires.create_connection(
          drawing_wire.from_box, drawing_wire.from_port,
          target_port.box_id, target_port.port_name
        );
      }
      drawing_wire = null;
      Canvas.mark_dirty();
      return;
    }

    if (drag_box_id) {
      // persist position on mouseup
      const b = Boxes.boxes[drag_box_id];
      if (b) {
        try { await API.put_box(b.id, b); }
        catch(e) { status_msg('save position failed: ' + e.message, 'error'); }
      }
      drag_box_id = null;
    }
  });

  // Double-click to create box
  Canvas.el.addEventListener('dblclick', async e => {
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);
    if (!Boxes.hit_test_box(w.x, w.y)) {
      await new_box_at(w.x, w.y);
    }
  });

  // Delete key: remove selected box or wire
  window.addEventListener('keydown', async e => {
    if (e.code !== 'Delete' && e.code !== 'Backspace') return;
    if (document.activeElement && document.activeElement.tagName === 'INPUT') return;

    const sel_wire = Wires.selected_wire();
    if (sel_wire) {
      await Wires.delete_connection(sel_wire);
      Canvas.mark_dirty();
      return;
    }

    if (selected_id) {
      const b = Boxes.boxes[selected_id];
      const conns = (b && b.connections || []).filter(c => c.from_box === selected_id);
      if (conns.length > 0) {
        const refs = conns.map(c => c.to_box).join(', ');
        if (!confirm(`Box "${selected_id}" has connections to: ${refs}.\nDelete anyway?`)) return;
      }
      try {
        await API.delete_box(selected_id);
        delete Boxes.boxes[selected_id];
        Inspector.hide();
        selected_id = null;
        Canvas.mark_dirty();
      } catch(e) {
        status_msg('delete error: ' + e.message, 'error');
      }
    }
  });

  // {{{ init
  async function init() {
    let server_url = localStorage.getItem('soramech_server') || '';
    let map_name   = localStorage.getItem('soramech_map')    || '';

    if (!server_url || !map_name) {
      server_url = prompt('Server URL:', 'http://localhost:7700') || 'http://localhost:7700';
      map_name   = prompt('Map name:',   'hello')                || 'hello';
      localStorage.setItem('soramech_server', server_url);
      localStorage.setItem('soramech_map',    map_name);
    }

    document.getElementById('map-label').textContent = map_name;
    API.init(server_url, map_name);
    await load_map();
    requestAnimationFrame(render);
  }
  // }}}

  window.addEventListener('DOMContentLoaded', init);
  return { load_map };
})();
