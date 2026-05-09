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

    // 1. Sever every incoming wire to the box. Conservative — earlier
    // versions only severed wires to slots that still matched
    // `<base>_<N>`, but if a slot was renamed away from that pattern
    // (legacy from when the canvas overlay let users rename freely)
    // its wire would dangle. The "shape change clears wires" rule
    // applies to the whole box, not just to the still-pattern-shaped
    // slots, so we drop all of them.
    await sever_incoming_wires();

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

  // Remove a single variadic slot at `slot_name`. Other slots keep
  // their existing names — we used to compact `text_2`, `text_3`,
  // ... down to `text_1`, `text_2`, ... after removing `text_1`,
  // but that clobbered any user-renamed downstream slot's wiring
  // and produced surprising sidebar updates. The variadic helpers
  // tolerate gaps in the index sequence (auto-grow uses the highest
  // existing index + 1), so leaving holes is harmless.
  async function remove_variadic_slot(slot_name) {
    const pv = parse_variadic_name(slot_name);
    if (!pv) return;

    // The first slot is permanent — un-variadic instead if you want
    // to remove it entirely.
    if (pv.index === 0) {
      status_msg('cannot remove slot 0 — toggle variadic off instead', 'error');
      return;
    }

    current_box.inputs = current_box.inputs.filter(p => p.name !== slot_name);
    await update_target_connections([slot_name], () => null);

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // {{{ Iterator helpers (issue 221)
  // Iterator boxes are pure routing primitives: input copies straight
  // to one output, the dispatch layer (phase 3, issue 304) advances a
  // counter mod #iterator_outputs each call. No ref/fn — the box body
  // is just the counter + slot list. Each slot has a user-renamable
  // name; the slot name lands in a connection's `from_branch` field
  // exactly like a comparator branch name.
  //
  // The inspector edits the slot list; the dispatch layer is what
  // actually rotates between them at runtime.

  function is_iterator(box) {
    return Array.isArray(box && box.iterator_outputs);
  }

  // Toggle the box ON as an iterator. Ref/fn/comparand are mutually
  // exclusive with iterator routing (a routing primitive has no
  // function to invoke), so they are stripped. All outgoing wires are
  // dropped because the output shape changes from a single dot (or
  // lt/eq/gt) to N named slots — same shape-change-clears-wires rule
  // as the variadic input toggle.
  async function make_iterator() {
    if (!current_box) return;

    await sever_output_wires();

    delete current_box.ref;
    delete current_box.fn;
    delete current_box.comparand;
    delete current_box.has_output;
    current_box.iterator_outputs = ['output_0'];

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Toggle iterator OFF. Drop the iterator_outputs field; sever
  // outgoing wires (their from_branch values referenced slot names
  // that no longer exist). The user re-picks a function via the
  // browse button to restore ref/fn.
  async function unmake_iterator() {
    if (!current_box) return;

    await sever_output_wires();
    delete current_box.iterator_outputs;
    // Restore an empty ref/fn so the schema (which requires ref on
    // non-iterator call boxes) accepts the save. The user picks a
    // function via the browse button to populate them.
    if (current_box.ref === undefined) current_box.ref = '';
    if (current_box.fn  === undefined) current_box.fn  = '';

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Append a new slot with a placeholder name (`output_N`). The user
  // can rename it after — placeholder is just to avoid prompting on
  // every grow. Mirrors the variadic auto-grow pattern.
  async function add_iterator_slot() {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    let n = slots.length;
    // pick the lowest free `output_N` so renaming + adding doesn't
    // collide with an existing user-named slot
    while (slots.includes('output_' + n)) n++;
    slots.push('output_' + n);
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Remove the slot at index `idx`. Outgoing wires whose from_branch
  // matched the removed slot get severed; remaining slots keep their
  // names (no rename — slot names are user-defined and free-form, no
  // need to compact like the variadic numeric indexes).
  async function remove_iterator_slot(idx) {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    if (idx < 0 || idx >= slots.length) return;
    if (slots.length <= 1) {
      status_msg('cannot remove last slot — toggle iterator off instead', 'error');
      return;
    }
    const removed = slots[idx];

    // Sever every outgoing wire on this branch.
    const my_id = current_box.id;
    const dst_ids = new Set();
    (current_box.connections || []).forEach(c => {
      if (c.from_box === my_id && c.from_branch === removed) dst_ids.add(c.to_box);
    });
    current_box.connections = (current_box.connections || [])
      .filter(c => !(c.from_box === my_id && c.from_branch === removed));
    for (const dst_id of dst_ids) {
      const dst = Boxes.boxes[dst_id];
      if (!dst) continue;
      dst.connections = (dst.connections || [])
        .filter(c => !(c.from_box === my_id && c.from_branch === removed));
      try { await API.put_box(dst_id, dst); }
      catch (e) { console.error('failed to clean wires to ' + dst_id + ': ' + e.message); }
    }

    slots.splice(idx, 1);
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Rename slot at index `idx` to `new_name`. Renames the slot itself
  // and rewrites `from_branch` on every outgoing wire that used the
  // old name (both the source-box record and the destination-box
  // copy). No-op if the name is unchanged or already taken.
  async function rename_iterator_slot(idx, new_name) {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    if (idx < 0 || idx >= slots.length) return;
    const old_name = slots[idx];
    if (old_name === new_name) return;
    if (!new_name) {
      status_msg('slot name cannot be empty', 'error');
      return;
    }
    if (slots.includes(new_name)) {
      status_msg('slot name already in use', 'error');
      return;
    }

    slots[idx] = new_name;

    const my_id = current_box.id;
    const dst_ids = new Set();
    (current_box.connections || []).forEach(c => {
      if (c.from_box === my_id && c.from_branch === old_name) {
        c.from_branch = new_name;
        dst_ids.add(c.to_box);
      }
    });
    for (const dst_id of dst_ids) {
      const dst = Boxes.boxes[dst_id];
      if (!dst) continue;
      (dst.connections || []).forEach(c => {
        if (c.from_box === my_id && c.from_branch === old_name) {
          c.from_branch = new_name;
        }
      });
      try { await API.put_box(dst_id, dst); }
      catch (e) { console.error('failed to rename wires on ' + dst_id + ': ' + e.message); }
    }

    await save();
    Canvas.mark_dirty();
  }

  // Auto-grow check used by the wire-connect path. If `from_branch` is
  // the LAST slot of an iterator box, append a new placeholder slot
  // and persist. Idempotent — calling again on the same slot is a
  // no-op once a newer slot exists below it. Mirrors the variadic
  // input auto-grow.
  async function auto_grow_iterator_after_connect(box, from_branch) {
    if (!is_iterator(box)) return;
    const slots = box.iterator_outputs;
    if (slots[slots.length - 1] !== from_branch) return;

    let n = slots.length;
    while (slots.includes('output_' + n)) n++;
    slots.push('output_' + n);
    try {
      await API.put_box(box.id, box);
      Canvas.mark_dirty();
      if (current_box && current_box.id === box.id) {
        show(current_box, on_change_cb, on_delete_cb);
      }
    } catch (e) {
      slots.pop();
      status_msg('iterator auto-grow failed: ' + e.message, 'error');
    }
  }
  // }}}

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

  // Sever every incoming wire to this box. Used when the input-side
  // shape changes in a way that invalidates all current wires —
  // currently the unmake_variadic path. Mirrors sever_output_wires
  // on the input side.
  async function sever_incoming_wires() {
    if (!current_box) return;
    const my_id = current_box.id;
    const incoming = (current_box.connections || []).filter(c => c.to_box === my_id);
    if (incoming.length === 0) return;

    const src_ids = new Set();
    incoming.forEach(c => src_ids.add(c.from_box));

    current_box.connections = (current_box.connections || [])
      .filter(c => c.to_box !== my_id);

    await Promise.all(Array.from(src_ids).map(async src_id => {
      // Self-loops have src_id === my_id; we already filtered
      // current_box, so re-filtering is a no-op but harmless.
      const src = Boxes.boxes[src_id];
      if (!src) return;
      const before = src.connections || [];
      src.connections = before.filter(c => c.to_box !== my_id);
      try {
        await API.put_box(src_id, src);
      } catch (e) {
        src.connections = before;
        console.error('failed to sever incoming wires from ' + src_id + ': ' + e.message);
      }
    }));

    status_msg('cleared ' + incoming.length + ' incoming wire(s)');
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

  // {{{ rename_port
  // Rename an input port at index `port_idx` on `box`. Updates the
  // port name in box.inputs and rewrites `to_input` on every wire
  // (both endpoints, mutating in place per the stale-cache lesson
  // from 217). Reverses the read-only-name decision from issue 208;
  // tradeoff: a typo desynchronizes the box from the function it
  // calls, but the resulting runtime error surfaces clearly rather
  // than silently skipping (issue 224).
  //
  // Validation: name must be a non-empty identifier-shaped string and
  // unique within the box's inputs. Variadic-slot names (`base_N`)
  // are accepted; the editor uses that shape internally and won't
  // confuse it with a user rename because the variadic operations
  // own the rename path for those cases.
  async function rename_port(box, port_idx, new_name) {
    const port = box.inputs && box.inputs[port_idx];
    if (!port) return false;
    const old_name = port.name;
    if (old_name === new_name) return false;

    if (!new_name || !/^[A-Za-z_]\w*$/.test(new_name)) {
      status_msg('invalid port name: must start with a letter or _', 'error');
      return false;
    }
    if (box.inputs.some((p, i) => i !== port_idx && p.name === new_name)) {
      status_msg('duplicate port name "' + new_name + '"', 'error');
      return false;
    }

    port.name = new_name;

    // Rewrite every wire that targeted the old name (on both
    // endpoints).
    const my_id = box.id;
    const upstream_ids = new Set();
    (box.connections || []).forEach(c => {
      if (c.to_box === my_id && c.to_input === old_name) {
        c.to_input = new_name;
        upstream_ids.add(c.from_box);
      }
    });
    for (const up_id of upstream_ids) {
      const up = Boxes.boxes[up_id];
      if (!up) continue;
      (up.connections || []).forEach(c => {
        if (c.to_box === my_id && c.to_input === old_name) {
          c.to_input = new_name;
        }
      });
      try { await API.put_box(up_id, up); }
      catch (e) { console.error('rename: failed to update ' + up_id + ':', e.message); }
    }

    try { await API.put_box(box.id, box); }
    catch (e) {
      status_msg('rename save failed: ' + e.message, 'error');
      return false;
    }
    Canvas.mark_dirty();
    // Refresh the inspector's read-only name label so it reflects the
    // new name immediately (it caches text on show — without this the
    // user would see the old name in the sidebar until they re-clicked
    // the box).
    if (current_box && current_box.id === box.id) {
      show(current_box, on_change_cb, on_delete_cb);
    }
    return true;
  }
  // }}}

  // {{{ mk_port_display
  // Renders input ports as a two-row block per port: name field +
  // variadic toggle on the top row, value field on the bottom row.
  // Layout chosen so the value (often the longest entry) gets the
  // full sidebar width instead of competing for space with the name
  // (issue 224 reverted: name editing comes back to the inspector
  // because the canvas <input> overlay was eating drag attempts).
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
        const block = document.createElement('div');
        block.className = 'port-display-block';

        const top_row = document.createElement('div');
        top_row.className = 'port-display-row';

        // Variadic context (issue 217 part B):
        //   in_var_group = is part of a variadic group (one of multiple slots)
        //   pv           = { base, index } parsed from the slot name
        const pv           = current_box ? parse_variadic_name(p.name) : null;
        const in_var_group = current_box && is_variadic_slot(current_box, p.name);

        // Editable name input. Variadic slots are managed by the
        // variadic ops (toggle / add / remove), so renaming them
        // through this field is blocked — the operations rely on the
        // `<base>_<N>` pattern, and a stray rename would orphan the
        // slot from its group.
        const name_inp = document.createElement('input');
        name_inp.type      = 'text';
        name_inp.value     = p.name || '';
        name_inp.className = 'port-name-inp';
        if (in_var_group) {
          name_inp.disabled = true;
          name_inp.title    = 'variadic slot name — toggle off the group to rename';
        } else {
          const commit = async () => {
            const new_name = name_inp.value.trim();
            if (new_name === p.name) return;
            const ok = await Inspector.rename_port(current_box, i, new_name);
            if (!ok) name_inp.value = p.name;
          };
          name_inp.addEventListener('blur', commit);
          name_inp.addEventListener('keydown', e => {
            if (e.key === 'Enter')  { e.preventDefault(); name_inp.blur(); }
            if (e.key === 'Escape') { name_inp.value = p.name; name_inp.blur(); }
          });
        }
        top_row.appendChild(name_inp);

        if (p.type && p.type !== 'any') {
          const type_el = document.createElement('span');
          type_el.className   = 'port-type';
          type_el.textContent = ':' + p.type;
          top_row.appendChild(type_el);
        }

        // Variadic control button. Three states:
        //   1. Plain port — "var" toggle turns it on
        //   2. First slot of a variadic group — "var ×" collapses back
        //   3. Non-first slot — "×" removes that slot
        const btn_style = 'background:none;border:1px solid #2a2f45;border-radius:3px;' +
          'color:#6c72a0;cursor:pointer;font-family:monospace;font-size:9px;' +
          'padding:1px 5px;line-height:1.4;';

        if (!in_var_group) {
          const var_btn = document.createElement('button');
          var_btn.style.cssText = btn_style;
          var_btn.textContent   = 'var';
          var_btn.title         = 'mark this input as variadic (grows to N slots)';
          var_btn.onclick       = () => make_variadic(p.name);
          top_row.appendChild(var_btn);
        } else if (pv.index === 0) {
          const var_btn = document.createElement('button');
          var_btn.style.cssText = btn_style + 'border-color:#4a9eff;color:#4a9eff;';
          var_btn.textContent   = 'var ×';
          var_btn.title         = 'collapse back to a single non-variadic input';
          var_btn.onclick       = () => unmake_variadic(pv.base);
          top_row.appendChild(var_btn);
        } else {
          const x_btn = document.createElement('button');
          x_btn.style.cssText = btn_style;
          x_btn.textContent   = '×';
          x_btn.title         = 'remove this slot';
          x_btn.onclick       = () => remove_variadic_slot(p.name);
          top_row.appendChild(x_btn);
        }

        block.appendChild(top_row);

        // Value input — full-width row below the name. Empty means
        // "no literal" (wire supplies the value at runtime).
        const val_inp = document.createElement('input');
        val_inp.type        = 'text';
        val_inp.value       = p.value !== undefined ? String(p.value) : '';
        val_inp.placeholder = 'value…';
        val_inp.className   = 'port-val-inp port-val-inp-wide';
        val_inp.addEventListener('input', async () => {
          const new_val = val_inp.value === '' ? undefined : val_inp.value;
          ports[i].value = new_val;

          // A literal value and an incoming wire are contradictory —
          // sever wires into this port so only one source of truth
          // exists.
          if (new_val !== undefined && current_box) {
            const port_name = ports[i].name;
            const box_id    = current_box.id;
            const to_break  = (current_box.connections || []).filter(
              c => c.to_box === box_id && c.to_input === port_name
            );
            if (to_break.length > 0) {
              current_box.connections = current_box.connections.filter(
                c => !(c.to_box === box_id && c.to_input === port_name)
              );
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

          // Auto-grow when a value lands on a variadic group's last
          // slot. Idempotent.
          if (new_val !== undefined && current_box && in_var_group &&
              pv.index === last_variadic_index(current_box, pv.base)) {
            await auto_grow_after_set(current_box, p.name);
          }

          save();
        });
        block.appendChild(val_inp);

        wrap.appendChild(block);
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
    FileBrowser.render(fields, async (filename, fn) => {
      current_box.ref    = filename;
      current_box.fn     = fn.name;
      current_box.inputs = fn.inputs.map(n => ({ name: n, type: 'any' }));
      // outputs are not set — all boxes have exactly one output wire.
      // When the parsed function has zero return statements, mark the
      // box as a sink (issue 226). Outgoing wires are dropped because
      // the canvas would no longer have an output port to host them.
      if (fn.outputs && fn.outputs.length === 0) {
        current_box.has_output = false;
        await sever_output_wires();
      } else {
        delete current_box.has_output;
      }
      await save();
      show(current_box, on_change_cb, on_delete_cb);
      Canvas.mark_dirty();
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

    // iterator toggle (issue 221) — flips between function-backed and
    // routing-primitive shapes; mutually exclusive with ref/fn/comparator
    const iter_btn = document.createElement('button');
    iter_btn.className   = 'toolbar-btn';
    iter_btn.textContent = is_iterator(box) ? 'iterator ×' : 'iterator';
    iter_btn.onclick = () => {
      if (is_iterator(current_box)) unmake_iterator();
      else make_iterator();
    };
    fields.appendChild(mk_row('mode', iter_btn));

    // For iterator boxes, ref/fn/output are all replaced by the slot
    // list. Inputs still render below (one input wire feeds the
    // routing primitive). The render flow forks here.
    if (is_iterator(box)) {
      // inputs section (same as below; one input is typical but we
      // accept whatever the user has set)
      const in_sec = document.createElement('div');
      in_sec.className   = 'section-label';
      in_sec.textContent = 'inputs';
      fields.appendChild(in_sec);
      fields.appendChild(mk_port_display(box.inputs));

      // outputs: editable list of slot names with × per slot, + at end
      const out_sec = document.createElement('div');
      out_sec.className   = 'section-label';
      out_sec.textContent = 'outputs (round-robin)';
      fields.appendChild(out_sec);

      const slot_wrap = document.createElement('div');
      slot_wrap.className = 'port-display';
      box.iterator_outputs.forEach((slot, i) => {
        const row = document.createElement('div');
        row.className = 'port-display-row';

        const inp = document.createElement('input');
        inp.type      = 'text';
        inp.value     = slot;
        inp.className = 'port-name-inp';
        inp.style.flex = '1';
        inp.addEventListener('change', () => {
          rename_iterator_slot(i, inp.value.trim());
        });
        row.appendChild(inp);

        const rm = document.createElement('button');
        rm.className   = 'toolbar-btn';
        rm.textContent = '×';
        rm.title       = 'remove slot';
        rm.onclick     = () => remove_iterator_slot(i);
        row.appendChild(rm);

        slot_wrap.appendChild(row);
      });
      const add_btn = document.createElement('button');
      add_btn.className   = 'toolbar-btn';
      add_btn.textContent = '+ slot';
      add_btn.onclick     = () => add_iterator_slot();
      slot_wrap.appendChild(add_btn);
      fields.appendChild(slot_wrap);
      return;
    }

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

    // output: single wire; optional comparator splits into lt/eq/gt.
    // Skipped entirely for sink boxes (functions with no return values,
    // issue 226) — no output dot, no comparator toggle, nothing to wire.
    if (box.has_output === false) {
      return;
    }

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

  return { show, hide, show_content, auto_grow_after_set,
           auto_grow_iterator_after_connect, rename_port };
})();
