// Camera state and canvas management.
// Owns pan/zoom, the render loop, and coordinate conversion.

const Canvas = (() => {
  const el  = document.getElementById('canvas');
  const ctx = el.getContext('2d');

  const cam = { x: 0, y: 0, zoom: 1 };
  let dirty = true;
  let dragging_cam = false;
  let drag_start   = null;

  // {{{ resize
  function resize() {
    el.width  = window.innerWidth  - (document.getElementById('sidebar').offsetWidth || 0);
    el.height = window.innerHeight;
    dirty = true;
  }
  // }}}

  // {{{ world_to_screen
  function world_to_screen(wx, wy) {
    return {
      x: (wx + cam.x) * cam.zoom,
      y: (wy + cam.y) * cam.zoom,
    };
  }
  // }}}

  // {{{ screen_to_world
  function screen_to_world(sx, sy) {
    return {
      x: sx / cam.zoom - cam.x,
      y: sy / cam.zoom - cam.y,
    };
  }
  // }}}

  // {{{ mark_dirty
  function mark_dirty() { dirty = true; }
  // }}}

  // {{{ start_frame
  // Called by the render loop; returns true if a redraw is needed.
  function start_frame() {
    if (!dirty) return false;
    dirty = false;
    ctx.clearRect(0, 0, el.width, el.height);
    ctx.save();
    ctx.scale(cam.zoom, cam.zoom);
    ctx.translate(cam.x, cam.y);
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

  // Panning via middle-mouse or space+drag
  let space_down = false;
  window.addEventListener('keydown', e => { if (e.code === 'Space') space_down = true; });
  window.addEventListener('keyup',   e => { if (e.code === 'Space') space_down = false; });

  el.addEventListener('mousedown', e => {
    if (e.button === 1 || (e.button === 0 && space_down)) {
      dragging_cam = true;
      drag_start   = { x: e.clientX - cam.x * cam.zoom,
                       y: e.clientY - cam.y * cam.zoom };
      e.preventDefault();
    }
  });

  window.addEventListener('mousemove', e => {
    if (dragging_cam && drag_start) {
      cam.x = (e.clientX - drag_start.x) / cam.zoom;
      cam.y = (e.clientY - drag_start.y) / cam.zoom;
      mark_dirty();
    }
  });

  window.addEventListener('mouseup', e => {
    if (e.button === 1 || e.button === 0) dragging_cam = false;
  });

  // Zoom via scroll wheel
  el.addEventListener('wheel', e => {
    e.preventDefault();
    const m   = mouse_pos(e);
    const wbefore = screen_to_world(m.x, m.y);
    const factor  = e.deltaY < 0 ? 1.1 : 0.9;
    cam.zoom = Math.max(0.15, Math.min(5, cam.zoom * factor));
    const wafter = screen_to_world(m.x, m.y);
    cam.x += wafter.x - wbefore.x;
    cam.y += wafter.y - wbefore.y;
    mark_dirty();
  }, { passive: false });

  window.addEventListener('resize', resize);
  resize();

  return { el, ctx, cam, world_to_screen, screen_to_world, mark_dirty,
           start_frame, end_frame, mouse_pos };
})();
