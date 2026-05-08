// Source file browser and function signature parser.
// Renders into the inspector panel: file list → function list → auto-populate box ports.

const FileBrowser = (() => {

  // {{{ parse_lua_returns
  // Scans a function body for return statements; returns an array of output names.
  function parse_lua_returns(body) {
    const re    = /\breturn\b\s*([^\n]+)/g;
    const names = new Set();
    let m;
    while ((m = re.exec(body)) !== null) {
      const expr = m[1].replace(/--.*$/, '').trim();
      if (!expr) continue;
      expr.split(',').forEach((part, i) => {
        part = part.trim();
        // bare identifier → use as port name
        if (/^[a-zA-Z_]\w*$/.test(part) &&
            !['nil','true','false','not','and','or','end'].includes(part)) {
          names.add(part);
        } else if (expr.split(',').length === 1 && part) {
          // single complex expression → call it "result"
          names.add('result');
        }
        // multi-value complex expression: skip unnamed parts
      });
    }
    return [...names];
  }
  // }}}

  // {{{ parse_lua
  function parse_lua(content) {
    const fns    = [];
    const fn_re  = /^function\s+M\.(\w+)\s*\(([^)]*)\)/mg;
    const starts = [];
    let m;
    while ((m = fn_re.exec(content)) !== null) {
      starts.push({ name: m[1], args: m[2], body_start: m.index + m[0].length });
    }
    starts.forEach((fn, i) => {
      const body_end = i + 1 < starts.length ? starts[i + 1].body_start : content.length;
      const body     = content.slice(fn.body_start, body_end);
      const inputs   = fn.args.trim()
        ? fn.args.split(',').map(s => s.trim()).filter(Boolean)
        : [];
      const outputs  = parse_lua_returns(body);
      fns.push({ name: fn.name, inputs, outputs });
    });
    return fns;
  }
  // }}}

  // {{{ parse_bash_inputs
  function parse_bash_inputs(body) {
    const params = {};
    // local var="${N}" or local var="$N"
    const local_re = /local\s+(\w+)\s*=\s*["']?\$\{?(\d+)\}?["']?/g;
    let m;
    while ((m = local_re.exec(body)) !== null) {
      params[parseInt(m[2])] = m[1];
    }
    if (Object.keys(params).length > 0) {
      const max = Math.max(...Object.keys(params).map(Number));
      return Array.from({ length: max }, (_, i) => params[i + 1] || `arg${i + 1}`);
    }
    // fall back: scan for raw $N references
    const seen = new Set();
    const pos_re = /\$\{?(\d+)\}?/g;
    while ((m = pos_re.exec(body)) !== null) {
      const n = parseInt(m[1]);
      if (n > 0) seen.add(n);
    }
    if (seen.size > 0) {
      const max = Math.max(...seen);
      return Array.from({ length: max }, (_, i) => `arg${i + 1}`);
    }
    return [];
  }
  // }}}

  // {{{ parse_bash
  function parse_bash(content) {
    const fns   = [];
    const re    = /^(\w+)\s*\(\s*\)\s*\{/mg;
    const skip  = new Set(['if','while','for','case','do','done','fi','then','else']);
    let m;
    while ((m = re.exec(content)) !== null) {
      const name = m[1];
      if (skip.has(name)) continue;
      const body_start = m.index + m[0].length;
      const close      = content.indexOf('\n}', body_start);
      const body       = content.slice(body_start, close > -1 ? close : body_start + 800);
      const inputs     = parse_bash_inputs(body);
      fns.push({ name, inputs, outputs: ['output'] });
    }
    return fns;
  }
  // }}}

  // {{{ parse_functions
  // Returns [{name, inputs:[string], outputs:[string]}] for a given file.
  function parse_functions(content, filename) {
    const ext = filename.split('.').pop();
    if (ext === 'lua') return parse_lua(content);
    if (ext === 'sh')  return parse_bash(content);
    return null;  // null = no parser for this extension
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

  // {{{ render
  // Renders the full file browser into container.
  // on_select(filename, fn_obj) called when the user picks a function.
  async function render(container, on_select) {
    container.innerHTML = '<div class="fb-note">loading src/…</div>';

    let files;
    try {
      files = await API.list_src_files();
    } catch (e) {
      container.innerHTML = '<div class="fb-note error">could not list src/: ' + e.message + '</div>';
      return;
    }

    if (!files || files.length === 0) {
      container.innerHTML = '<div class="fb-note">src/ is empty — add source files to the map</div>';
      return;
    }

    // show file list
    const show_file_list = () => {
      container.innerHTML = '';
      const hdr = document.createElement('div');
      hdr.className   = 'fb-header';
      hdr.textContent = 'src/';
      container.appendChild(hdr);

      files.forEach(filename => {
        const row = document.createElement('div');
        row.className   = 'fb-file-row';
        row.textContent = filename;
        row.onclick     = async () => {
          row.textContent = filename + ' …';
          let content;
          try {
            content = await API.get_src_file(filename);
          } catch (e) {
            container.innerHTML = '<div class="fb-note error">could not read ' + filename + ': ' + e.message + '</div>';
            show_file_list();
            return;
          }
          const fns = parse_functions(content, filename);
          render_fn_list(container, filename, fns,
            (f, fn) => on_select(f, fn),
            show_file_list
          );
        };
        container.appendChild(row);
      });
    };

    show_file_list();
  }
  // }}}

  return { parse_functions, render };
})();
