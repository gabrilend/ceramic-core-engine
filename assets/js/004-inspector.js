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

  // {{{ Language-spec registry (issue 217 part C)
  // Loads `langs/<name>/spec.js` on demand so the variadic UI can ask
  // the language — not the per-function source — whether a `var`
  // toggle is meaningful. Today every shipped language declares
  // `variadic_shape: 'positional'`, so the toggle shows; a future
  // language declaring `'none'` (or anything else we don't know how
  // to render) would suppress it for its boxes. Cached per session
  // to mirror the lexer / parser registries elsewhere.
  const SPEC_EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash', c: 'c', h: 'c' };
  const spec_cache = {};   // lang_name → LANGUAGE_SPEC | null

  // {{{ function lang_for_ref()
  function lang_for_ref(ref) {
    if (!ref) return null;
    const m = /\.([^./]+)$/.exec(ref);
    if (!m) return null;
    return SPEC_EXT_TO_LANG[m[1].toLowerCase()] || null;
  }
  // }}}

  // {{{ async function load_spec()
  async function load_spec(lang) {
    if (!lang) return null;
    if (lang in spec_cache) return spec_cache[lang];
    try {
      const mod = await import('/langs/' + lang + '/spec.js');
      spec_cache[lang] = mod.LANGUAGE_SPEC || null;
      return spec_cache[lang];
    } catch (e) {
      console.warn('no spec.js for ' + lang + ':', e.message);
      spec_cache[lang] = null;
      return null;
    }
  }
  // }}}

  // {{{ function spec_for_box()
  // Sync read of an already-loaded spec. Returns null when the spec
  // hasn't loaded yet — callers default to "no gate" so the first
  // render of a box doesn't blink the var button off and on.
  function spec_for_box(box) {
    const lang = lang_for_ref(box && box.ref);
    return (lang && lang in spec_cache) ? spec_cache[lang] : null;
  }
  // }}}
  // }}}

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

  // {{{ basename_no_ext
  // Strip the directory components and the final extension from a
  // path. Used by the ref display so the inspector shows
  // "write-result" instead of "src/write-result.lua" — the full path
  // is still on box.ref for the runtime, only the display is short.
  function basename_no_ext(path) {
    if (!path) return '';
    const base = path.split('/').pop();
    return base.replace(/\.[^.]+$/, '');
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

  // {{{ Routing helpers (issue 233)
  // Every call box carries a `routing` field declaring how its
  // single output reaches downstream wires. Six kinds ship:
  //
  //   plain       — single output, fan to every wire
  //   comparator  — lt/eq/gt by `comparand`, OR multi-band by
  //                 `thresholds` array (issue 243); the band names
  //                 are below_<t> / between_<a>_<b> / above_<t>
  //                 (plus eq_<t> for doubled adjacent thresholds)
  //   iterator    — round-robin over N `out_<i>` ports, multi-spawn
  //   randomizer  — hash(counter) mod N pick over `out_<i>` ports
  //                 (issue 240)
  //   weighted    — probability-weighted pick over `out_<i>` ports,
  //                 weights array length = output count (issue 241)
  //   distributor — argmin over downstream slot fill, picks the
  //                 least-busy `out_<i>` (issue 242)
  //
  // Plain is the default for a newly-created box. Switching kinds
  // severs every outgoing wire because the output port shape
  // changes — same shape-change-clears-wires rule the variadic
  // input toggle uses.
  //
  // Iterator routing is a pure routing primitive: no language
  // function is invoked, ref/fn are not set. Iterator port names
  // are fixed as `out_0`, `out_1`, ... `out_<n-1>` (not user-
  // renameable — the read-only-port-name rule from issue 224
  // applies on the output side too).

  // {{{ function routing_kind()
  function routing_kind(box) {
    return (box && box.routing && box.routing.kind) || 'plain';
  }
  // }}}

  // {{{ function default_routing_for()
  // Every kind has its own minimum-viable starting shape so
  // flipping the dropdown leaves the box in a schema-valid
  // state without the user having to fill in any control first.
  function default_routing_for(kind) {
    if (kind === 'comparator')  return { kind: 'comparator',  comparand: 0 };
    if (kind === 'iterator')    return { kind: 'iterator',    n_outputs: 2 };
    if (kind === 'randomizer')  return { kind: 'randomizer',  n_outputs: 2 };
    if (kind === 'distributor') return { kind: 'distributor', n_outputs: 2 };
    if (kind === 'weighted')    return { kind: 'weighted',    weights:   [0.5, 0.5] };
    return { kind: 'plain' };
  }
  // }}}

  // {{{ async function set_routing_kind()
  async function set_routing_kind(kind) {
    if (!current_box) return;
    if (routing_kind(current_box) === kind) return;
    await sever_output_wires();
    current_box.routing = default_routing_for(kind);
    if (kind === 'iterator') {
      // Iterator boxes are routing primitives — no function.
      delete current_box.ref;
      delete current_box.fn;
      delete current_box.has_output;
    } else {
      // Plain/comparator need a function. Restore empty ref/fn
      // so the schema (which requires ref on non-iterator call
      // boxes) accepts the save; the user picks via the browse
      // button to populate them.
      if (current_box.ref === undefined) current_box.ref = '';
      if (current_box.fn  === undefined) current_box.fn  = '';
    }
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }
  // }}}

  // {{{ async function set_comparand()
  async function set_comparand(n) {
    if (!current_box || routing_kind(current_box) !== 'comparator') return;
    current_box.routing.comparand = n;
    await save();
  }
  // }}}

  // {{{ async function set_thresholds()
  // Issue 243 — multi-band-comparator thresholds setter. Parses a
  // comma-separated text input into a numeric array; an empty
  // input restores single-comparand mode (clears the thresholds
  // field so the loader's legacy path runs). Non-decreasing is
  // enforced by sorting silently — if the user typed "7, 3" we
  // store "3, 7". Changing thresholds renames every output
  // port (the band names embed the threshold values), so this
  // severs all outgoing wires the same way iterator's
  // n_outputs change does.
  async function set_thresholds(text) {
    if (!current_box || routing_kind(current_box) !== 'comparator') return;
    const trimmed = String(text || '').trim();
    if (trimmed === '') {
      // Empty input → single-comparand mode. Drop the array.
      if ('thresholds' in (current_box.routing || {})) {
        await sever_output_wires();
        delete current_box.routing.thresholds;
        await save();
        Canvas.mark_dirty();
        show(current_box, on_change_cb, on_delete_cb);
      }
      return;
    }
    const nums = trimmed.split(',')
      .map(s => parseFloat(s.trim()))
      .filter(n => !isNaN(n));
    if (nums.length === 0) return;
    nums.sort((a, b) => a - b);
    await sever_output_wires();
    current_box.routing.thresholds = nums;
    // Remove the legacy field so the loader picks the
    // multi-band path unambiguously.
    delete current_box.routing.comparand;
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }
  // }}}

  // {{{ async function set_weights()
  // Issue 241 — weighted routing weights setter. Parses a
  // comma-separated text input into a non-negative number array.
  // Changing the array length changes the n_out shape (one port
  // per weight), so this severs all outgoing wires.
  async function set_weights(text) {
    if (!current_box || routing_kind(current_box) !== 'weighted') return;
    const trimmed = String(text || '').trim();
    if (trimmed === '') return;
    const nums = trimmed.split(',')
      .map(s => parseFloat(s.trim()))
      .filter(n => !isNaN(n) && n >= 0);
    if (nums.length === 0) return;
    await sever_output_wires();
    current_box.routing.weights = nums;
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }
  // }}}

  // {{{ async function set_n_outputs()
  // Shared n_outputs port-count setter for iterator (issue 233),
  // randomizer (240), and distributor (242) — all three use
  // `out_<i>` ports. Shrinking the count means wires whose
  // from_branch is `out_<i>` with i >= new_n no longer have a
  // port to attach to — sever them on the source side and on
  // each destination box's copy, same pattern as the iterator
  // slot removal used under the legacy schema.
  async function set_n_outputs(n) {
    if (!current_box) return;
    const rk = routing_kind(current_box);
    if (rk !== 'iterator' && rk !== 'randomizer' && rk !== 'distributor') return;
    if (typeof n !== 'number' || n < 1 || n !== Math.floor(n)) return;
    const old_n = current_box.routing.n_outputs || 1;
    current_box.routing.n_outputs = n;
    if (n < old_n) {
      const my_id = current_box.id;
      const orphans = new Set();
      for (let i = n; i < old_n; i++) orphans.add('out_' + i);
      const dst_ids = new Set();
      (current_box.connections || []).forEach(c => {
        if (c.from_box === my_id && orphans.has(c.from_branch)) {
          dst_ids.add(c.to_box);
        }
      });
      current_box.connections = (current_box.connections || [])
        .filter(c => !(c.from_box === my_id && orphans.has(c.from_branch)));
      for (const dst_id of dst_ids) {
        const dst = Boxes.boxes[dst_id];
        if (!dst) continue;
        dst.connections = (dst.connections || [])
          .filter(c => !(c.from_box === my_id && orphans.has(c.from_branch)));
        try { await API.put_box(dst_id, dst); }
        catch (e) { console.error('failed to clean wires to ' + dst_id + ': ' + e.message); }
      }
    }
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }
  // }}}
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

  // {{{ create_shim_for — issue 246
  // Creates a per-port custom translation file with default-spec-
  // equivalent content (identity pass-through) and sets the port's
  // `custom_translation` field on the current box. The filename
  // convention matches issue 246: <box_id>__<port_name>.<lang_ext>.
  function shim_default_content(lang) {
    if (lang === 'lua') {
      return [
        '-- Per-port custom translation shim (issue 246).',
        '-- The file returns a single function taking the raw bytes',
        '-- and a "raw_native" flag (true if the bytes came from a',
        '-- same-language producer, false if from a cross-language',
        '-- producer via JSON). Return the translated bytes the box',
        '-- function will see in place of the raw bytes.',
        '--',
        '-- Default: identity pass-through. Edit to add your',
        '-- per-port decode / coerce / validate logic.',
        '',
        'return function(raw, raw_native)',
        '    return raw',
        'end',
        '',
      ].join('\n');
    }
    if (lang === 'c') {
      return [
        '/* Per-port custom translation shim (issue 246).',
        ' * sm_translate is the entry symbol. Default is identity',
        ' * pass-through — edit to add decode / coerce / validate */',
        '#include <string.h>',
        '',
        'int sm_translate(const void *raw, int raw_size, int raw_native,',
        '                 void *out_buf, int out_capacity, int *out_size)',
        '{',
        '    (void)raw_native;',
        '    if (raw_size > out_capacity) return -1;',
        '    memcpy(out_buf, raw, (size_t)raw_size);',
        '    *out_size = raw_size;',
        '    return 0;',
        '}',
        '',
      ].join('\n');
    }
    // Unknown language — give the user an empty file so they can
    // author whatever's right; the spec will error at task time if
    // the shape doesn't match its translate callback's expectations.
    return '-- Custom translation shim — author for lang: ' + lang + '\n';
  }

  function shim_ext_for(lang) {
    if (lang === 'lua') return 'lua';
    if (lang === 'c')   return 'c';
    if (lang === 'bash')return 'sh';
    return 'txt';
  }

  async function create_shim_for(port) {
    if (!current_box || !current_box.id || !port || !port.name) return;
    const lang = current_box.lang || 'lua';
    const ext  = shim_ext_for(lang);
    const fname = current_box.id + '__' + port.name + '.' + ext;
    const content = shim_default_content(lang);
    try {
      await API.put_translation(fname, content);
    } catch (e) {
      status_msg('shim create failed: ' + e.message, 'error');
      return;
    }
    port.custom_translation = 'translations/' + fname;
    await save();
    status_msg('created shim: translations/' + fname);
    show(current_box, on_change_cb, on_delete_cb);
    Canvas.mark_dirty();
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

        // Read-only name label. Port names come from the parsed
        // function signature (file browser populates them); editing
        // them in the inspector is gone — the original 208 rationale
        // (prevent typo drift between source and box JSON) holds, and
        // post-fact rename was just a user tag anyway.
        const name_el = document.createElement('span');
        name_el.className   = 'port-name-display';
        name_el.textContent = p.name || '';
        top_row.appendChild(name_el);

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
          // Issue 217 part C: the var toggle is offered when the box's
          // language declares `variadic_shape: 'positional'`. Until
          // the spec is loaded we default to "offered" (every shipped
          // language is positional) so the first render of a box
          // doesn't lose the button. Already-variadic groups keep
          // their collapse + remove controls regardless, so flipping
          // a language to 'none' later can't trap an existing group.
          const spec          = spec_for_box(current_box);
          const var_supported = !spec || spec.variadic_shape === 'positional';
          if (var_supported) {
            const var_btn = document.createElement('button');
            var_btn.style.cssText = btn_style;
            var_btn.textContent   = 'var';
            var_btn.title         = 'mark this input as variadic (grows to N slots)';
            var_btn.onclick       = () => make_variadic(p.name);
            top_row.appendChild(var_btn);
          }
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

        // `opt` toggle (issue 230). Flips port.optional between true
        // and undefined (absent = false). An optional port doesn't
        // require a wire or literal — the compile-time check from
        // 230 allows it through. Amber styling so it reads as a
        // different category from the blue `var` family.
        const opt_btn = document.createElement('button');
        const opt_on  = ports[i].optional === true;
        opt_btn.style.cssText = btn_style + (opt_on
          ? 'border-color:#d4762a;color:#d4762a;'
          : '');
        opt_btn.textContent = 'opt';
        opt_btn.title       = 'mark this input as optional — the box runs even if unwired';
        opt_btn.onclick     = () => {
          ports[i].optional = opt_on ? undefined : true;
          save();
          show(current_box, on_change_cb, on_delete_cb);
        };
        top_row.appendChild(opt_btn);

        // {{{ Custom translation shim affordance (issue 246)
        // Three states:
        //   - no custom_translation: "+xlate" button — creates a
        //     default shim file matching the box's language and
        //     sets the port's custom_translation field.
        //   - custom_translation set: "xlate" button opens the shim
        //     in the source-view window; small "×" clears the field
        //     (file stays on disk per the issue's delete rule).
        // Suppressed entirely when the box has no language (read /
        // data / write boxes — shims only make sense on call boxes
        // whose spec runs the translate callback). */
        if (current_box && current_box.kind === 'call' && current_box.lang) {
          const has_shim = !!ports[i].custom_translation;
          if (!has_shim) {
            const new_btn = document.createElement('button');
            new_btn.style.cssText = btn_style;
            new_btn.textContent   = '+xlate';
            new_btn.title         = 'add a per-port custom translation shim (issue 246)';
            new_btn.onclick       = () => create_shim_for(ports[i]);
            top_row.appendChild(new_btn);
          } else {
            const view_btn = document.createElement('button');
            view_btn.style.cssText = btn_style + 'border-color:#e8a317;color:#e8a317;';
            view_btn.textContent   = 'xlate';
            view_btn.title         = 'view shim: ' + ports[i].custom_translation;
            view_btn.onclick       = () =>
              SourceView.open_source_view(ports[i].custom_translation, current_box);
            top_row.appendChild(view_btn);

            const clr_btn = document.createElement('button');
            clr_btn.style.cssText = btn_style + 'border-color:#e8a317;color:#e8a317;';
            clr_btn.textContent   = '×';
            clr_btn.title         = 'clear shim reference (file stays on disk)';
            clr_btn.onclick       = () => {
              ports[i].custom_translation = undefined;
              save();
              show(current_box, on_change_cb, on_delete_cb);
            };
            top_row.appendChild(clr_btn);
          }
        }
        // }}}

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
      // Issue 236: when the user picks from a DIFFERENT source file,
      // snap the box label to that file's basename so the canvas
      // header stays in sync. Same file (different function within
      // it) is left alone — the user is just swapping which entry
      // from this module they're calling, and any custom label they
      // typed (e.g. "Join paragraphs") should survive that.
      if (current_box.ref !== filename) {
        current_box.label = Boxes.default_header({ kind: 'call', ref: filename });
      }
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

  // {{{ render_external_binding (issue 248)
  // Inspector affordance for the `external` block on `read` and
  // `write` boxes. When toggled on, marks a read box as
  // **externally-supplied** (its value comes from the encapsulating
  // parent's wire) or a write box as **externally-consumed** (its
  // emitted value surfaces back to the parent on a wire). Same
  // visual idiom for both; the only difference is what the
  // marking means in the encapsulation splice.
  //
  // The block records a binding kind plus either a name (for
  // named matching against the encap's port list) or an index
  // (for positional / numbered). The loader's
  // encapsulation pass reads this to wire the parent's
  // producers through to the corresponding sub-map data flow.
  function render_external_binding(fields, box) {
    if (box.kind !== 'read' && box.kind !== 'write') return;

    const role = box.kind === 'read' ? 'externally-supplied'
                                     : 'externally-consumed';
    const sec = document.createElement('div');
    sec.className   = 'section-label';
    sec.textContent = role + ' (issue 248)';
    fields.appendChild(sec);

    // Top row: checkbox toggle on / off. When off, the rest of
    // the block doesn't render — the port behaves as a normal
    // read literal / write file sink. When on, the binding kind
    // dropdown + the matching name / index field appear below.
    const toggle_wrap = document.createElement('div');
    toggle_wrap.style.cssText = 'display:flex;gap:6px;align-items:center;';
    const toggle = document.createElement('input');
    toggle.type    = 'checkbox';
    toggle.checked = !!(box.external && box.external.kind);
    const toggle_lbl = document.createElement('span');
    toggle_lbl.style.cssText = 'font-size:11px;color:#9ea3c0;';
    toggle_lbl.textContent = toggle.checked
      ? 'bound to encapsulating parent'
      : '(off — port behaves normally)';
    toggle.addEventListener('change', () => {
      if (toggle.checked) {
        current_box.external = current_box.external && current_box.external.kind
          ? current_box.external
          : { kind: 'named', name: '' };
      } else {
        delete current_box.external;
      }
      show(current_box, on_change_cb, on_delete_cb);
      save();
    });
    toggle_wrap.appendChild(toggle);
    toggle_wrap.appendChild(toggle_lbl);
    fields.appendChild(mk_row('external', toggle_wrap));

    if (!toggle.checked) return;

    // Binding-kind dropdown. The three kinds mirror the matching
    // rules the encapsulation splice uses:
    //   named       → matched by string against encap port's name
    //   positional  → matched by argument order
    //   numbered    → matched by an explicit integer index
    const kind_sel = document.createElement('select');
    ['named', 'positional', 'numbered'].forEach(k => {
      const opt = document.createElement('option');
      opt.value = k; opt.textContent = k;
      if (k === (box.external && box.external.kind)) opt.selected = true;
      kind_sel.appendChild(opt);
    });
    kind_sel.addEventListener('change', () => {
      const next = kind_sel.value;
      const prev = current_box.external || {};
      // Preserve a name if we're keeping `named`; reset on a kind
      // change to avoid stale fields tagging along.
      if (next === 'named') {
        current_box.external = { kind: 'named', name: prev.name || '' };
      } else {
        current_box.external = { kind: next, index: prev.index || 0 };
      }
      show(current_box, on_change_cb, on_delete_cb);
      save();
    });
    fields.appendChild(mk_row('kind', kind_sel));

    if (box.external.kind === 'named') {
      const name_inp = document.createElement('input');
      name_inp.type        = 'text';
      name_inp.value       = box.external.name || '';
      name_inp.placeholder = 'port name on encap';
      name_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      name_inp.addEventListener('input', () => {
        current_box.external.name = name_inp.value;
        save();
      });
      fields.appendChild(mk_row('name', name_inp));
    } else {
      const idx_inp = document.createElement('input');
      idx_inp.type    = 'number';
      idx_inp.min     = '0';
      idx_inp.value   = String(box.external.index ?? 0);
      idx_inp.style.cssText = 'width:80px;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      idx_inp.addEventListener('change', () => {
        const v = parseInt(idx_inp.value, 10);
        if (!isNaN(v) && v >= 0) {
          current_box.external.index = v;
          save();
        }
      });
      fields.appendChild(mk_row('index', idx_inp));
    }
  }
  // }}}

  // {{{ render_map_box (issue 248)
  // Specialized inspector for `kind: "map"` boxes (encapsulated
  // sub-maps). Shows the ref path (the sub-map directory it
  // points at) and the declared input + output port arrays —
  // the user can edit each port's name. The arrays are normally
  // populated by the encapsulate action at box-creation time
  // (derived from the sub-map's externally-marked boxes) but
  // remain editable here so the user can rename or re-sync when
  // the sub-map drifts.
  function render_map_box(fields, box) {
    if (box.kind !== 'map') return;

    // ref display + edit input. Free-text rather than a file
    // picker because the path is a directory, not a source file,
    // and the existing file browser doesn't browse directories
    // outside the source-search-path. A future slice can add a
    // map-directory picker.
    const ref_inp = document.createElement('input');
    ref_inp.type        = 'text';
    ref_inp.value       = box.ref || '';
    ref_inp.placeholder = '../some-other-map';
    ref_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
      'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
    ref_inp.addEventListener('input', () => {
      current_box.ref = ref_inp.value;
      save();
    });
    fields.appendChild(mk_row('ref', ref_inp));

    // Helper: render an editable list of {name, type} entries
    // backed by the named field on the box. Each row is a
    // text input for the name; "+" appends, "−" on each row
    // removes. Used for both inputs[] and outputs[].
    const mk_port_list = (label, key) => {
      const sec = document.createElement('div');
      sec.className   = 'section-label';
      sec.textContent = label;
      fields.appendChild(sec);

      const list_wrap = document.createElement('div');
      list_wrap.style.cssText = 'display:flex;flex-direction:column;gap:3px;';
      const entries = box[key] || [];
      entries.forEach((p, i) => {
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;gap:4px;align-items:center;';
        const name_inp = document.createElement('input');
        name_inp.type  = 'text';
        name_inp.value = p.name || '';
        name_inp.placeholder = label.slice(0, -1) + ' name';
        name_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
          'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:3px 5px;';
        name_inp.addEventListener('input', () => {
          (current_box[key] || [])[i].name = name_inp.value;
          save();
        });
        const rm_btn = document.createElement('button');
        rm_btn.className   = 'toolbar-btn';
        rm_btn.textContent = '−';
        rm_btn.style.cssText = 'padding:2px 8px;';
        rm_btn.onclick = () => {
          (current_box[key] || []).splice(i, 1);
          show(current_box, on_change_cb, on_delete_cb);
          save();
        };
        row.appendChild(name_inp);
        row.appendChild(rm_btn);
        list_wrap.appendChild(row);
      });
      const add_btn = document.createElement('button');
      add_btn.className   = 'toolbar-btn';
      add_btn.textContent = '+ ' + label.slice(0, -1);
      add_btn.style.alignSelf = 'flex-start';
      add_btn.onclick = () => {
        current_box[key] = current_box[key] || [];
        current_box[key].push({
          name: label.slice(0, -1) + '_' + current_box[key].length,
          type: 'string',
        });
        show(current_box, on_change_cb, on_delete_cb);
        save();
      };
      list_wrap.appendChild(add_btn);
      fields.appendChild(list_wrap);
    };

    mk_port_list('inputs',  'inputs');
    mk_port_list('outputs', 'outputs');

    const note = document.createElement('div');
    note.style.cssText = 'font-size:10px;color:#6c72a0;margin-top:8px;line-height:1.4;';
    note.textContent = 'inputs / outputs match the sub-map’s ' +
      'externally-supplied read boxes and externally-consumed write ' +
      'boxes by name (or by index for positional / numbered bindings).';
    fields.appendChild(note);
  }
  // }}}

  // {{{ render_io_box
  // Issue 229: `read` boxes carry an optional inline `value` literal
  // that, when set, hides the `path` input port on the canvas and
  // overrides the file-read path entirely. `write` boxes have no
  // inline value — they receive what they write through the `value`
  // input port. Both kinds skip the call-box machinery (ref/fn/
  // routing don't apply).
  //
  // The textarea below feeds box.value. Setting it non-empty puts
  // the box in literal-source mode; clearing it returns to file-
  // source mode where box.path (or the wired `path` input) is used.
  function render_io_box(fields, box) {
    if (box.kind !== 'read') return;

    const ta = document.createElement('textarea');
    ta.className   = 'inspector-value';
    ta.value       = box.value || '';
    ta.rows        = 3;
    ta.placeholder = '(empty — read from path instead)';
    ta.addEventListener('input', () => {
      current_box.value = ta.value;
      // Force a re-render so the path input port hides / un-hides on
      // the canvas immediately.
      show(current_box, on_change_cb, on_delete_cb);
      save();
    });
    fields.appendChild(mk_row('value', ta));
  }
  // }}}

  // {{{ show
  function show(box, on_changed, on_delete) {
    current_box  = box;
    on_change_cb = on_changed;
    on_delete_cb = on_delete || null;
    panel.classList.remove('hidden');

    // Pre-warm the spec cache for this box's language so subsequent
    // renders of the same box read a known `variadic_shape`. First
    // render of a never-seen language paints with the default (var
    // toggle visible); the re-render kicked off here applies the
    // real gate once spec.js has resolved. No-op if the spec is
    // already cached or if box has no ref (no language to look up).
    const lang = lang_for_ref(box && box.ref);
    if (lang && !(lang in spec_cache)) {
      load_spec(lang).then(() => {
        if (current_box === box) show(box, on_changed, on_delete);
      });
    }

    // title: editable label input + read-only id subtitle. The
    // placeholder shows the canvas-side default (issue 236) — basename
    // of ref for a call box, the kind name otherwise — so the user
    // can see what would render with the field left empty.
    title.innerHTML = '';
    const lbl_inp = document.createElement('input');
    lbl_inp.type        = 'text';
    lbl_inp.value       = box.label || '';
    lbl_inp.placeholder = Boxes.default_header(box);
    lbl_inp.className   = 'inspector-title-inp';
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
      // 'map' (issue 248) is the encapsulated-sub-map kind — picks
      // a sub-map directory as a single box on the parent canvas.
      // Flipping to or away from 'map' changes the box's port
      // shape, so the canvas re-renders below via show().
      ['call', 'read', 'write', 'map'].forEach(k => {
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

    // Issue 229: read and write boxes have a different inspector shape
    // than call boxes — `ref`/`fn`/routing don't apply, and `read` has
    // an inline `value` field that swaps out the path port. Render the
    // IO-primitive form and stop here so the call-box machinery below
    // doesn't try to attach.
    if (box.kind === 'read' || box.kind === 'write') {
      render_io_box(fields, box);
      // Inputs section — same port table as call boxes.
      const in_sec = document.createElement('div');
      in_sec.className   = 'section-label';
      in_sec.textContent = 'inputs';
      fields.appendChild(in_sec);
      fields.appendChild(mk_port_display(box.inputs));
      // Issue 248: externally-supplied / externally-consumed
      // binding to the encapsulating parent. Off by default;
      // when on, this box's role flips from disk-IO to wire-IO
      // through the parent's encapsulation splice.
      render_external_binding(fields, box);
      return;
    }

    // Issue 248: encapsulated sub-map box. ref + inputs + outputs
    // arrays; no routing, no language function. Stop here so the
    // call-box machinery below doesn't try to attach a fn / refs.
    if (box.kind === 'map') {
      render_map_box(fields, box);
      return;
    }

    // Routing-kind dropdown (issue 233). The mode picks which
    // dispatch-layer rule decides where this box's output goes:
    //   plain       → fan to every wire on the single output
    //   comparator  → lt/eq/gt by comparand, OR multi-band by
    //                 thresholds[] (issue 243)
    //   iterator    → N round-robin out_<i> ports (multi-spawn)
    //   randomizer  → hash(counter) mod N over out_<i> (issue 240)
    //   weighted    → cumulative-band lookup over weights (issue 241)
    //   distributor → least-busy of out_<i> by downstream fill
    //                 (issue 242)
    // Switching kinds changes the output-port shape on the
    // canvas; set_routing_kind severs every outgoing wire on the
    // transition for the same reason the variadic input toggle
    // clears its inputs.
    const mode_sel = document.createElement('select');
    ['plain', 'comparator', 'iterator', 'randomizer', 'weighted', 'distributor'].forEach(k => {
      const opt = document.createElement('option');
      opt.value = k; opt.textContent = k;
      if (k === routing_kind(box)) opt.selected = true;
      mode_sel.appendChild(opt);
    });
    mode_sel.addEventListener('change', () => set_routing_kind(mode_sel.value));
    fields.appendChild(mk_row('routing', mode_sel));

    // Iterator boxes are pure routing primitives — no ref/fn, no
    // language function invoked. The inspector shows inputs (a
    // single input wire feeds the routing) plus an n_outputs
    // number control; the output ports themselves are fixed as
    // out_0..out_<n-1>, not user-renameable (issue 224 rule
    // extended to the output side).
    if (routing_kind(box) === 'iterator') {
      const in_sec = document.createElement('div');
      in_sec.className   = 'section-label';
      in_sec.textContent = 'inputs';
      fields.appendChild(in_sec);
      fields.appendChild(mk_port_display(box.inputs));

      const out_sec = document.createElement('div');
      out_sec.className   = 'section-label';
      out_sec.textContent = 'outputs (round-robin)';
      fields.appendChild(out_sec);

      const n_inp = document.createElement('input');
      n_inp.type        = 'number';
      n_inp.min         = '1';
      n_inp.value       = String((box.routing && box.routing.n_outputs) || 2);
      n_inp.style.cssText = 'width:80px;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      n_inp.addEventListener('change', () => {
        const v = parseInt(n_inp.value, 10);
        if (!isNaN(v) && v >= 1) set_n_outputs(v);
      });
      fields.appendChild(mk_row('n_outputs', n_inp));
      return;
    }


    // ref: read-only display showing just the basename (no path, no
    // extension), plus browse / view buttons. Full path stored on the
    // box still — the display is just a UI tightening since paths can
    // be long enough to overflow the sidebar.
    const ref_wrap = document.createElement('div');
    ref_wrap.style.cssText = 'display:flex;gap:4px;align-items:center;';
    const ref_disp = document.createElement('span');
    ref_disp.className = 'ref-display';
    ref_disp.title     = box.ref || '';   // full path on hover
    ref_disp.textContent = basename_no_ext(box.ref) || '(none)';
    const browse_btn = document.createElement('button');
    browse_btn.className   = 'toolbar-btn';
    browse_btn.textContent = 'browse';
    browse_btn.style.whiteSpace = 'nowrap';
    browse_btn.onclick = open_browser;
    // "view" button opens a floating, draggable read-only window with
    // the current ref's source content (issue 215). Hidden when ref
    // is empty since there's nothing to fetch (issue 227).
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
    ref_wrap.appendChild(ref_disp);
    ref_wrap.appendChild(browse_btn);
    ref_wrap.appendChild(view_btn);
    fields.appendChild(mk_row('ref', ref_wrap));

    // fn: clickable button that jumps the file browser to the
    // function-list step for the current ref. Lets the user pick a
    // different fn from the same file, or back-button up to swap files
    // entirely. Reads as a button rather than a label so the
    // affordance is unambiguous.
    const fn_btn = document.createElement('button');
    fn_btn.className   = 'fn-btn';
    fn_btn.textContent = box.fn || '(no function)';
    fn_btn.title       = 'change function — opens file browser at the function list';
    fn_btn.disabled    = !box.ref;
    fn_btn.onclick     = () => {
      title.textContent = 'pick fn';
      fields.innerHTML  = '';
      FileBrowser.render_at_fn_list(fields, current_box.ref, async (filename, fn) => {
        // Same label-swap rule as the full browse (issue 236) —
        // changing the file overwrites the label, switching fn
        // within the same file leaves it alone.
        if (current_box.ref !== filename) {
          current_box.label = Boxes.default_header({ kind: 'call', ref: filename });
        }
        current_box.ref    = filename;
        current_box.fn     = fn.name;
        current_box.inputs = fn.inputs.map(n => ({ name: n, type: 'any' }));
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
    };
    fields.appendChild(mk_row('fn', fn_btn));

    // inputs: read-only names from parsing, editable literal values
    const in_sec = document.createElement('div');
    in_sec.className   = 'section-label';
    in_sec.textContent = 'inputs';
    fields.appendChild(in_sec);
    fields.appendChild(mk_port_display(box.inputs));

    // output section (issue 233). Per-kind controls live below
    // the mode dropdown above. Sink boxes (has_output false)
    // skip the section entirely — no output dot, no controls.
    if (box.has_output === false) {
      return;
    }

    const out_sec = document.createElement('div');
    out_sec.className   = 'section-label';
    out_sec.textContent = 'output';
    fields.appendChild(out_sec);

    // Per-kind controls. Plain has no per-kind knob; comparator,
    // randomizer, weighted, distributor each carry their own.
    const rk = routing_kind(box);
    if (rk === 'comparator') {
      // Issue 233 single-comparand UI stays the default; issue 243
      // adds an inline thresholds editor underneath. The two
      // schema-side fields are mutually exclusive — the loader
      // prefers `thresholds` when both are present. The editor
      // mirrors that: typing into the threshold list switches the
      // box to multi-band mode (comparand removed), clearing the
      // list returns to single-comparand mode (thresholds removed).
      const cmp_inp = document.createElement('input');
      cmp_inp.type        = 'number';
      cmp_inp.value       = String((box.routing && box.routing.comparand) ?? 0);
      cmp_inp.placeholder = 'number';
      cmp_inp.style.cssText = 'width:100%;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      cmp_inp.disabled = Array.isArray(box.routing && box.routing.thresholds)
                         && box.routing.thresholds.length > 0;
      cmp_inp.addEventListener('input', () => {
        const v = parseFloat(cmp_inp.value);
        if (!isNaN(v)) set_comparand(v);
      });
      fields.appendChild(mk_row('comparand', cmp_inp));

      // Multi-band threshold list (issue 243). Comma-separated
      // numbers in a text input keeps the UX minimal; a richer
      // sortable list with +/− buttons can land later if the
      // textarea becomes painful. Doubled adjacent values mark
      // zero-width equality bands (e.g. "3,3,7,7" gives the
      // < 3 / == 3 / 3<x<7 / == 7 / > 7 layout).
      const ts_inp = document.createElement('input');
      ts_inp.type        = 'text';
      ts_inp.placeholder = 'e.g. 3, 7, 11 (multi-band)';
      ts_inp.value       = Array.isArray(box.routing && box.routing.thresholds)
                             ? box.routing.thresholds.join(', ')
                             : '';
      ts_inp.style.cssText = 'width:100%;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      ts_inp.addEventListener('change', () => set_thresholds(ts_inp.value));
      fields.appendChild(mk_row('thresholds', ts_inp));

      const ts_note = document.createElement('div');
      ts_note.style.cssText = 'font-size:10px;color:#6c72a0;margin-top:4px;';
      ts_note.textContent = 'comma-separated, non-decreasing. doubled values carve equality bands.';
      fields.appendChild(ts_note);
    } else if (rk === 'randomizer' || rk === 'distributor') {
      // Same n_outputs control iterator uses, but the routing
      // semantics are different — randomizer (240) picks branches
      // pseudo-randomly via a hashed counter; distributor (242)
      // picks the least-busy downstream branch by reading slot
      // fill levels at dispatch time.
      const n_inp = document.createElement('input');
      n_inp.type        = 'number';
      n_inp.min         = '1';
      n_inp.value       = String((box.routing && box.routing.n_outputs) || 2);
      n_inp.style.cssText = 'width:80px;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      n_inp.addEventListener('change', () => {
        const v = parseInt(n_inp.value, 10);
        if (!isNaN(v) && v >= 1) set_n_outputs(v);
      });
      fields.appendChild(mk_row('n_outputs', n_inp));
    } else if (rk === 'weighted') {
      // Issue 241: weights array editor. Minimum viable shape is
      // a comma-separated textbox; same upgrade path as the
      // multi-band thresholds list — replace with sliders later
      // if a real visual band UI earns its keep. Non-negative
      // numbers only; the dispatch normalises at run time so
      // they don't have to sum to 1.0.
      const ws = (box.routing && box.routing.weights) || [];
      const w_inp = document.createElement('input');
      w_inp.type        = 'text';
      w_inp.placeholder = 'e.g. 0.8, 0.2';
      w_inp.value       = ws.join(', ');
      w_inp.style.cssText = 'width:100%;background:#0f1117;border:1px solid #2a2f45;' +
        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
      w_inp.addEventListener('change', () => set_weights(w_inp.value));
      fields.appendChild(mk_row('weights', w_inp));

      const w_note = document.createElement('div');
      w_note.style.cssText = 'font-size:10px;color:#6c72a0;margin-top:4px;';
      w_note.textContent = 'comma-separated non-negative numbers. dispatch normalises to a probability table.';
      fields.appendChild(w_note);
    }
  }
  // }}}

  return { show, hide, show_content, auto_grow_after_set };
})();
