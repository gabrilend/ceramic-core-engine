// langs/lua/parser.js — Lua signature parser for the editor's file
// browser. Issue 231 moved this code out of assets/js/007-filebrowser.js
// so per-language knowledge lives next to the rest of each language's
// pieces (spec.c, Makefile, lexer.js).
//
// The contract is one ES-module export, `parse_functions(content)`,
// returning an array of records describing every public function the
// browser should offer. The file browser does the rest — listing,
// click-to-pick, port population.

// {{{ lua_fn_end
// Returns the index of the closing 'end' / 'until' that matches the
// function body starting at body_start. Tracks nesting depth by
// counting block openers and closers. Does not attempt to skip string
// literals — good enough for well-formatted source.
function lua_fn_end(content, body_start) {
  const tok = /\b(function|if|while|for|repeat|do|end|until)\b/g;
  tok.lastIndex = body_start;
  let depth = 1;
  let m;
  while ((m = tok.exec(content)) !== null) {
    const kw = m[1];
    if (kw === 'end' || kw === 'until') {
      depth--;
      if (depth === 0) return m.index;
    } else {
      depth++;
    }
  }
  return content.length;
}
// }}}

// {{{ parse_returns
// Scans a function body for `return` statements; returns an array of
// output names. Bare identifiers in the return list become port names;
// any complex expression collapses to a single "result" port.
function parse_returns(body) {
  const re    = /\breturn\b\s*([^\n]+)/g;
  const names = new Set();
  let m;
  while ((m = re.exec(body)) !== null) {
    const expr = m[1].replace(/--.*$/, '').trim();
    if (!expr) continue;
    if (/["']|\.\./.test(expr)) { names.add('result'); continue; }
    const parts = expr.split(',');
    parts.forEach(part => {
      part = part.trim();
      if (/^[a-zA-Z_]\w*$/.test(part) &&
          !['nil','true','false','not','and','or','end'].includes(part)) {
        names.add(part);
      } else if (parts.length === 1 && part) {
        names.add('result');
      }
    });
  }
  return [...names];
}
// }}}

// {{{ parse_functions
// Returns [{name, inputs, outputs}] for every `function M.<name>(<params>)`
// declared in the file.
//
// A trailing `...` in the parameter list is stripped from `inputs` —
// the editor's port list shows only the named parameters. Whether the
// language *accepts* extra positional args at call time is not a
// per-function property and not the parser's job; that's declared
// once per language in `langs/<name>/spec.js` via `variadic_shape`.
// The function's signature still drives the displayed port names so
// the user can see the arity the author wrote.
export function parse_functions(content) {
  const fns   = [];
  const fn_re = /^function\s+M\.(\w+)\s*\(([^)]*)\)/mg;
  let m;
  while ((m = fn_re.exec(content)) !== null) {
    const body_start = m.index + m[0].length;
    const body_end   = lua_fn_end(content, body_start);
    const body       = content.slice(body_start, body_end);
    const raw_params = m[2].trim()
      ? m[2].split(',').map(s => s.trim()).filter(Boolean)
      : [];
    const inputs  = raw_params.filter(p => p !== '...');
    const outputs = parse_returns(body);
    fns.push({ name: m[1], inputs, outputs });
  }
  return fns;
}
// }}}
