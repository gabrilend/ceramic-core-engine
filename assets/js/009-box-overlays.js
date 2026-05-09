// DOM overlays for editable input port names (issue 224).
//
// The canvas renders the box body and port dots; this module floats
// HTML <input> elements next to each input port so the user can
// rename them inline. Names used to be read-only labels (issue 208);
// the rationale for that decision was preventing typo drift between
// source code and box JSON, but the file browser still re-populates
// names on every re-pick, and a typo now produces an explicit
// runtime error rather than a silent skip — so editability is back.
//
// Literal values stay in the inspector's port display; only the
// name lives on the canvas.
//
// Lifecycle:
//   - redraw() runs in the main render loop after Boxes.draw_all,
//     so positions stay aligned with the camera and any box drag.
//   - Per-box overlays are created lazily on first sight and torn
//     down when the box leaves Boxes.boxes.
//   - Per-port rows rebuild whenever the port count changes
//     (variadic add/remove); otherwise their <input> elements
//     persist so user typing isn't clobbered between frames.
//   - Updates skip any input that currently has focus, again to
//     avoid stomping on in-progress edits.

const Overlays = (() => {
  // overlays: { [box_id]: { container, rows: [{ row, name_inp, port_idx }] } }
  const overlays = {};
  let root = null;

  // {{{ ensure_root
  function ensure_root() {
    if (root) return;
    root = document.getElementById('box-overlays');
    if (!root) {
      root = document.createElement('div');
      root.id = 'box-overlays';
      // The canvas takes the entire viewport; the overlay layer sits
      // on top of it. pointer-events:none lets canvas clicks pass
      // through whitespace; individual <input>s opt back in.
      root.style.cssText = 'position:absolute;top:0;left:0;width:100%;' +
        'height:100%;pointer-events:none;overflow:hidden;';
      // Layer inside the same wrapper as the canvas so it rides
      // along with any future canvas-wrap repositioning. The wrapper
      // is the one with #canvas-wrap; appending puts the overlay on
      // top of the canvas in z-order.
      const wrap = document.getElementById('canvas-wrap');
      wrap.appendChild(root);
    }
  }
  // }}}

  // {{{ build_row
  // Each row is one editable <input> for a single port name.
  // commit-on-blur (and Enter) — typing alone doesn't fire the
  // rename, so users can clear-and-retype without triggering wire
  // rewrites mid-edit.
  function build_row(box, port_idx) {
    const row = document.createElement('div');
    row.className = 'box-overlay-row';

    const name_inp = document.createElement('input');
    name_inp.type      = 'text';
    name_inp.className = 'box-overlay-name';
    name_inp.value     = (box.inputs[port_idx] && box.inputs[port_idx].name) || '';

    const commit = async () => {
      const new_name = name_inp.value.trim();
      const ok = await Inspector.rename_port(box, port_idx, new_name);
      if (!ok) {
        // restore to the box's actual name (which rename_port left
        // unchanged on rejection) so the field doesn't keep
        // displaying the rejected text
        name_inp.value = (box.inputs[port_idx] && box.inputs[port_idx].name) || '';
      }
    };
    name_inp.addEventListener('blur', commit);
    name_inp.addEventListener('keydown', e => {
      if (e.key === 'Enter')   { e.preventDefault(); name_inp.blur(); }
      if (e.key === 'Escape')  {
        name_inp.value = (box.inputs[port_idx] && box.inputs[port_idx].name) || '';
        name_inp.blur();
      }
    });

    row.appendChild(name_inp);
    return { row, name_inp };
  }
  // }}}

  // {{{ ensure_overlay
  function ensure_overlay(box) {
    let ov = overlays[box.id];
    if (!ov) {
      const container = document.createElement('div');
      container.className = 'box-overlay';
      ov = { container, rows: [] };
      overlays[box.id] = ov;
      root.appendChild(container);
    }
    const inputs = box.inputs || [];
    if (ov.rows.length !== inputs.length) {
      // Port count changed — rebuild rows. Cheap (small N), and it
      // skips the bookkeeping of which slots got renamed/removed.
      ov.container.innerHTML = '';
      ov.rows = inputs.map((_, i) => {
        const r = build_row(box, i);
        ov.container.appendChild(r.row);
        return { ...r, port_idx: i };
      });
    } else {
      // Same shape, possibly different names — sync values for
      // inputs that aren't focused (don't clobber in-progress edits).
      ov.rows.forEach((r, i) => {
        if (document.activeElement !== r.name_inp) {
          r.name_inp.value = inputs[i].name || '';
        }
      });
    }
    return ov;
  }
  // }}}

  // {{{ position_overlay
  // The overlay isn't a single transformed element — each port row
  // is positioned individually so the <input> sizing stays in
  // screen pixels regardless of zoom (consistent with most node
  // editors).
  function position_overlay(box, ov) {
    const inputs = box.inputs || [];
    inputs.forEach((p, i) => {
      const screen = Boxes.get_port_world_pos(box.id, p.name, 'input');
      if (!screen) return;
      const s = Canvas.world_to_screen(screen.x, screen.y);
      const row = ov.rows[i].row;
      // dot + 6px gap; vertically center the input on the dot
      row.style.left = (s.x + 10) + 'px';
      row.style.top  = (s.y - 9)  + 'px';
    });
  }
  // }}}

  // {{{ redraw
  // Called from the main render loop. Reconciles overlay set with
  // Boxes.boxes, then repositions every visible row.
  function redraw() {
    ensure_root();

    const live = new Set();
    for (const id in Boxes.boxes) {
      live.add(id);
      const box = Boxes.boxes[id];
      const ov  = ensure_overlay(box);
      position_overlay(box, ov);
    }
    for (const id in overlays) {
      if (!live.has(id)) {
        overlays[id].container.remove();
        delete overlays[id];
      }
    }
  }
  // }}}

  // {{{ remove
  // Called when a box is deleted explicitly so the overlay is gone
  // before the next frame catches up via redraw's reconciliation.
  function remove(box_id) {
    if (overlays[box_id]) {
      overlays[box_id].container.remove();
      delete overlays[box_id];
    }
  }
  // }}}

  return { redraw, remove };
})();
