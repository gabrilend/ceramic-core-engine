// langs/bash/parser.js — Bash signature parser for the editor's file
// browser. Issue 231 moved this code out of assets/js/007-filebrowser.js
// so per-language knowledge lives next to the rest of each language's
// pieces (spec.c, Makefile, lexer.js).

// {{{ parse_inputs
// Counts positional args a function references and names them after
// any `local var="$N"` assignments. Falls back to generic `argN` when
// no local binding is found. Returns an array sized to the highest
// `$N` the function touches.
function parse_inputs(body) {
  const params = {};
  const local_re = /local\s+(\w+)\s*=\s*["']?\$\{?(\d+)\}?["']?/g;
  let m;
  while ((m = local_re.exec(body)) !== null) {
    params[parseInt(m[2])] = m[1];
  }
  if (Object.keys(params).length > 0) {
    const max = Math.max(...Object.keys(params).map(Number));
    return Array.from({ length: max }, (_, i) => params[i + 1] || `arg${i + 1}`);
  }
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

// {{{ parse_functions
// Returns [{name, inputs, outputs}] for every shell function declared
// in the file.
//
// Output is always a single named port `output` — Bash functions emit
// one value via stdout (issue 218's single-output rule).
//
// Whether the language accepts extra positional args at call time is
// declared once per language in `langs/bash/spec.js` (`variadic_shape:
// 'positional'`), not detected per-function here. Every Bash function
// gets `"$@"` whether it touches it or not; the function's source
// decides what to do with extras.
export function parse_functions(content) {
  const fns  = [];
  const re   = /^(\w+)\s*\(\s*\)\s*\{/mg;
  const skip = new Set(['if','while','for','case','do','done','fi','then','else']);
  let m;
  while ((m = re.exec(content)) !== null) {
    const name = m[1];
    if (skip.has(name)) continue;
    const body_start = m.index + m[0].length;
    const close      = content.indexOf('\n}', body_start);
    const body       = content.slice(body_start, close > -1 ? close : body_start + 800);
    const inputs     = parse_inputs(body);
    fns.push({ name, inputs, outputs: ['output'] });
  }
  return fns;
}
// }}}
