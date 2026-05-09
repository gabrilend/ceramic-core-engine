// Inspector sidebar: shows and edits the selected box's fields.
// Fires PUT to the server on every field change (autosave).
// For call boxes, ref/fn are set via the file browser; inputs are read-only, derived from parsing.

const Inspector = (() => {
  const panel  = document.getElementById('inspector');
  const title  = document.getElementById('inspector-title');
  const fields = document.getElementById('inspector-fields');

  let current_box  = null;
  let on_change_cb = null;
  let on_delete_cb = null;

  // {{{ hide
  function hide() {
    panel.classList.add('hidden');
    current_box  = null;
    on_delete_cb = null;
  }
  // }}}

  // {{{ save
  async function save() {
    if (!current_box) return;
    try {
      await API.put_box(current_box.id, current_box);
      if (on_change_cb) on_change_cb(current_box);
    } catch(e) {
      console.error('autosave failed:', e.message);
      status_msg('save error: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ mk_row
  function mk_row(label_text, input_el) {
    const row = document.createElement('div');
    row.className = 'field-row';
    const lbl = document.createElement('label');
    lbl.textContent = label_text;
    row.appendChild(lbl);
    row.appendChild(input_el);
    return row;
  }
  // }}}

  // {{{ mk_text_input
  function mk_text_input(value, on_input) {
    const inp = document.createElement('input');
    inp.type  = 'text';
    inp.value = value || '';
    inp.addEventListener('input', () => { on_input(inp.value); save(); });
    return inp;
  }
  // }}}

  // {{{ mk_readonly
  function mk_readonly(value) {
    const el = document.createElement('div');
    el.className   = 'field-readonly';
    el.textContent = value || '—';
    return el;
  }
  // }}}

  // {{{ mk_port_display
  // Renders input ports as read-only name labels with an optional literal value field.
  // Names are derived from source parsing and are not editable here.
  function mk_port_display(ports) {
    const wrap = document.createElement('div');
    wrap.className = 'port-display';
    if (!ports || ports.length === 0) {
      const empty = document.createElement('span');
      empty.className   = 'port-display-empty';
      empty.textContent = '(none)';
      wrap.appendChild(empty);
    } else {
      ports.forEach((p, i) => {
        const row = document.createElement('div');
        row.className = 'port-display-row';

        // name is read-only — derived from function signature via file browser
        const name_el = document.createElement('span');
        name_el.className   = 'port-name-inp';
        name_el.textContent = p.name || '';
        name_el.style.color = '#9ea3c0';
        row.appendChild(name_el);

        if (p.type && p.type !== 'any') {
          const type_el = document.createElement('span');
          type_el.className   = 'port-type';
          type_el.textContent = ':' + p.type;
          row.appendChild(type_el);
        }

        // literal value input — empty means "no literal" (wire will supply the value)
        const val_inp = document.createElement('input');
        val_inp.type        = 'text';
        val_inp.value       = p.value !== undefined ? String(p.value) : '';
        val_inp.placeholder = 'value…';
        val_inp.className   = 'port-val-inp';
        val_inp.addEventListener('input', async () => {
          const new_val = val_inp.value === '' ? undefined : val_inp.value;
          ports[i].value = new_val;

          // a literal value and an incoming wire are contradictory — sever any wires
          // feeding this port so only one source of truth exists
          if (new_val !== undefined && current_box) {
            const port_name = ports[i].name;
            const box_id    = current_box.id;
            const to_break  = (current_box.connections || []).filter(
              c => c.to_box === box_id && c.to_input === port_name
            );
            if (to_break.length > 0) {
              // remove from local box first so save() persists the clean state
              current_box.connections = current_box.connections.filter(
                c => !(c.to_box === box_id && c.to_input === port_name)
              );
              // remove from each source box on the server
              await Promise.all(to_break.map(async conn => {
                try {
                  const src = await API.get_box(conn.from_box);
                  src.connections = (src.connections || []).filter(c =>
                    !(c.from_box === conn.from_box &&
                      (c.from_branch ?? null) === (conn.from_branch ?? null) &&
                      c.to_box    === conn.to_box &&
                      c.to_input  === conn.to_input)
                  );
                  await API.put_box(conn.from_box, src);
                  Boxes.boxes[conn.from_box] = src;
                } catch(e2) {
                  console.error('failed to sever wire from ' + conn.from_box + ':', e2.message);
                }
              }));
              Canvas.mark_dirty();
              status_msg('severed ' + to_break.length + ' wire(s) into port "' + port_name + '"');
            }
          }

          save();
        });
        row.appendChild(val_inp);

        wrap.appendChild(row);
      });
    }
    return wrap;
  }
  // }}}

  // {{{ open_browser
  // Replaces inspector content with the file browser.
  // On function select: sets ref/fn/inputs from parsed signature (no outputs — single wire).
  function open_browser() {
    title.textContent = 'browse src/';
    fields.innerHTML  = '';
    FileBrowser.render(fields, (filename, fn) => {
      current_box.ref    = filename;
      current_box.fn     = fn.name;
      current_box.inputs = fn.inputs.map(n => ({ name: n, type: 'any' }));
      // outputs are not set — all boxes have exactly one output wire
      save().then(() => show(current_box, on_change_cb, on_delete_cb));
    });
  }
  // }}}

  // {{{ show_content
  // Renders arbitrary content into the inspector panel without a box context.
  // Used by the map picker and any other non-box inspector views.
  function show_content(title_text, render_fn) {
    current_box  = null;
    on_change_cb = null;
    on_delete_cb = null;
    panel.classList.remove('hidden');
    title.innerHTML   = '';
    title.textContent = title_text;
    fields.innerHTML  = '';
    render_fn(fields);
  }
  // }}}

  // {{{ show
  function show(box, on_changed, on_delete) {
    current_box  = box;
    on_change_cb = on_changed;
    on_delete_cb = on_delete || null;
    panel.classList.remove('hidden');

    // title: editable label input + read-only id subtitle
    title.innerHTML = '';
    const lbl_inp = document.createElement('input');
    lbl_inp.type      = 'text';
    lbl_inp.value     = box.label || '';
    lbl_inp.className = 'inspector-title-inp';
    lbl_inp.addEventListener('input', () => { current_box.label = lbl_inp.value; save(); });
    title.appendChild(lbl_inp);
    const id_sub = document.createElement('div');
    id_sub.className   = 'inspector-id-sub';
    id_sub.textContent = box.id;
    title.appendChild(id_sub);

    fields.innerHTML = '';

    // delete button — always visible at the top of the inspector
    if (on_delete_cb) {
      const del_btn = document.createElement('button');
      del_btn.textContent = 'delete box';
      del_btn.className   = 'delete-box-btn';
      del_btn.onclick     = () => on_delete_cb();
      fields.appendChild(del_btn);
    }

    // label is now the editable title — no separate field needed

    fields.appendChild(mk_row('kind', (() => {
      const sel = document.createElement('select');
      ['call', 'data'].forEach(k => {
        const opt = document.createElement('option');
        opt.value = k; opt.textContent = k;
        if (k === box.kind) opt.selected = true;
        sel.appendChild(opt);
      });
      sel.addEventListener('change', () => {
        current_box.kind = sel.value;
        show(current_box, on_change_cb, on_delete_cb);
        save();
      });
      return sel;
    })()));

    // ref: editable text input + browse button to populate via file browser
    const ref_wrap = document.createElement('div');
    ref_wrap.style.cssText = 'display:flex;gap:4px;align-items:center;';
    const ref_inp = document.createElement('input');
    ref_inp.type  = 'text';
    ref_inp.value = box.ref || '';
    ref_inp.style.flex = '1';
    ref_inp.style.minWidth = '0';
    ref_inp.addEventListener('input', () => { current_box.ref = ref_inp.value; save(); });
    const browse_btn = document.createElement('button');
    browse_btn.className   = 'toolbar-btn';
    browse_btn.textContent = 'browse';
    browse_btn.style.whiteSpace = 'nowrap';
    browse_btn.onclick = open_browser;
    ref_wrap.appendChild(ref_inp);
    ref_wrap.appendChild(browse_btn);
    fields.appendChild(mk_row('ref', ref_wrap));

    fields.appendChild(mk_row('fn', mk_readonly(box.fn)));

    // inputs: read-only names from parsing, editable literal values
    const in_sec = document.createElement('div');
    in_sec.className   = 'section-label';
    in_sec.textContent = 'inputs';
    fields.appendChild(in_sec);
    fields.appendChild(mk_port_display(box.inputs));

    // output: single wire; optional comparator splits into lt/eq/gt
    const out_sec = document.createElement('div');
    out_sec.className   = 'section-label';
    out_sec.textContent = 'output';
    fields.appendChild(out_sec);

    const cmp_row = document.createElement('div');
    cmp_row.style.cssText = 'display:flex;gap:6px;align-items:center;margin-bottom:6px;';

    const has_cmp = box.comparand !== undefined && box.comparand !== '';
    const cmp_btn = document.createElement('button');
    cmp_btn.className   = 'toolbar-btn';
    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
    cmp_btn.onclick = () => {
      if (current_box.comparand !== undefined && current_box.comparand !== '') {
        delete current_box.comparand;
      } else {
        current_box.comparand = '0';
      }
      save().then(() => show(current_box, on_change_cb, on_delete_cb));
    };
    cmp_row.appendChild(cmp_btn);

    if (has_cmp) {
      const cmp_inp = document.createElement('input');
      cmp_inp.type        = 'text';
      cmp_inp.value       = box.comparand || '';
      cmp_inp.placeholder = 'number';
      cmp_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
      cmp_row.appendChild(cmp_inp);
    }

    fields.appendChild(cmp_row);
  }
  // }}}

  return { show, hide, show_content };
})();
