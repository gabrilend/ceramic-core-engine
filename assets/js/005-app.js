// Top-level app: connects everything, loads the map, drives the render loop.

const App = (() => {
  let selected_id   = null;
  let drag_box_id   = null;
  let drag_offset   = { x: 0, y: 0 };
  let drawing_wire  = null;  // { from_box, from_port, cur_x, cur_y }
  let hover_port    = null;
  let status_el     = document.getElementById('status');

  // tool_mode: 'select' | 'erase-wire'
  let tool_mode  = 'select';
  let erasing    = false;
  let erase_seen = new Set();  // wire signatures already queued for deletion this drag

  // context menu
  const ctx_menu = document.createElement('div');
  ctx_menu.id = 'context-menu';
  document.body.appendChild(ctx_menu);

  function show_ctx_menu(screen_x, screen_y, items) {
    ctx_menu.innerHTML = '';
    items.forEach(item => {
      if (item === 'sep') {
        const el = document.createElement('div');
        el.className = 'ctx-sep';
        ctx_menu.appendChild(el);
        return;
      }
      const el = document.createElement('div');
      el.className = 'ctx-item' + (item.danger ? ' danger' : '');
      el.textContent = item.label;
      el.onclick = () => { hide_ctx_menu(); item.action(); };
      ctx_menu.appendChild(el);
    });
    ctx_menu.style.display = 'block';
    ctx_menu.style.left = screen_x + 'px';
    ctx_menu.style.top  = screen_y + 'px';
    // keep on screen
    const r = ctx_menu.getBoundingClientRect();
    if (r.right  > window.innerWidth)  ctx_menu.style.left = (screen_x - r.width)  + 'px';
    if (r.bottom > window.innerHeight) ctx_menu.style.top  = (screen_y - r.height) + 'px';
  }

  function hide_ctx_menu() { ctx_menu.style.display = 'none'; }

  // hide context menu on any click or scroll
  document.addEventListener('click',    hide_ctx_menu);
  document.addEventListener('keydown',  e => { if (e.key === 'Escape') hide_ctx_menu(); });

  // {{{ status_msg
  window.status_msg = function(msg, level = 'info') {
    status_el.textContent = msg;
    status_el.className   = 'status ' + level;
  };
  // }}}

  // {{{ wire_sig
  // Stable string key for a connection record, used to prevent double-erase.
  function wire_sig(c) {
    return c.from_box + '|' + (c.from_branch || '') + '|' + c.to_box + '|' + c.to_input;
  }
  // }}}

  // {{{ set_tool_mode
  function set_tool_mode(mode) {
    tool_mode = mode;
    const era_btn = document.getElementById('tool-erase-wire');
    if (era_btn) era_btn.classList.toggle('active', mode === 'erase-wire');
    if (mode === 'erase-wire') {
      Canvas.el.style.cursor = 'cell';
      status_msg('click and drag to erase wires');
    } else {
      Canvas.el.style.cursor = '';
      status_msg('');
    }
  }
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
      fit_to_view();
      status_msg('loaded ' + ids.length + ' boxes');
    } catch(e) {
      status_msg('load error: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ fit_to_view
  // Center the camera on the bounding box of all boxes, picking a zoom
  // that fits the whole graph with a screen-pixel margin. Empty map
  // resets to origin / zoom 1.
  //
  // The new canvas viewport (001-canvas.js) uses screen-pixel pan
  // offsets, so the math here is one-to-one with what's drawn:
  //   sx = wx * zoom + pan_x
  // Solve for pan so the bounding-box center lands at the canvas center.
  function fit_to_view() {
    const ids = Object.keys(Boxes.boxes);
    if (ids.length === 0) {
      Canvas.cam.zoom = 1;
      Canvas.cam.x    = 0;
      Canvas.cam.y    = 0;
      return;
    }

    let min_x =  Infinity, min_y =  Infinity;
    let max_x = -Infinity, max_y = -Infinity;
    for (const id of ids) {
      const b = Boxes.boxes[id];
      const x = b.ui?.x ?? 0;
      const y = b.ui?.y ?? 0;
      const w = Boxes.BOX_W;
      const h = Boxes.box_height(b);
      if (x     < min_x) min_x = x;
      if (y     < min_y) min_y = y;
      if (x + w > max_x) max_x = x + w;
      if (y + h > max_y) max_y = y + h;
    }

    const margin = 60;
    const cw     = Canvas.el.width;
    const ch     = Canvas.el.height;
    const bb_w   = max_x - min_x;
    const bb_h   = max_y - min_y;

    // Pick the largest zoom that fits the bounding box with margin on
    // each side. Clamp to a comfortable range.
    let zoom = 1;
    if (bb_w > 0 && bb_h > 0) {
      const zx = (cw - 2 * margin) / bb_w;
      const zy = (ch - 2 * margin) / bb_h;
      zoom = Math.max(0.25, Math.min(1.5, Math.min(zx, zy)));
    }

    // Center the bounding box: canvas_center = bb_center * zoom + pan
    const center_wx = min_x + bb_w / 2;
    const center_wy = min_y + bb_h / 2;
    Canvas.cam.zoom = zoom;
    Canvas.cam.x    = cw / 2 - center_wx * zoom;
    Canvas.cam.y    = ch / 2 - center_wy * zoom;
  }
  // }}}

  // {{{ new_box_at
  async function new_box_at(wx, wy) {
    const id = 'box-' + Date.now().toString(36);
    const box = {
      id, label: 'New Box', kind: 'call',
      ref: '', fn: '', inputs: [], connections: [],
      ui: { x: Math.round(wx), y: Math.round(wy) },
    };
    try {
      await API.put_box(id, box);
      Boxes.boxes[id] = box;
      selected_id = id;
      Inspector.show(box, () => Canvas.mark_dirty(), () => delete_box(id));
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

    if (tool_mode === 'erase-wire') {
      erasing    = true;
      erase_seen = new Set();
      return;
    }

    // select mode: existing port / box / deselect logic
    const port = Boxes.hit_test_port(w.x, w.y);
    if (port && port.side === 'output') {
      drawing_wire = { from_box: port.box_id, from_port: port.port_name,
                       cur_x: w.x, cur_y: w.y };
      return;
    }

    const box_id = Boxes.hit_test_box(w.x, w.y);
    if (box_id) {
      selected_id = box_id;
      drag_box_id = box_id;
      const b = Boxes.boxes[box_id];
      drag_offset = { x: w.x - (b.ui?.x ?? 0), y: w.y - (b.ui?.y ?? 0) };
      Inspector.show(b, () => Canvas.mark_dirty(), () => delete_box(box_id));
      Canvas.mark_dirty();
      return;
    }

    selected_id = null;
    Inspector.hide();
    Canvas.mark_dirty();
  });

  Canvas.el.addEventListener('mousemove', e => {
    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    if (erasing && tool_mode === 'erase-wire') {
      const wire = Wires.hit_test_wire(w.x, w.y, Canvas.cam.zoom);
      if (wire) {
        const sig = wire_sig(wire);
        if (!erase_seen.has(sig)) {
          erase_seen.add(sig);
          // remove from local cache immediately for instant visual feedback
          [wire.from_box, wire.to_box].forEach(id => {
            if (Boxes.boxes[id]) {
              Boxes.boxes[id].connections = (Boxes.boxes[id].connections || []).filter(
                c => wire_sig(c) !== sig
              );
            }
          });
          Canvas.mark_dirty();
          Wires.delete_connection(wire).catch(e2 =>
            status_msg('erase error: ' + e2.message, 'error'));
        }
      }
      return;
    }

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

    if (erasing) {
      erasing    = false;
      erase_seen = new Set();
      set_tool_mode('select');
      return;
    }

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

  // {{{ delete_box
  // Strips all connection records referencing this box from other boxes first,
  // then deletes the box. No confirmation — deletion is always immediate.
  async function delete_box(id) {
    const b = Boxes.boxes[id];
    if (!b) return;
    try {
      // remove all connection records that reference this box from every other box
      const other_ids = Object.keys(Boxes.boxes).filter(bid => bid !== id);
      await Promise.all(other_ids.map(async bid => {
        const box = await API.get_box(bid);
        const before = (box.connections || []).length;
        box.connections = (box.connections || []).filter(
          c => c.from_box !== id && c.to_box !== id
        );
        if (box.connections.length !== before) {
          await API.put_box(bid, box);
          Boxes.boxes[bid] = box;
        }
      }));
      await API.delete_box(id);
      delete Boxes.boxes[id];
      Inspector.hide();
      selected_id = null;
      Canvas.mark_dirty();
      status_msg('deleted ' + id);
    } catch(e) {
      status_msg('delete error: ' + e.message, 'error');
    }
  }
  // }}}

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

    if (selected_id) await delete_box(selected_id);
  });

  // {{{ save_to_history
  function save_to_history(server_url, map_name) {
    const history = JSON.parse(localStorage.getItem('soramech_maps_history') || '[]');
    // deduplicate: remove existing entry for this server+map combo, then prepend
    const deduped = history.filter(h => !(h.server_url === server_url && h.map_name === map_name));
    deduped.unshift({ server_url, map_name });
    localStorage.setItem('soramech_maps_history', JSON.stringify(deduped.slice(0, 20)));
  }
  // }}}

  // {{{ switch_to_map
  function switch_to_map(server_url, map_name) {
    const su = server_url || localStorage.getItem('soramech_server');
    localStorage.setItem('soramech_server', su);
    localStorage.setItem('soramech_map',    map_name);
    save_to_history(su, map_name);
    API.init(su, map_name);
    document.getElementById('map-label').textContent = map_name;
    Inspector.hide();
    load_map();
  }
  // }}}

  // {{{ show_map_picker
  // Renders the map picker into the inspector sidebar. Used for both
  // first-time load (no map stored yet) and switching maps later. The
  // picker carries its own server-URL input so it works before any
  // server has been configured; nothing here reads from API state.
  function show_map_picker() {
    Inspector.show_content('maps', (container) => {
      container.innerHTML = '';

      // -- helper: collapsible group with a body element
      function mk_group(label) {
        let collapsed = false;
        const wrap = document.createElement('div');
        const hdr  = document.createElement('div');
        hdr.className = 'fb-group-hdr';
        const toggle = document.createElement('span');
        toggle.className   = 'fb-group-toggle';
        toggle.textContent = '▼';
        hdr.appendChild(toggle);
        const lbl = document.createElement('span');
        lbl.className   = 'fb-header';
        lbl.textContent = label;
        hdr.appendChild(lbl);
        const body = document.createElement('div');
        hdr.onclick = () => {
          collapsed = !collapsed;
          toggle.textContent = collapsed ? '▶' : '▼';
          body.style.display = collapsed ? 'none' : '';
        };
        wrap.appendChild(hdr);
        wrap.appendChild(body);
        return { wrap, body };
      }

      // -- helper: clickable map row
      function mk_map_row(name, subtitle, onclick) {
        const row = document.createElement('div');
        row.className   = 'fb-file-row';
        row.textContent = name;
        if (subtitle) {
          const s = document.createElement('span');
          s.style.cssText = 'font-size:9px;color:#3a3f55;margin-left:6px;font-family:monospace;';
          s.textContent   = subtitle;
          row.appendChild(s);
        }
        row.onclick = onclick;
        return row;
      }

      // -- server URL input row at the top.
      // The input is the source of truth for "which server are we
      // talking to right now". All map-row click handlers read from it.
      const stored_server = localStorage.getItem('soramech_server') || 'http://localhost:7700';

      const server_label = document.createElement('div');
      server_label.className   = 'section-label';
      server_label.textContent = 'server';
      container.appendChild(server_label);

      const server_row = document.createElement('div');
      server_row.style.cssText = 'display:flex;gap:4px;margin-bottom:10px;';
      const server_inp = document.createElement('input');
      server_inp.type        = 'text';
      server_inp.value       = stored_server;
      server_inp.placeholder = 'http://host:port';
      server_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      const connect_btn = document.createElement('button');
      connect_btn.className   = 'toolbar-btn';
      connect_btn.textContent = '→';
      server_row.appendChild(server_inp);
      server_row.appendChild(connect_btn);
      container.appendChild(server_row);

      // -- maps list section: re-renders on connect.
      const maps_section = document.createElement('div');
      container.appendChild(maps_section);

      function current_server() { return server_inp.value.trim().replace(/\/$/, ''); }

      async function refresh_maps() {
        maps_section.innerHTML = '<div class="fb-note">loading…</div>';

        const recent = JSON.parse(localStorage.getItem('soramech_maps_history') || '[]');
        const url    = current_server();

        let available = [];
        let err = null;
        if (url) {
          try {
            const r = await fetch(url + '/maps');
            if (r.ok) available = await r.json();
            else      err = 'server returned ' + r.status;
          } catch(e) {
            err = e.message || 'unreachable';
          }
        } else {
          err = 'enter a server URL';
        }

        maps_section.innerHTML = '';

        // recent group: shows every distinct (server, map) pair from
        // history. Subtitle shows the server URL when it differs from
        // the current picker URL.
        if (recent.length > 0) {
          const g = mk_group('recent');
          recent.forEach(({ server_url: su, map_name: mn }) => {
            const sub = su !== url ? su : null;
            g.body.appendChild(mk_map_row(mn, sub, () => switch_to_map(su, mn)));
          });
          maps_section.appendChild(g.wrap);
        }

        // available group / error: maps fetched from the picker URL.
        if (err) {
          const note = document.createElement('div');
          note.className   = 'fb-note error';
          note.textContent = 'cannot reach server: ' + err;
          maps_section.appendChild(note);
        } else if (available.length > 0) {
          const g = mk_group('on this server');
          available.forEach(mn => {
            g.body.appendChild(mk_map_row(mn, null, () => switch_to_map(url, mn)));
          });
          maps_section.appendChild(g.wrap);
        } else if (recent.length === 0) {
          const note = document.createElement('div');
          note.className   = 'fb-note';
          note.textContent = 'no maps on this server';
          maps_section.appendChild(note);
        }

        // open-map-by-name section.
        const sep = document.createElement('div');
        sep.className   = 'section-label';
        sep.textContent = 'open map by name';
        sep.style.marginTop = '10px';
        maps_section.appendChild(sep);

        const new_wrap = document.createElement('div');
        new_wrap.style.cssText = 'display:flex;gap:4px;';
        const new_inp = document.createElement('input');
        new_inp.type        = 'text';
        new_inp.placeholder = 'map name';
        new_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
          'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
        const new_btn = document.createElement('button');
        new_btn.className   = 'toolbar-btn';
        new_btn.textContent = 'open';
        new_btn.onclick = () => {
          const name = new_inp.value.trim();
          if (name) switch_to_map(current_server(), name);
        };
        new_inp.addEventListener('keydown', e => { if (e.key === 'Enter') new_btn.click(); });
        new_wrap.appendChild(new_inp);
        new_wrap.appendChild(new_btn);
        maps_section.appendChild(new_wrap);
      }

      connect_btn.onclick = refresh_maps;
      server_inp.addEventListener('keydown', e => {
        if (e.key === 'Enter') refresh_maps();
      });

      refresh_maps();
    });
  }
  // }}}

  // {{{ export_image
  function export_image() {
    const link = document.createElement('a');
    link.download = 'soramech-' + Date.now() + '.png';
    link.href     = Canvas.el.toDataURL('image/png');
    link.click();
  }
  // }}}

  // right-click on canvas: context menu
  Canvas.el.addEventListener('contextmenu', e => {
    e.preventDefault();
    hide_ctx_menu();

    const s = Canvas.mouse_pos(e);
    const w = Canvas.screen_to_world(s.x, s.y);

    const box_id = Boxes.hit_test_box(w.x, w.y);
    const wire   = !box_id ? Wires.hit_test_wire(w.x, w.y, Canvas.cam.zoom) : null;

    const items = [];
    if (box_id) {
      const box   = Boxes.boxes[box_id];
      const label = (box.label && box.label !== 'New Box') ? '"' + box.label + '"' : box_id;
      items.push({ label: 'delete ' + label, danger: true, action: () => delete_box(box_id) });
    } else if (wire) {
      const branch_str = wire.from_branch ? ' .' + wire.from_branch : '';
      items.push({
        label:  'delete wire ' + wire.from_box + branch_str + ' → ' + wire.to_box,
        danger: true,
        action: () => Wires.delete_connection(wire),
      });
    } else {
      items.push({ label: 'add box here', action: () => new_box_at(w.x, w.y) });
    }

    show_ctx_menu(e.clientX, e.clientY, items);
  });

  // {{{ init
  async function init() {
    const server_url = localStorage.getItem('soramech_server') || '';
    const map_name   = localStorage.getItem('soramech_map')    || '';

    // Wire the map-label click handler before either path: it stays
    // valid through the picker flow and any later switch.
    document.getElementById('map-label').onclick = show_map_picker;

    // Render loop runs regardless of whether a map is loaded yet.
    requestAnimationFrame(render);

    if (!server_url || !map_name) {
      // First-time load: no stored map. Show the picker; selection
      // there saves to localStorage and triggers load_map via
      // switch_to_map.
      document.getElementById('map-label').textContent = '';
      show_map_picker();
      return;
    }

    save_to_history(server_url, map_name);
    document.getElementById('map-label').textContent = map_name;
    API.init(server_url, map_name);
    await load_map();
  }
  // }}}

  window.addEventListener('DOMContentLoaded', init);
  return { load_map, fit_to_view, set_tool_mode, export_image };
})();
