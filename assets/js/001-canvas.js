// Canvas viewport: pan + zoom, and the per-frame draw transform.
//
// Coordinate model
//   screen pixel (sx, sy)  ←→  world point (wx, wy)
//     sx = wx * zoom + pan_x
//     wx = (sx - pan_x) / zoom
//
// `pan_x` / `pan_y` are screen-pixel offsets passed to ctx.translate
// AFTER scaling: the world is scaled around its origin first, then the
// result is shifted on screen. This is simpler than a world-space camera
// because every operation is either pure-screen-pixel arithmetic
// (panning, mouse positions) or pure-world arithmetic (drawing). They
// only meet in two functions: world_to_screen and screen_to_world.

const Canvas = (() => {
  const el  = document.getElementById('canvas');
  const ctx = el.getContext('2d');

  let zoom  = 1;
  let pan_x = 0;
  let pan_y = 0;

  let dirty = true;

  // {{{ resize
  function resize() {
    el.width  = window.innerWidth  - (document.getElementById('sidebar').offsetWidth || 0);
    el.height = window.innerHeight;
    dirty = true;
  }
  // }}}

  // {{{ world_to_screen
  function world_to_screen(wx, wy) {
    return { x: wx * zoom + pan_x, y: wy * zoom + pan_y };
  }
  // }}}

  // {{{ screen_to_world
  function screen_to_world(sx, sy) {
    return { x: (sx - pan_x) / zoom, y: (sy - pan_y) / zoom };
  }
  // }}}

  // {{{ mark_dirty
  function mark_dirty() { dirty = true; }
  // }}}

  // {{{ start_frame
  // Reset the transform, clear, then set up the world-drawing transform.
  // The explicit setTransform reset is defensive — guarantees no leftover
  // matrix state can affect this frame even if a previous restore was missed.
  function start_frame() {
    if (!dirty) return false;
    dirty = false;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, el.width, el.height);
    ctx.save();
    ctx.translate(pan_x, pan_y);
    ctx.scale(zoom, zoom);
    return true;
  }
  // }}}

  // {{{ end_frame
  function end_frame() { ctx.restore(); }
  // }}}

  // {{{ mouse_pos
  function mouse_pos(e) {
    const r = el.getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top };
  }
  // }}}

  // -- Pan: middle-click drag, or space + left-click drag.
  // Pan math is in screen pixels: pan_{x,y} += mouse_delta_in_screen_pixels.
  // The pan anchors record the state at mousedown so each mousemove
  // computes the pan from the original anchor, not incrementally — that
  // way no error accumulates over a long drag.
  let panning          = false;
  let pan_anchor_mouse = null;
  let pan_anchor_pan   = null;

  let space_down = false;
  window.addEventListener('keydown', e => { if (e.code === 'Space') space_down = true;  });
  window.addEventListener('keyup',   e => { if (e.code === 'Space') space_down = false; });

  el.addEventListener('mousedown', e => {
    if (e.button === 1 || (e.button === 0 && space_down)) {
      panning          = true;
      pan_anchor_mouse = mouse_pos(e);
      pan_anchor_pan   = { x: pan_x, y: pan_y };
      e.preventDefault();
    }
  });

  window.addEventListener('mousemove', e => {
    if (!panning) return;
    const m = mouse_pos(e);
    pan_x = pan_anchor_pan.x + (m.x - pan_anchor_mouse.x);
    pan_y = pan_anchor_pan.y + (m.y - pan_anchor_mouse.y);
    mark_dirty();
  });

  window.addEventListener('mouseup', () => { panning = false; });

  // -- Zoom: scroll wheel, anchored on the cursor.
  // The world point under the cursor stays under the cursor across zoom
  // changes. After updating zoom we solve the screen-equation for the
  // new pan offset:
  //     m.x = w.x * zoom + pan_x   →   pan_x = m.x - w.x * zoom
  el.addEventListener('wheel', e => {
    e.preventDefault();
    // Ignore wheel input while a pan drag is in progress. Middle-click
    // pans tend to nudge the wheel as a side effect; without this guard
    // the canvas zooms unexpectedly mid-pan.
    if (panning) return;

    const m = mouse_pos(e);
    const w = screen_to_world(m.x, m.y);

    // Direction by sign of deltaY: up (negative) zooms in, down zooms out.
    const factor   = e.deltaY < 0 ? 1.1 : 0.9;
    const new_zoom = Math.max(0.15, Math.min(5, zoom * factor));
    zoom = new_zoom;

    pan_x = m.x - w.x * zoom;
    pan_y = m.y - w.y * zoom;
    mark_dirty();
  }, { passive: false });

  window.addEventListener('resize', resize);
  resize();

  // Backward-compatible `cam` accessor. External modules read `cam.zoom`
  // for hit-test tolerance scaling. `cam.x` / `cam.y` here expose the new
  // screen-pixel pan offsets; that's a semantic change from the previous
  // world-space camera, but no current consumer outside this module reads
  // those two — only `cam.zoom`.
  const cam = {
    get x()     { return pan_x; },
    set x(v)    { pan_x = v; mark_dirty(); },
    get y()     { return pan_y; },
    set y(v)    { pan_y = v; mark_dirty(); },
    get zoom()  { return zoom;  },
    set zoom(v) { zoom  = v;  mark_dirty(); },
  };

  return { el, ctx, cam, world_to_screen, screen_to_world, mark_dirty,
           start_frame, end_frame, mouse_pos };
})();
