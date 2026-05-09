// Floating, draggable, resizable, scrollable read-only source viewer.
//
// open_source_view(ref, anchor_box?) creates an absolutely-positioned
// window over the canvas showing the file's content. Multiple windows
// can be open simultaneously; each is independently positioned and
// closes only when its × button is clicked.
//
// The window is confined to the canvas's bounding box: drag and resize
// operations clamp position and dimensions so the window can't leave
// the canvas area.
//
// Default sizing (per issue 215):
//   width  = (average line length + 3 chars) × monospace char width
//   height = canvas height / 2

const SourceView = (() => {
  // Track open windows so we can bring-to-front on click and so a future
  // close-all (e.g. on map switch) can find them. Each entry is the
  // outer DOM element.
  const open_windows = [];
  let z_top = 100;

  // {{{ measure_char_w
  // Measure the width of a single 12px-monospace character once at module
  // load. Used for translating "characters" into pixels for default sizing.
  // The measurement node is removed after use; no DOM clutter.
  function measure_char_w() {
    const probe = document.createElement('span');
    probe.style.cssText =
      'visibility:hidden;position:absolute;font:12px monospace;' +
      'white-space:pre;left:-9999px;top:-9999px;';
    probe.textContent = 'M'.repeat(50);
    document.body.appendChild(probe);
    const w = probe.getBoundingClientRect().width / 50;
    probe.remove();
    return w;
  }
  const CHAR_W = measure_char_w();
  // }}}

  // {{{ Lexer registry (issue 223)
  // Lazy-loads a per-language tokenizer the first time a viewer asks
  // for that language. Cached afterwards. Files with an unknown
  // extension or a missing lexer fall back to plain rendering.
  const EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash', c: 'c', h: 'c' };
  const lexer_cache = {};   // lang_name → { tokenize } | null (no lexer)

  async function load_lexer(lang) {
    if (lang in lexer_cache) return lexer_cache[lang];
    try {
      const mod = await import('/langs/' + lang + '/lexer.js');
      lexer_cache[lang] = mod;
      return mod;
    } catch (e) {
      console.warn('no lexer for ' + lang + ':', e.message);
      lexer_cache[lang] = null;
      return null;
    }
  }

  // Render `text` into `body` with syntax-highlighted spans. Tokens
  // are gap-free per the lexer contract; we just walk them and emit
  // one span per token. Plain rendering (no spans) is the fallback
  // when the lexer can't be loaded or the file extension is unknown.
  function render_with_lexer(body, text, tokens) {
    if (!tokens) {
      body.textContent = text;
      return;
    }
    body.innerHTML = '';
    for (const t of tokens) {
      const span = document.createElement('span');
      span.className   = 'syntax-' + t.type;
      span.textContent = text.slice(t.start, t.end);
      body.appendChild(span);
    }
  }

  function ext_of(ref) {
    const m = /\.([^./]+)$/.exec(ref || '');
    return m ? m[1].toLowerCase() : '';
  }
  // }}}

  // {{{ fetch_source
  // Resolve a box's `ref` to file content. The ref is just a filename or a
  // path relative to the map; we try the map's main src/ first (stripping
  // a leading "src/" that older boxes sometimes carry), then walk through
  // any extra source directories the server has registered. Throws if
  // nothing matches.
  async function fetch_source(ref) {
    if (!ref) throw new Error('no ref set on this box');

    const stripped = ref.replace(/^src\//, '');

    try { return await API.get_src_file(stripped); } catch (e) { /* fall through */ }

    let extras = [];
    try { extras = await API.list_extra_src(); } catch (e) {}
    for (let i = 0; i < (extras || []).length; i++) {
      try { return await API.get_extra_src_file(i, stripped); } catch (e) {}
    }

    throw new Error('source not found: ' + ref);
  }
  // }}}

  // {{{ canvas_rect
  // Bounding box of the canvas in viewport coordinates. Used as the clamp
  // region for the floating window.
  function canvas_rect() {
    return Canvas.el.getBoundingClientRect();
  }
  // }}}

  // {{{ default_size
  // Returns the default { w, h } for a window showing the given source
  // text. Width is the file's longest line clamped to [80, 120] chars —
  // tiny files don't get pointlessly small windows, big files don't get
  // pointlessly huge ones, and the user never sees horizontal scroll
  // for content that already fits in 120 columns. Height is half the
  // canvas height; the user resizes if they want more.
  //
  // Average line length was the first attempt and got pulled way down
  // by blank lines and short single-keyword lines, so most files
  // opened too narrow.
  function default_size(text) {
    const lines   = (text || '').split('\n');
    const max_len = lines.reduce((m, l) => Math.max(m, l.length), 0);
    const w_chars = Math.max(80, Math.min(120, max_len));
    const w       = w_chars * CHAR_W + 20;  // +20 for padding
    const h       = canvas_rect().height / 2;
    return { w: Math.round(w), h: Math.round(h) };
  }
  // }}}

  // {{{ clamp_position
  // Clamp a candidate (left, top, width, height) so the window stays
  // fully inside the canvas's bounding box.
  function clamp_position(left, top, width, height) {
    const r = canvas_rect();
    const max_left = r.right  - width;
    const max_top  = r.bottom - height;
    return {
      left: Math.min(Math.max(left, r.left), Math.max(r.left, max_left)),
      top:  Math.min(Math.max(top,  r.top),  Math.max(r.top,  max_top)),
    };
  }
  // }}}

  // {{{ bring_to_front
  function bring_to_front(win) {
    z_top += 1;
    win.style.zIndex = String(z_top);
  }
  // }}}

  // {{{ make_window
  // Create the outer DOM structure for a source window with the given
  // title and body text. Wires up the drag-from-header behavior and the
  // close button. Returns the outer element; caller positions and sizes
  // it.
  function make_window(title, body_text) {
    const wrap = document.createElement('div');
    wrap.className = 'source-view-window';

    const hdr = document.createElement('div');
    hdr.className = 'source-view-header';

    const lbl = document.createElement('span');
    lbl.className   = 'source-view-title';
    lbl.textContent = title;
    hdr.appendChild(lbl);

    const close_btn = document.createElement('button');
    close_btn.className   = 'source-view-close';
    close_btn.textContent = '×';
    close_btn.title       = 'close';
    close_btn.onclick     = () => {
      const i = open_windows.indexOf(wrap);
      if (i >= 0) open_windows.splice(i, 1);
      wrap.remove();
    };
    hdr.appendChild(close_btn);

    wrap.appendChild(hdr);

    const body = document.createElement('pre');
    body.className   = 'source-view-body';
    // Plain text first as a guaranteed-correct baseline; the lexer
    // pass below replaces it asynchronously if a tokenizer is
    // available for the file's extension (issue 223).
    body.textContent = body_text;
    const lang = EXT_TO_LANG[ext_of(title)];
    if (lang) {
      load_lexer(lang).then(mod => {
        if (!mod || !mod.tokenize) return;
        try {
          render_with_lexer(body, body_text, mod.tokenize(body_text));
        } catch (e) {
          console.warn('lexer ' + lang + ' failed:', e.message);
        }
      });
    }
    wrap.appendChild(body);

    // Drag-from-header: anchor at mousedown, every mousemove computes
    // from the anchor to avoid accumulating error over a long drag.
    let drag_anchor_mouse = null;
    let drag_anchor_pos   = null;

    hdr.addEventListener('mousedown', e => {
      // Don't start a drag from the close button.
      if (e.target === close_btn) return;
      drag_anchor_mouse = { x: e.clientX, y: e.clientY };
      const r = wrap.getBoundingClientRect();
      drag_anchor_pos   = { left: r.left, top: r.top };
      bring_to_front(wrap);
      e.preventDefault();
    });

    window.addEventListener('mousemove', e => {
      if (!drag_anchor_mouse) return;
      const cand_left = drag_anchor_pos.left + (e.clientX - drag_anchor_mouse.x);
      const cand_top  = drag_anchor_pos.top  + (e.clientY - drag_anchor_mouse.y);
      const r         = wrap.getBoundingClientRect();
      const clamped   = clamp_position(cand_left, cand_top, r.width, r.height);
      wrap.style.left = clamped.left + 'px';
      wrap.style.top  = clamped.top  + 'px';
    });

    window.addEventListener('mouseup', () => { drag_anchor_mouse = null; });

    // Click anywhere on the window brings it to the front.
    wrap.addEventListener('mousedown', () => bring_to_front(wrap));

    // Resize handle: native CSS resize=both, but we observe size changes
    // and re-clamp position so the window doesn't grow off-canvas.
    const ro = new ResizeObserver(() => {
      const r = wrap.getBoundingClientRect();
      const c = clamp_position(r.left, r.top, r.width, r.height);
      // Cap maximum size at the canvas bounds.
      const cr = canvas_rect();
      const max_w = cr.width;
      const max_h = cr.height;
      if (r.width  > max_w) wrap.style.width  = max_w + 'px';
      if (r.height > max_h) wrap.style.height = max_h + 'px';
      wrap.style.left = c.left + 'px';
      wrap.style.top  = c.top  + 'px';
    });
    ro.observe(wrap);

    return wrap;
  }
  // }}}

  // {{{ open_source_view
  // Public entry point. Fetches the source for `ref` and opens a floating
  // window. If `anchor_box` is given, the window is centered over that
  // box's screen position; otherwise it's centered on the canvas.
  async function open_source_view(ref, anchor_box) {
    let text;
    try {
      text = await fetch_source(ref);
    } catch (e) {
      status_msg('view source: ' + e.message, 'error');
      return;
    }

    const win  = make_window(ref, text);
    const size = default_size(text);

    document.body.appendChild(win);
    win.style.width  = size.w + 'px';
    win.style.height = size.h + 'px';

    // Default position: centered over anchor_box (if provided) or canvas.
    const cr = canvas_rect();
    let cand_left, cand_top;
    if (anchor_box && anchor_box.ui) {
      const s = Canvas.world_to_screen(anchor_box.ui.x + Boxes.BOX_W / 2,
                                       anchor_box.ui.y + Boxes.box_height(anchor_box) / 2);
      cand_left = cr.left + s.x - size.w / 2;
      cand_top  = cr.top  + s.y - size.h / 2;
    } else {
      cand_left = cr.left + cr.width  / 2 - size.w / 2;
      cand_top  = cr.top  + cr.height / 2 - size.h / 2;
    }
    const clamped = clamp_position(cand_left, cand_top, size.w, size.h);
    win.style.left = clamped.left + 'px';
    win.style.top  = clamped.top  + 'px';

    bring_to_front(win);
    open_windows.push(win);
  }
  // }}}

  return { open_source_view };
})();
