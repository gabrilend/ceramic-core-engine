// Inspector sidebar: shows and edits the selected box's fields.
// Fires PUT to the server on every field change (autosave).
// For call boxes, ref/fn are set via the file browser; ports are read-only and derived.

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
  // Read-only list of derived port names.
  function mk_port_display(ports) {
    const wrap = document.createElement('div');
    wrap.className = 'port-display';
    if (!ports || ports.length === 0) {
      const empty = document.createElement('span');
      empty.className   = 'port-display-empty';
      empty.textContent = '(none)';
      wrap.appendChild(empty);
    } else {
      ports.forEach(p => {
        const row = document.createElement('div');
        row.className   = 'port-display-row';
        row.textContent = p.name + (p.type && p.type !== 'any' ? '  :' + p.type : '');
        wrap.appendChild(row);
      });
    }
    return wrap;
  }
  // }}}

  // {{{ mk_branch_port_list
  const OPS = ['eq','lt','gt','lte','gte','contains','matches'];
  function mk_branch_port_list(ports, on_change) {
    const wrap = document.createElement('div');
    wrap.className = 'port-list';

    const render_ports = () => {
      wrap.innerHTML = '';
      ports.forEach((p, i) => {
        const row = document.createElement('div');
        row.className = 'port-row branch-port-row';

        const name_inp = document.createElement('input');
        name_inp.type  = 'text';
        name_inp.value = p.name || '';
        name_inp.placeholder = 'port name';
        name_inp.addEventListener('input', () => { ports[i].name = name_inp.value; on_change(); save(); });
        row.appendChild(name_inp);

        if (p.name !== 'else') {
          if (!p.predicate) p.predicate = { op: 'eq', value: '' };

          const op_sel = document.createElement('select');
          OPS.forEach(op => {
            const opt = document.createElement('option');
            opt.value = op; opt.textContent = op;
            if (op === p.predicate.op) opt.selected = true;
            op_sel.appendChild(opt);
          });
          op_sel.addEventListener('change', () => { ports[i].predicate.op = op_sel.value; save(); });

          const val_inp = document.createElement('input');
          val_inp.type  = 'text';
          val_inp.value = p.predicate.value ?? '';
          val_inp.placeholder = 'value';
          val_inp.addEventListener('input', () => { ports[i].predicate.value = val_inp.value; save(); });

          row.appendChild(op_sel);
          row.appendChild(val_inp);
        }

        if (p.name !== 'else') {
          const del_btn = document.createElement('button');
          del_btn.textContent = '×';
          del_btn.className   = 'port-del';
          del_btn.addEventListener('click', () => { ports.splice(i, 1); render_ports(); on_change(); save(); });
          row.appendChild(del_btn);
        }

        wrap.appendChild(row);
      });

      const add_btn = document.createElement('button');
      add_btn.textContent = '+ port';
      add_btn.className   = 'add-port';
      add_btn.addEventListener('click', () => {
        const else_i = ports.findIndex(p => p.name === 'else');
        const new_port = { name: '', predicate: { op: 'eq', value: '' } };
        if (else_i >= 0) ports.splice(else_i, 0, new_port);
        else ports.push(new_port);
        render_ports(); on_change(); save();
      });
      wrap.appendChild(add_btn);
    };

    render_ports();
    return wrap;
  }
  // }}}

  // {{{ open_browser
  // Replaces inspector content with the file browser.
  // On function select: populates ref/fn/inputs/outputs and returns to normal view.
  function open_browser() {
    title.textContent = 'browse src/';
    fields.innerHTML  = '';
    FileBrowser.render(fields, (filename, fn) => {
      current_box.ref     = filename;
      current_box.fn      = fn.name;
      current_box.inputs  = fn.inputs.map(n  => ({ name: n, type: 'any' }));
      current_box.outputs = fn.outputs.map(n => ({ name: n, type: 'any' }));
      save().then(() => show(current_box, on_change_cb));
    });
  }
  // }}}

  // {{{ show
  function show(box, on_changed, on_delete) {
    current_box  = box;
    on_change_cb = on_changed;
    on_delete_cb = on_delete || null;
    panel.classList.remove('hidden');
    title.textContent = box.id;
    fields.innerHTML  = '';

    // delete button — always visible at the top of the inspector
    if (on_delete_cb) {
      const del_btn = document.createElement('button');
      del_btn.textContent = 'delete box';
      del_btn.className   = 'delete-box-btn';
      del_btn.onclick     = () => on_delete_cb();
      fields.appendChild(del_btn);
    }

    fields.appendChild(mk_row('label',
      mk_text_input(box.label, v => { current_box.label = v; })));

    fields.appendChild(mk_row('kind', (() => {
      const sel = document.createElement('select');
      ['call','branch','data'].forEach(k => {
        const opt = document.createElement('option');
        opt.value = k; opt.textContent = k;
        if (k === box.kind) opt.selected = true;
        sel.appendChild(opt);
      });
      sel.addEventListener('change', () => {
        current_box.kind = sel.value;
        show(current_box, on_change_cb);
        save();
      });
      return sel;
    })()));

    if (box.kind === 'call' || box.kind === 'data') {
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

      const in_sec = document.createElement('div');
      in_sec.className   = 'section-label';
      in_sec.textContent = 'inputs';
      fields.appendChild(in_sec);
      fields.appendChild(mk_port_display(box.inputs));

      const out_sec = document.createElement('div');
      out_sec.className   = 'section-label';
      out_sec.textContent = 'outputs';
      fields.appendChild(out_sec);
      fields.appendChild(mk_port_display(box.outputs));
    }

    if (box.kind === 'branch') {
      if (!current_box.ports) current_box.ports = [{ name: 'else' }];
      fields.appendChild(mk_row('retry limit',
        mk_text_input(String(box.retry_limit ?? 3),
          v => { current_box.retry_limit = parseInt(v) || 3; })));

      // branch inputs are manually configured (no source file to parse)
      const in_sec = document.createElement('div');
      in_sec.className   = 'section-label';
      in_sec.textContent = 'inputs';
      fields.appendChild(in_sec);
      if (!current_box.inputs) current_box.inputs = [];
      const in_list = document.createElement('div');
      in_list.className = 'port-list';
      (current_box.inputs || []).forEach((p, i) => {
        const row = document.createElement('div');
        row.className = 'port-row';
        const ni = document.createElement('input');
        ni.type = 'text'; ni.value = p.name || ''; ni.placeholder = 'name';
        ni.addEventListener('input', () => { current_box.inputs[i].name = ni.value; save(); });
        const ti = document.createElement('input');
        ti.type = 'text'; ti.value = p.type || ''; ti.placeholder = 'type';
        ti.addEventListener('input', () => { current_box.inputs[i].type = ti.value; save(); });
        const del = document.createElement('button');
        del.textContent = '×'; del.className = 'port-del';
        del.addEventListener('click', () => {
          current_box.inputs.splice(i, 1);
          show(current_box, on_change_cb);
          save();
        });
        row.appendChild(ni); row.appendChild(ti); row.appendChild(del);
        in_list.appendChild(row);
      });
      const add_in = document.createElement('button');
      add_in.textContent = '+ input'; add_in.className = 'add-port';
      add_in.addEventListener('click', () => {
        current_box.inputs.push({ name: '', type: 'any' });
        show(current_box, on_change_cb);
        save();
      });
      in_list.appendChild(add_in);
      fields.appendChild(in_list);

      const ports_sec = document.createElement('div');
      ports_sec.className   = 'section-label';
      ports_sec.textContent = 'ports (evaluated top-to-bottom)';
      fields.appendChild(ports_sec);
      fields.appendChild(mk_branch_port_list(current_box.ports, () => {}));
    }
  }
  // }}}

  return { show, hide };
})();
