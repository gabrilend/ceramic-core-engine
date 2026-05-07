// Inspector sidebar: shows and edits the selected box's fields.
// Fires PUT to the server on every field change (autosave).

const Inspector = (() => {
  const panel  = document.getElementById('inspector');
  const title  = document.getElementById('inspector-title');
  const fields = document.getElementById('inspector-fields');

  let current_box = null;
  let on_change_cb = null;

  // {{{ hide
  function hide() {
    panel.classList.add('hidden');
    current_box = null;
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

  // {{{ mk_port_list
  function mk_port_list(ports, on_change) {
    const wrap = document.createElement('div');
    wrap.className = 'port-list';

    const render_ports = () => {
      wrap.innerHTML = '';
      ports.forEach((p, i) => {
        const row = document.createElement('div');
        row.className = 'port-row';

        const name_inp = document.createElement('input');
        name_inp.type  = 'text';
        name_inp.value = p.name || '';
        name_inp.placeholder = 'name';
        name_inp.addEventListener('input', () => { ports[i].name = name_inp.value; on_change(); save(); });

        const type_inp = document.createElement('input');
        type_inp.type  = 'text';
        type_inp.value = p.type || '';
        type_inp.placeholder = 'type';
        type_inp.addEventListener('input', () => { ports[i].type = type_inp.value; on_change(); save(); });

        const del_btn = document.createElement('button');
        del_btn.textContent = '×';
        del_btn.className   = 'port-del';
        del_btn.addEventListener('click', () => { ports.splice(i, 1); render_ports(); on_change(); save(); });

        row.appendChild(name_inp);
        row.appendChild(type_inp);
        row.appendChild(del_btn);
        wrap.appendChild(row);
      });

      const add_btn = document.createElement('button');
      add_btn.textContent = '+ port';
      add_btn.className   = 'add-port';
      add_btn.addEventListener('click', () => {
        ports.push({ name: '', type: 'string' });
        render_ports();
        on_change();
        save();
      });
      wrap.appendChild(add_btn);
    };

    render_ports();
    return wrap;
  }
  // }}}

  // {{{ mk_branch_port_list
  function mk_branch_port_list(ports, on_change) {
    const OPS = ['eq','lt','gt','lte','gte','contains','matches'];
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

        // can't delete the else port
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
        // insert before else
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

  // {{{ show
  function show(box, on_changed) {
    current_box  = box;
    on_change_cb = on_changed;
    panel.classList.remove('hidden');
    title.textContent = box.id;
    fields.innerHTML  = '';

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
      sel.addEventListener('change', () => { current_box.kind = sel.value; show(current_box, on_change_cb); save(); });
      return sel;
    })()));

    if (box.kind === 'call' || box.kind === 'data') {
      fields.appendChild(mk_row('ref',
        mk_text_input(box.ref, v => { current_box.ref = v; })));
      fields.appendChild(mk_row('fn',
        mk_text_input(box.fn, v => { current_box.fn = v; })));
    }

    const inputs_sec = document.createElement('div');
    inputs_sec.className = 'section-label';
    inputs_sec.textContent = 'inputs';
    fields.appendChild(inputs_sec);
    if (!current_box.inputs) current_box.inputs = [];
    fields.appendChild(mk_port_list(current_box.inputs, () => {}));

    if (box.kind === 'branch') {
      if (!current_box.ports) current_box.ports = [{ name: 'else' }];
      fields.appendChild(mk_row('retry limit',
        mk_text_input(String(box.retry_limit ?? 3),
          v => { current_box.retry_limit = parseInt(v) || 3; })));
      const ports_sec = document.createElement('div');
      ports_sec.className = 'section-label';
      ports_sec.textContent = 'ports (evaluated top-to-bottom)';
      fields.appendChild(ports_sec);
      fields.appendChild(mk_branch_port_list(current_box.ports, () => {}));
    } else {
      if (!current_box.outputs) current_box.outputs = [];
      const out_sec = document.createElement('div');
      out_sec.className = 'section-label';
      out_sec.textContent = 'outputs';
      fields.appendChild(out_sec);
      fields.appendChild(mk_port_list(current_box.outputs, () => {}));
    }
  }
  // }}}

  return { show, hide };
})();
