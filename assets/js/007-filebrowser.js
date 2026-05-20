// Source file browser and function signature parser.
// Renders into the inspector panel: file list → function list → auto-populate box ports.

const FileBrowser = (() => {

  // {{{ Parser registry (issue 231)
  // Lazy-loads a per-language signature parser the first time a file
  // of that language is opened. Cached afterwards. Files with an
  // unknown extension or a missing parser fall back to "no parser
  // available" — the file is listed but offers no function picker.
  // Same lazy-import shape as the lexer registry in 008-source-view.js.
  const EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash' };
  const parser_cache = {};   // lang_name → { parse_functions } | null

  async function load_parser(lang) {
    if (lang in parser_cache) return parser_cache[lang];
    try {
      const mod = await import('/langs/' + lang + '/parser.js');
      parser_cache[lang] = mod;
      return mod;
    } catch (e) {
      console.warn('no parser for ' + lang + ':', e.message);
      parser_cache[lang] = null;
      return null;
    }
  }
  // }}}

  // {{{ parse_functions
  // Returns [{name, inputs, outputs, variadic_tail}] for the file's
  // public functions, or null when no parser is available for the
  // extension. Per-language detail lives in langs/<name>/parser.js;
  // this function just dispatches.
  async function parse_functions(content, filename) {
    const ext  = (filename || '').split('.').pop();
    const lang = EXT_TO_LANG[ext];
    if (!lang) return null;
    const mod = await load_parser(lang);
    if (!mod || typeof mod.parse_functions !== 'function') return null;
    try {
      return mod.parse_functions(content);
    } catch (e) {
      console.warn('parser error for ' + filename + ':', e.message);
      return null;
    }
  }
  // }}}

  // {{{ render_fn_list
  // Shows parsed functions for one file; calls on_select(filename, fn) on click.
  function render_fn_list(container, filename, fns, on_select, on_back) {
    container.innerHTML = '';

    const back_btn = document.createElement('button');
    back_btn.className   = 'fb-back';
    back_btn.textContent = '← ' + filename;
    back_btn.onclick     = on_back;
    container.appendChild(back_btn);

    if (fns === null) {
      const note = document.createElement('div');
      note.className   = 'fb-note';
      note.textContent = 'no parser for .' + filename.split('.').pop() + ' files';
      container.appendChild(note);
      return;
    }

    if (fns.length === 0) {
      const note = document.createElement('div');
      note.className   = 'fb-note';
      note.textContent = 'no public functions found (expected: function M.name(...))';
      container.appendChild(note);
      return;
    }

    fns.forEach(fn => {
      const row = document.createElement('div');
      row.className = 'fb-fn-row';

      const name_el = document.createElement('div');
      name_el.className   = 'fb-fn-name';
      name_el.textContent = fn.name + '(' + fn.inputs.join(', ') + ')';
      row.appendChild(name_el);

      const in_el = document.createElement('div');
      in_el.className   = 'fb-fn-ports';
      in_el.textContent = 'in:  ' + (fn.inputs.length ? fn.inputs.join(', ') : '(none)');
      row.appendChild(in_el);

      const out_el = document.createElement('div');
      out_el.className   = 'fb-fn-ports';
      out_el.textContent = 'out: ' + (fn.outputs.length ? fn.outputs.join(', ') : '(none)');
      row.appendChild(out_el);

      row.onclick = () => on_select(filename, fn);
      container.appendChild(row);
    });
  }
  // }}}

  // {{{ show_dir_picker
  // Renders a directory browser into container, navigating the server filesystem.
  // on_select(abs_path) called when the user confirms a directory.
  // on_cancel() called when the user clicks back.
  // initial_path: starting directory; defaults to /home if omitted.
  async function show_dir_picker(container, on_select, on_cancel, initial_path) {
    let current_path = initial_path || '/home';

    const render_picker = async () => {
      container.innerHTML = '<div class="fb-note">loading…</div>';
      let result;
      try {
        result = await API.list_dirs(current_path);
      } catch(e) {
        container.innerHTML = '<div class="fb-note error">error: ' + e.message + '</div>';
        return;
      }

      container.innerHTML = '';

      // back button returns to file list without selecting
      const cancel_btn = document.createElement('button');
      cancel_btn.className   = 'fb-back';
      cancel_btn.textContent = '← cancel';
      cancel_btn.onclick     = on_cancel;
      container.appendChild(cancel_btn);

      // current path breadcrumb
      const path_el = document.createElement('div');
      path_el.style.cssText = 'font-size:10px;color:#4a9eff;word-break:break-all;' +
        'margin-bottom:6px;font-family:monospace;';
      path_el.textContent = result.path;
      container.appendChild(path_el);

      // "use this directory" confirmation button
      const sel_btn = document.createElement('button');
      sel_btn.className   = 'add-port';
      sel_btn.textContent = '✓ use this directory';
      sel_btn.style.cssText = 'display:block;width:100%;margin-bottom:8px;' +
        'color:#4caf7d;border-color:#4caf7d;';
      sel_btn.onclick = () => on_select(result.path);
      container.appendChild(sel_btn);

      // parent directory row (except at filesystem root). It's a
      // directory navigation target, so it gets the dir-row treatment
      // (issue 225).
      if (result.path !== '/') {
        const up_row = document.createElement('div');
        up_row.className   = 'fb-dir-row';
        up_row.textContent = '> ..';
        up_row.onclick = () => {
          // strip last path component
          const parent = result.path.replace(/\/[^/]+$/, '') || '/';
          current_path = parent;
          render_picker();
        };
        container.appendChild(up_row);
      }

      if (result.dirs.length === 0 && (result.files || []).length === 0) {
        const note = document.createElement('div');
        note.className   = 'fb-note';
        note.textContent = '(empty directory)';
        container.appendChild(note);
      }

      // Directories: clickable, navigate on click, prefixed with `>`
      // and styled in the editor's accent blue.
      result.dirs.forEach(name => {
        const row = document.createElement('div');
        row.className   = 'fb-dir-row';
        row.textContent = '> ' + name + '/';
        row.onclick = () => {
          current_path = result.path === '/' ? '/' + name : result.path + '/' + name;
          render_picker();
        };
        container.appendChild(row);
      });

      // Files: read-only confirmation list — these are what the user
      // would import if they pick this directory. Prefixed with `·`,
      // dimmed, no click handler.
      (result.files || []).forEach(name => {
        const row = document.createElement('div');
        row.className   = 'fb-file-readonly';
        row.textContent = '· ' + name;
        container.appendChild(row);
      });
    };

    render_picker();
  }
  // }}}

  // {{{ show_fb_ctx_menu
  // Displays a small floating context menu; reuses .ctx-item CSS from the global stylesheet.
  // items: [{label, action, danger?}]
  function show_fb_ctx_menu(screen_x, screen_y, items) {
    let menu = document.getElementById('fb-ctx-menu');
    if (!menu) {
      menu = document.createElement('div');
      menu.id = 'fb-ctx-menu';
      menu.style.cssText =
        'position:fixed;background:#181c28;border:1px solid #2a2f45;' +
        'border-radius:4px;padding:4px 0;z-index:1001;min-width:150px;' +
        'box-shadow:0 4px 16px #00000066;display:none;';
      document.body.appendChild(menu);
      // close on any click or Escape — attached once at creation
      document.addEventListener('click', () => { menu.style.display = 'none'; });
      document.addEventListener('keydown', e => {
        if (e.key === 'Escape') menu.style.display = 'none';
      });
    }
    menu.innerHTML = '';
    items.forEach(item => {
      const el = document.createElement('div');
      el.className  = item.danger ? 'ctx-item danger' : 'ctx-item';
      el.textContent = item.label;
      // stopPropagation so the document click listener above doesn't immediately close it
      el.addEventListener('click', e => { e.stopPropagation(); menu.style.display = 'none'; item.action(); });
      menu.appendChild(el);
    });
    menu.style.left    = screen_x + 'px';
    menu.style.top     = screen_y + 'px';
    menu.style.display = 'block';
    const r = menu.getBoundingClientRect();
    if (r.right  > window.innerWidth)  menu.style.left = (screen_x - r.width)  + 'px';
    if (r.bottom > window.innerHeight) menu.style.top  = (screen_y - r.height) + 'px';
  }
  // }}}

  // {{{ render
  // Renders the full file browser into container.
  // on_select(filename, fn_obj) called when the user picks a function.
  // Groups: [{label, path?, files:[string], fetch_file: async fn(filename)->text}]
  async function render(container, on_select) {
    container.innerHTML = '<div class="fb-note">loading…</div>';

    // collapsed state: set of group labels
    const collapsed = new Set();

    // all browseable dirs come from one endpoint — src/ is no longer a special case
    let src_dirs = [];
    try {
      src_dirs = await API.list_extra_src().catch(() => []);
    } catch (e) {
      container.innerHTML = '<div class="fb-note error">load error: ' + e.message + '</div>';
      return;
    }

    // build group list — every entry has a path; no exceptions.
    // `kind` (issue 238): "default" = the map's own src/, "added" =
    // user-opted-in via the + library dir button, "bundled" =
    // editor-wide config default. Tag and hide-storage differ by
    // kind below.
    const groups = [];
    (src_dirs || []).forEach(d => {
      if (d.files && d.files.length > 0) {
        groups.push({
          label:      d.label + '/',
          path:       d.path,
          kind:       d.kind || 'added',
          files:      d.files,
          fetch_file: f => API.get_extra_src_file(d.index, f),
        });
      }
    });

    const show_file_list = () => {
      container.innerHTML = '';

      // add-library-dir button
      const add_btn = document.createElement('button');
      add_btn.textContent = '+ library dir';
      add_btn.className   = 'fb-add-dir-btn';
      add_btn.onclick = () => {
        // open picker at parent of last known dir so the user lands near recent work
        const last = groups.length > 0 ? groups[groups.length - 1].path : null;
        const initial = last ? (last.replace(/\/[^/]+$/, '') || '/') : '/home';
        show_dir_picker(container,
          async (path) => {
            try {
              const meta = await API.get_meta();
              // Derive current list from groups, not meta.src_dirs:
              // groups already reflects the server-migrated state
              // (extra_src_dirs + src/), whereas meta.src_dirs is
              // absent on old maps and would discard prior entries.
              // Strip bundled dirs (issue 238) — they live in the
              // server config, not per-map meta.json, and we never
              // want them to migrate into a map's own list.
              const current = groups
                .filter(g => g.kind !== 'bundled')
                .map(g => g.path);
              if (!current.includes(path)) {
                current.push(path);
                await API.put_meta({ ...meta, src_dirs: current });
              }
              render(container, on_select);
            } catch(e) {
              show_file_list();
              const note = document.createElement('div');
              note.className   = 'fb-note error';
              note.textContent = 'could not save dir: ' + e.message;
              container.insertBefore(note, container.firstChild);
            }
          },
          () => show_file_list(),
          initial
        );
      };
      container.appendChild(add_btn);

      if (groups.length === 0) {
        const note = document.createElement('div');
        note.className   = 'fb-note';
        note.textContent = 'src/ is empty — add source files or a library dir';
        container.appendChild(note);
        return;
      }

      groups.forEach(group => {
        // use full path as the collapse key for extra dirs so same-named dirs don't share state
        const collapse_key = group.path || group.label;
        const is_collapsed = collapsed.has(collapse_key);

        // group header row with toggle
        const hdr = document.createElement('div');
        hdr.className = 'fb-group-hdr';

        const toggle = document.createElement('span');
        toggle.className   = 'fb-group-toggle';
        toggle.textContent = is_collapsed ? '▶' : '▼';
        hdr.appendChild(toggle);

        const lbl = document.createElement('span');
        lbl.className   = 'fb-header';
        lbl.textContent = group.label;
        hdr.appendChild(lbl);

        // The "bundled" tag marks dirs that ship with the map
        // automatically — the map's own src/ and the editor-wide
        // config defaults. User-added dirs (kind === 'added') stay
        // untagged so the user can tell at a glance what they
        // explicitly attached vs what is here by default.
        if (group.kind === 'default' || group.kind === 'bundled') {
          const tag = document.createElement('span');
          tag.textContent = ' bundled';
          tag.style.cssText = 'font-size:9px;color:#4caf7d;margin-left:6px;' +
            'border:1px solid #4caf7d;padding:0 4px;border-radius:2px;';
          hdr.appendChild(tag);
        }

        hdr.onclick = () => {
          if (collapsed.has(collapse_key)) collapsed.delete(collapse_key);
          else collapsed.add(collapse_key);
          show_file_list();
        };

        // Right-click → "hide directory" works on every group. The
        // storage differs by kind: map dirs come out of src_dirs;
        // editor-wide bundled dirs are recorded in
        // hidden_bundled_dirs (an opt-out list the server checks
        // when merging in the config defaults). The user
        // experiences both as the same action.
        hdr.addEventListener('contextmenu', e => {
          e.preventDefault();
          show_fb_ctx_menu(e.clientX, e.clientY, [{
            label:  'hide directory',
            danger: true,
            action: async () => {
              try {
                const meta = await API.get_meta();
                let patch;
                if (group.kind === 'bundled') {
                  // record the opt-out; the server filters this
                  // path out of the bundled list on next load
                  const hidden = Array.isArray(meta.hidden_bundled_dirs)
                    ? meta.hidden_bundled_dirs.slice() : [];
                  if (!hidden.includes(group.path)) hidden.push(group.path);
                  patch = { ...meta, hidden_bundled_dirs: hidden };
                } else {
                  // drop from src_dirs; bundled paths never bleed
                  // into the per-map list. Default and added live
                  // in src_dirs together, so both go through here.
                  const dirs = groups
                    .filter(g => g.kind !== 'bundled' && g.path !== group.path)
                    .map(g => g.path);
                  patch = { ...meta, src_dirs: dirs };
                }
                await API.put_meta(patch);
                render(container, on_select);
              } catch (err) {
                show_file_list();
                const note = document.createElement('div');
                note.className   = 'fb-note error';
                note.textContent = 'could not hide dir: ' + err.message;
                container.insertBefore(note, container.firstChild);
              }
            },
          }]);
        });

        container.appendChild(hdr);

        if (is_collapsed) return;

        group.files.forEach(filename => {
          const row = document.createElement('div');
          row.className   = 'fb-file-row';
          // Prefix matches the dir picker's `·` for files so the same
          // glyph means the same thing across both views (issue 225).
          row.textContent = '· ' + filename;
          row.onclick     = async () => {
            row.textContent = filename + ' …';
            let content;
            try {
              content = await group.fetch_file(filename);
            } catch (e) {
              container.innerHTML = '<div class="fb-note error">could not read ' + filename + ': ' + e.message + '</div>';
              show_file_list();
              return;
            }
            const fns = await parse_functions(content, filename);
            render_fn_list(container, group.label + filename, fns,
              (f, fn) => on_select(f, fn),
              show_file_list
            );
          };
          container.appendChild(row);
        });
      });
    };

    show_file_list();
  }
  // }}}

  // {{{ render_at_fn_list
  // Public entry point used by the inspector's "fn" button (issue
  // 224 follow-up). Jumps straight to the function picker for the
  // file at `ref`, with a back button that unwinds to the full
  // browser. Useful when the user wants to pick a different function
  // from the same file without re-walking the directory tree.
  async function render_at_fn_list(container, ref, on_select) {
    if (!ref) {
      // No ref to land on — open the full browser instead.
      return render(container, on_select);
    }

    container.innerHTML = '<div class="fb-note">loading…</div>';
    const stripped = ref.replace(/^src\//, '');

    let content = null;
    let error_msg = null;

    // Mirror fetch_source's resolution order: main src/ first, then
    // each extra dir in turn. The first successful fetch wins.
    try {
      content = await API.get_src_file(stripped);
    } catch (e) { /* try extras */ }

    if (content === null) {
      let extras = [];
      try { extras = await API.list_extra_src(); } catch (e) {}
      for (let i = 0; i < (extras || []).length; i++) {
        try { content = await API.get_extra_src_file(i, stripped); break; }
        catch (e) {}
      }
    }

    if (content === null) {
      container.innerHTML = '';
      const note = document.createElement('div');
      note.className   = 'fb-note error';
      note.textContent = 'source not found: ' + ref;
      container.appendChild(note);
      const back = document.createElement('button');
      back.className   = 'fb-back';
      back.textContent = '← browse';
      back.onclick     = () => render(container, on_select);
      container.appendChild(back);
      return;
    }

    const fns = await parse_functions(content, stripped);
    render_fn_list(container, stripped, fns,
      (f, fn) => on_select(f, fn),
      () => render(container, on_select));
  }
  // }}}

  return { parse_functions, render, render_at_fn_list };
})();
