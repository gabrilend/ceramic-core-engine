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

  // {{{ Variadic helpers (issue 217 part B)
  // Variadic input naming convention:
  //   - non-variadic input is just `text`
  //   - variadic group has slots `text_0`, `text_1`, ..., `text_N`
  //   - the base name `text` is listed in box.variadic_inputs
  //
  // These helpers parse and query that convention without mutating
  // anything; the toggle / add / remove operations live below.

  function parse_variadic_name(name) {
    const m = /^(.+)_(\d+)$/.exec(name || '');
    return m ? { base: m[1], index: parseInt(m[2], 10) } : null;
  }

  function is_variadic_slot(box, port_name) {
    const p = parse_variadic_name(port_name);
    if (!p) return false;
    return (box.variadic_inputs || []).includes(p.base);
  }

  function variadic_slots_for(box, base) {
    const out = [];
    (box.inputs || []).forEach((p, i) => {
      const pv = parse_variadic_name(p.name);
      if (pv && pv.base === base) out.push({ port: p, idx: i, n: pv.index });
    });
    out.sort((a, b) => a.n - b.n);
    return out;
  }

  function last_variadic_index(box, base) {
    const slots = variadic_slots_for(box, base);
    return slots.length === 0 ? -1 : slots[slots.length - 1].n;
  }

  // Bilateral connection update: walk current_box.connections and the
  // affected upstream boxes, applying `mut` to each connection record
  // that has `to_box === current_box.id` and `to_input` matching one
  // of `target_names`. mut returns either an updated record or null
  // (delete). Persists changes to the server for every box touched.
  async function update_target_connections(target_names, mut) {
    if (!current_box) return;
    const my_id = current_box.id;

    // Collect upstream box ids whose connection records may need to change.
    const upstream_ids = new Set();
    (current_box.connections || []).forEach(c => {
      if (c.to_box === my_id && target_names.includes(c.to_input)) {
        upstream_ids.add(c.from_box);
      }
    });

    // Update this box's connections in-place.
    current_box.connections = (current_box.connections || []).flatMap(c => {
      if (c.to_box === my_id && target_names.includes(c.to_input)) {
        const updated = mut(c);
        return updated ? [updated] : [];
      }
      return [c];
    });

    // Update each upstream box. Mutates the cached object in place;
    // never replaces the cache reference, so anyone else holding a
    // reference (including this inspector if it later switches focus)
    // sees the updates immediately.
    await Promise.all(Array.from(upstream_ids).map(async up_id => {
      const up = Boxes.boxes[up_id];
      if (!up) return;
      const before = up.connections || [];
      up.connections = before.flatMap(c => {
        if (c.to_box === my_id && target_names.includes(c.to_input)) {
          const updated = mut(c);
          return updated ? [updated] : [];
        }
        return [c];
      });
      try {
        await API.put_box(up_id, up);
      } catch (e) {
        up.connections = before;   // roll back on PUT failure
        console.error('failed to update upstream ' + up_id + ': ' + e.message);
      }
    }));
  }
  // }}}

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

  // {{{ Variadic operations (issue 217 part B)
  // Each operation mutates current_box, persists changes to all affected
  // boxes (this one and any upstream boxes whose connection records
  // referenced the renamed/removed slots), then re-renders the inspector.

  // Toggle a non-variadic input ON: snip every wire targeting the
  // port, rename the port `text` → `text_0`, add base to
  // box.variadic_inputs.
  //
  // Snipping (rather than renaming the wire to point at `text_0`)
  // matches the user's mental model: variadic-on and variadic-off
  // are different input shapes; switching shapes clears the wires.
  // The user reconnects after the toggle. This avoids a class of
  // edge cases where rename + wire-create + auto-grow have to
  // coordinate.
  //
  // Special case: a port literally named `...` (Lua variadic that
  // the file-browser parser left in place) gets renamed to `args_0`
  // with base `args` — there's no other sensible name we can derive.
  async function make_variadic(port_name) {
    const idx = current_box.inputs.findIndex(p => p.name === port_name);
    if (idx < 0) return;

    const base     = port_name === '...' ? 'args' : port_name;
    const new_name = base + '_0';

    // 1. Drop any wires currently targeting this port.
    await update_target_connections([port_name], () => null);

    // 2. Rename the port and add base to variadic_inputs.
    current_box.inputs[idx].name = new_name;
    current_box.variadic_inputs  = current_box.variadic_inputs || [];
    if (!current_box.variadic_inputs.includes(base)) {
      current_box.variadic_inputs.push(base);
    }

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Toggle a variadic group OFF (collapse to single port `<base>`).
  // Snips every wire targeting any slot in the group, drops higher
  // slots from box.inputs, renames slot 0 back to base, removes the
  // base from box.variadic_inputs.
  //
  // Snipping (rather than keeping slot-0's wire) matches the same
  // "shape change clears wires" rule as make_variadic. Symmetric.
  async function unmake_variadic(base) {
    const slots = variadic_slots_for(current_box, base);
    if (slots.length === 0) return;

    // Capture all slot names BEFORE mutating port objects.
    const all_names = slots.map(s => s.port.name);

    // 1. Drop every wire targeting any slot in the group.
    await update_target_connections(all_names, () => null);

    // 2. Drop higher slots from box.inputs and rename slot 0 to base.
    const slot_0_old_name = slots[0].port.name;
    current_box.inputs = current_box.inputs.filter((_, i) =>
      !slots.slice(1).some(s => s.idx === i));
    const new_keep_idx = current_box.inputs.findIndex(p => p.name === slot_0_old_name);
    if (new_keep_idx >= 0) current_box.inputs[new_keep_idx].name = base;

    // 3. Remove base from variadic_inputs.
    current_box.variadic_inputs = (current_box.variadic_inputs || [])
      .filter(n => n !== base);

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Append `<base>_<last+1>` to box.inputs. Idempotent if no slot grows
  // would be needed (caller checks). Used by the wire-connect and
  // value-set auto-grow paths and by sibling code in the wires module.
  async function add_variadic_slot(base) {
    const next = last_variadic_index(current_box, base) + 1;
    current_box.inputs.push({ name: base + '_' + next, type: 'any' });
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Remove a single variadic slot at `slot_name`. Higher slots in the
  // same group rename down by one. Connections targeting the removed
  // slot are dropped; connections targeting renamed slots get their
  // to_input rewritten on both endpoints.
  async function remove_variadic_slot(slot_name) {
    const pv = parse_variadic_name(slot_name);
    if (!pv) return;

    // The first slot is permanent — un-variadic instead if you want
    // to remove it entirely.
    if (pv.index === 0) {
      status_msg('cannot remove slot 0 — toggle variadic off instead', 'error');
      return;
    }

    // Drop the slot.
    current_box.inputs = current_box.inputs.filter(p => p.name !== slot_name);

    // Rename higher slots down by one.
    const renames = [];  // [{ old: 'text_3', new: 'text_2' }, ...]
    current_box.inputs.forEach((p, i) => {
      const ppv = parse_variadic_name(p.name);
      if (ppv && ppv.base === pv.base && ppv.index > pv.index) {
        const new_name = pv.base + '_' + (ppv.index - 1);
        renames.push({ old: p.name, new: new_name });
        current_box.inputs[i].name = new_name;
      }
    });

    // Drop connections to the removed slot. Renames go in ascending
    // order so we never collide with an existing slot's name (we just
    // freed it up by the previous rename).
    await update_target_connections([slot_name], () => null);
    renames.sort((a, b) => parse_variadic_name(a.old).index - parse_variadic_name(b.old).index);
    for (const r of renames) {
      await update_target_connections([r.old], c => ({ ...c, to_input: r.new }));
    }

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Sever every outgoing wire from this box. Used when toggling the
  // comparator on/off — wires from null-output and lt/eq/gt outputs
  // can't all be valid at once, so we wipe the slate on either toggle
  // direction. Also clears any selected wire if it pointed at one of
  // the removed connections.
  async function sever_output_wires() {
    if (!current_box) return;
    const my_id = current_box.id;
    const out   = (current_box.connections || []).filter(c => c.from_box === my_id);
    if (out.length === 0) return;

    // Collect destinations whose connection records also need cleaning.
    const dest_ids = new Set();
    out.forEach(c => dest_ids.add(c.to_box));

    current_box.connections = (current_box.connections || [])
      .filter(c => c.from_box !== my_id);

    await Promise.all(Array.from(dest_ids).map(async dst_id => {
      const dst = Boxes.boxes[dst_id];
      if (!dst) return;
      const before = dst.connections || [];
      dst.connections = before.filter(c => c.from_box !== my_id);
      try {
        await API.put_box(dst_id, dst);
      } catch (e) {
        dst.connections = before;
        console.error('failed to sever wires to ' + dst_id + ': ' + e.message);
      }
    }));

    status_msg('cleared ' + out.length + ' outgoing wire(s)');
  }

  // Auto-grow check used by the wire-connect path and the value-set
  // path. If `slot_name` is the LAST slot of a variadic group on
  // `box`, append a new empty slot and persist. Idempotent — calling
  // again on the same slot is a no-op because it's no longer last.
  // Exported via the Inspector return so other modules (Wires) can call
  // it after creating a connection.
  async function auto_grow_after_set(box, slot_name) {
    const pv = parse_variadic_name(slot_name);
    if (!pv) return;
    if (!(box.variadic_inputs || []).includes(pv.base)) return;
    if (pv.index !== last_variadic_index(box, pv.base)) return;

    box.inputs = box.inputs || [];
    box.inputs.push({ name: pv.base + '_' + (pv.index + 1), type: 'any' });
    try {
      await API.put_box(box.id, box);
      Canvas.mark_dirty();
      // Mutating in place means box === Boxes.boxes[id] === current_box
      // (when the inspector is showing this box), so re-rendering with
      // the same reference works.
      if (current_box && current_box.id === box.id) {
        show(current_box, on_change_cb, on_delete_cb);
      }
    } catch (e) {
      // roll back
      box.inputs.pop();
      status_msg('auto-grow failed: ' + e.message, 'error');
    }
  }
  // }}}

  // {{{ mk_port_display
  // Renders input ports as read-only name labels with an optional literal value field.
  // Names are derived from source parsing and are not editable here.
  // Variadic groups (issue 217 part B) get extra controls: a "var" toggle
  // on the first slot, "+" to add a slot, and "×" to remove non-first slots.
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

        // Variadic context for this port (issue 217 part B):
        //   in_var_group = is part of a variadic group (one of multiple slots)
        //   pv           = { base, index } parsed from the slot name
        //   group_last   = the last index in the group (so we know whether
        //                  to render the "+" button on this row)
        const pv           = current_box ? parse_variadic_name(p.name) : null;
        const in_var_group = current_box && is_variadic_slot(current_box, p.name);
        const group_last   = in_var_group ? last_variadic_index(current_box, pv.base) : -1;

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
              // remove from each source box (mutate cached object in place)
              await Promise.all(to_break.map(async conn => {
                const src = Boxes.boxes[conn.from_box];
                if (!src) return;
                const before = src.connections || [];
                src.connections = before.filter(c =>
                  !(c.from_box === conn.from_box &&
                    (c.from_branch ?? null) === (conn.from_branch ?? null) &&
                    c.to_box    === conn.to_box &&
                    c.to_input  === conn.to_input)
                );
                try {
                  await API.put_box(conn.from_box, src);
                } catch (e2) {
                  src.connections = before;
                  console.error('failed to sever wire from ' + conn.from_box + ':', e2.message);
                }
              }));
              Canvas.mark_dirty();
              status_msg('severed ' + to_break.length + ' wire(s) into port "' + port_name + '"');
            }
          }

          // Auto-grow the variadic group when a value lands on its last
          // slot. Idempotent — once the group has grown, this slot is
          // no longer the last and the call is a no-op.
          if (new_val !== undefined && current_box && in_var_group &&
              pv.index === last_variadic_index(current_box, pv.base)) {
            await auto_grow_after_set(current_box, p.name);
          }

          save();
        });
        row.appendChild(val_inp);

        // Variadic control buttons. Two states a port row can be in:
        //   1. Plain port (not variadic): show a "var" toggle that turns it on.
        //   2. First slot of a variadic group (index 0): show "var ×"
        //      to collapse the group back to a single port.
        //   3. Non-first slot in a variadic group: show "×" to remove it.
        //
        // No "+" button — slots auto-grow when wired or when a value is
        // set on the last slot.
        const btn_style = 'background:none;border:1px solid #2a2f45;border-radius:3px;' +
          'color:#6c72a0;cursor:pointer;font-family:monospace;font-size:9px;' +
          'padding:1px 5px;line-height:1.4;';

        if (!in_var_group) {
          // Plain port — offer the variadic toggle.
          const var_btn = document.createElement('button');
          var_btn.style.cssText = btn_style;
          var_btn.textContent   = 'var';
          var_btn.title         = 'mark this input as variadic (grows to N slots)';
          var_btn.onclick       = () => make_variadic(p.name);
          row.appendChild(var_btn);
        } else if (pv.index === 0) {
          // First slot of a variadic group: collapse-back toggle.
          const var_btn = document.createElement('button');
          var_btn.style.cssText = btn_style + 'border-color:#4a9eff;color:#4a9eff;';
          var_btn.textContent   = 'var ×';
          var_btn.title         = 'collapse back to a single non-variadic input';
          var_btn.onclick       = () => unmake_variadic(pv.base);
          row.appendChild(var_btn);
        } else {
          // Non-first variadic slot: remove this slot.
          const x_btn = document.createElement('button');
          x_btn.style.cssText = btn_style;
          x_btn.textContent   = '×';
          x_btn.title         = 'remove this slot';
          x_btn.onclick       = () => remove_variadic_slot(p.name);
          row.appendChild(x_btn);
        }

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
    // view button hides until a ref is set — clicking it before then
    // would only produce a "no ref" error. Toggle on every input event so
    // the visibility tracks the field live (issue 227).
    ref_inp.addEventListener('input', () => {
      current_box.ref = ref_inp.value;
      view_btn.hidden = !ref_inp.value;
      save();
    });
    const browse_btn = document.createElement('button');
    browse_btn.className   = 'toolbar-btn';
    browse_btn.textContent = 'browse';
    browse_btn.style.whiteSpace = 'nowrap';
    browse_btn.onclick = open_browser;
    // "view" button opens a floating, draggable read-only window with
    // the current ref's source content (issue 215). Disabled when ref
    // is empty since there's nothing to fetch.
    const view_btn = document.createElement('button');
    view_btn.className   = 'toolbar-btn';
    view_btn.textContent = 'view';
    view_btn.style.whiteSpace = 'nowrap';
    view_btn.hidden = !box.ref;
    view_btn.onclick = () => {
      if (!current_box || !current_box.ref) {
        status_msg('view source: no ref set', 'error');
        return;
      }
      SourceView.open_source_view(current_box.ref, current_box);
    };
    ref_wrap.appendChild(ref_inp);
    ref_wrap.appendChild(browse_btn);
    ref_wrap.appendChild(view_btn);
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
    cmp_btn.onclick = async () => {
      // Toggling the comparator changes the set of valid output ports
      // (single null wire ↔ three lt/eq/gt wires). Wires from the old
      // configuration would re-appear if we left them in the
      // connections array, so clear all outgoing wires on either
      // toggle direction.
      await sever_output_wires();
      if (current_box.comparand !== undefined && current_box.comparand !== '') {
        delete current_box.comparand;
      } else {
        current_box.comparand = '0';
      }
      await save();
      show(current_box, on_change_cb, on_delete_cb);
      Canvas.mark_dirty();
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

  return { show, hide, show_content, auto_grow_after_set };
})();
