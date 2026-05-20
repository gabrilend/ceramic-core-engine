# 231 — Move per-language signature parsers out of the file browser into `langs/<name>/parser.js`

## Status
complete (2026-05-19) — lua + bash parsers and their editor-facing
language specs moved into `langs/<lang>/`, file browser is now
language-agnostic. C parser explicitly deferred per the original
step 3. Test coverage lands under issue 232.

## Current behavior

`assets/js/007-filebrowser.js` carries language-specific code:
- `parse_lua(content)` — extracts `function M.<name>(<params>)`
  declarations and scans for `return` statements to enumerate
  outputs.
- `parse_bash(content)` — extracts `<name>() {` declarations and
  uses `parse_bash_inputs` to count `$N` references.
- `parse_functions(content, filename)` — dispatches on file
  extension to one of the above.

C has no parser yet. Languages a user might add (Python, Rust, …)
have no path to a parser.

The lexer infrastructure (issue 223, complete) already establishes
the right pattern: per-language files under `langs/<name>/`,
loaded on demand by the editor through dynamic `import()`. The
signature parsers belong there, not in the editor.

## Intended behavior

Each language directory carries a `parser.js` ES module exporting
a single function:

```js
// langs/<name>/parser.js
export function parse_functions(content) {
  return [
    {
      name: 'write_result',
      inputs: ['text'],
      outputs: [],            // empty array → sink box (issue 226)
      variadic_base: null,    // or 'args' for f(a, ...)
    },
    // ...
  ];
}
```

The editor's file browser drops its `parse_lua` / `parse_bash` /
`parse_functions` and asks the per-language parser instead. Same
lazy-load pattern as the lexers: a registry keyed by file
extension, dynamic `import()` on first need, cached per session.
Files with an unknown extension fall back to "no parser available"
— the file browser displays the file name but offers no
function-list step.

### Extended return shape

The parser returns a richer record than the current shape:
- `name` — function name (existing).
- `inputs` — array of parameter names in declaration order
  (existing).
- `outputs` — array of return-value names; empty for sinks
  (existing).
- `variadic_base` — string or null. Set when the function
  declaration has a variadic tail (Lua `...`, Bash `"$@"`, C
  `va_list`). Editor uses it to auto-flip the var toggle for that
  port. Existing parsers don't compute this; new field, optional
  for parsers that don't bother.

### Why this matters

The file browser is the editor's single entry point for "import
function from source." Any language-specific logic in it is the
wrong level of abstraction. Moving it puts the editor on equal
footing with the runtime: both treat languages as pluggable, both
discover capabilities through `langs/<name>/`, neither has a
hardcoded list.

It also makes adding a new language a self-contained job: drop
`langs/<name>/parser.js`, `langs/<name>/lexer.js`, and (phase 3)
`langs/<name>/spec.so` into `langs/`, and the editor and the
runner both pick it up.

## Suggested implementation sequence

1. Create `langs/lua/parser.js` with the existing `parse_lua` /
   `parse_lua_returns` logic, exporting `parse_functions`.
2. Create `langs/bash/parser.js` with the existing `parse_bash` /
   `parse_bash_inputs` logic.
3. Optionally create `langs/c/parser.js` — a minimal one that
   extracts `int name(int a, char *b)` style declarations from
   header files. Defer if not needed yet.
4. Add a parser registry to `assets/js/007-filebrowser.js` mirroring
   the lexer registry in `008-source-view.js`:
   ```js
   const EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash' };
   const parser_cache = {};
   async function load_parser(lang) { /* dynamic import */ }
   ```
5. Replace `parse_functions(content, filename)` body with a lookup
   into the registry. The function becomes async; existing callers
   already `await` async file fetches, so this slots in.
6. Delete `parse_lua`, `parse_bash`, `parse_lua_returns`,
   `parse_bash_inputs`, `lua_fn_end` from `007-filebrowser.js`.
7. (Optional, separate change) Wire `variadic_base` into the
   on-select handler so the editor auto-flips the var toggle.

## Open questions

- **Test coverage**: parsers are easy to break with a regex tweak.
  A small set of fixture sources (`maps/_test/parser-fixtures/*.lua`,
  `*.sh`) and a JS test harness would catch regressions. Out of
  scope for this issue, worth a follow-up.
- **Parsing failure shape**: a parser that throws should be
  reported as "parser error" not "no parser." Add a try/catch in
  the registry layer with a clear message.

## Relevant files

- `assets/js/007-filebrowser.js` — current parsers move out
- `langs/lua/parser.js` — new
- `langs/bash/parser.js` — new
- `langs/lua/lexer.js`, `langs/bash/lexer.js` — peer modules; same
  loading pattern
- `assets/js/008-source-view.js` — reference implementation for
  the lazy lexer registry
- `issues/completed/223-syntax-highlighting-via-user-lexers.md` —
  established the `langs/<name>/<file>` server-serve route and the
  dynamic-import loading model
- `docs/005-language-specs.md` — add a "parser interface" section
  alongside the existing "lexer interface"
