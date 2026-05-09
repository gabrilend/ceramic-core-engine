# 223 — Syntax highlighting in the source viewer via user-written lexers

## Status
complete

## Implementation notes

Three reference lexers shipped under `langs/<name>/lexer.js` —
`lua`, `bash`, and `c`. Each is a hand-rolled state machine
exporting a single `tokenize(text)` function per the spec.

Server-side: `src/005-http-server.lua` adds `/langs/<...>` to the
static-serve route so the editor can `import()` the lexer modules.
The path traversal block from the existing static handler still
applies.

Client-side: `assets/js/008-source-view.js` gained a small lexer
registry (extension → language → cached module). When opening a
viewer, the body is set to plain text first as a guaranteed-correct
baseline; if a lexer is available for the file's extension it
re-renders the body asynchronously into syntax-highlighted spans.
Plain rendering is the fallback for any failure (missing lexer,
extension not in the map, lexer throw).

CSS additions in `assets/index.html`: `.syntax-keyword`,
`.syntax-comment`, `.syntax-string`, `.syntax-number`,
`.syntax-operator`, `.syntax-identifier`, `.syntax-plain`,
`.syntax-default` — colors echo the editor's accent palette.

`docs/005-language-specs.md` documents the lexer interface (token
shape, type conventions, file location, loading model, reference
implementations) so users can add lexers for languages that don't
ship by default.

### Memoization deferred

The open question about caching tokens per-(ref, content-hash) was
left for later — re-tokenizing on every viewer open is fast enough
in practice (the lexers are linear-time, no regex backtracking).
Worth revisiting if someone opens a 10k-line file and feels lag.

## Current behavior

The source viewer (issue 215, complete) shows file content as plain
monospace text. Every box source — Lua, C, Bash, anything else —
renders the same: white text on dark background, no syntax cues.
Reading a function in the editor is harder than it should be.

## Intended behavior

Each language has its own small JavaScript lexer that turns raw text
into a list of typed tokens. The source viewer asks the lexer for the
file's tokens, then wraps each token in a span with a CSS class per
token type. Styling is done purely in CSS, so themes are independent
of lexers.

This mirrors the runtime language-spec architecture (issue 303):
each language is responsible for its own concerns, the editor /
runner only knows how to call into a shared interface. No
SoraMech-internal language list, no transpiling. Users add a new
language by writing a small JS file.

### Lexer interface

Each lexer is a JS module exporting a single function:

```js
// langs/<name>/lexer.js
function tokenize(text) {
  // Returns an ordered array of tokens covering the entire input.
  // Tokens must not overlap and together must cover [0, text.length).
  return [
    { type: 'keyword', start: 0,  end: 5  },
    { type: 'plain',   start: 5,  end: 6  },
    { type: 'string',  start: 6,  end: 17 },
    // ...
  ];
}
```

Token types are convention-only — the viewer applies a CSS class
named `syntax-<type>` to each span. The standard set is:

| type        | typical use                                        |
|-------------|----------------------------------------------------|
| `keyword`   | language keywords (`function`, `if`, `local`, …)   |
| `comment`   | line and block comments                            |
| `string`    | quoted strings, char literals, here-docs           |
| `number`    | numeric literals                                   |
| `operator`  | `+`, `-`, `==`, `=`, `:`, …                        |
| `identifier`| variable / function names                          |
| `plain`     | everything not classified                          |

A lexer is free to introduce its own token types (`regex`,
`annotation`, `decorator`, etc.) — the viewer just renders unknown
types with a default `syntax-default` style.

### File location

Lexers live next to the language they belong to. For shipped
languages this is `langs/<name>/lexer.js`. The runner doesn't read
them — the editor server serves them as static JS files alongside
the `spec.c` and `spec.so` files used by the phase 3 runner.

```
langs/lua/
    spec.c          ← runtime spec (issue 306, phase 3)
    lexer.js        ← editor lexer (this issue)
    Makefile

langs/c/
    spec.c
    lexer.js
    soramech-c.h
    Makefile

langs/bash/
    spec.c
    lexer.js
    bash-server.sh
    Makefile
```

For a language whose runtime spec hasn't been written (still in phase
2), there's no `spec.c` yet — but the editor lexer can land first
and live in `langs/<name>/lexer.js` waiting for the runtime work.

### Loading

The source viewer determines a file's language by its extension and
loads the corresponding lexer on demand. Dynamic ES module import
(`import('/langs/lua/lexer.js')`) works in modern browsers and lets
the editor lazy-load only the lexers it needs.

A small registry inside the viewer caches loaded lexers so each is
fetched at most once per session:

```
ext_to_lang : { ".lua": "lua", ".c": "c", ".sh": "bash", ... }
loaded      : { "lua": <module>, "bash": <module>, ... }
```

Files with an unknown extension or a missing lexer fall back to
plain rendering — no error, just no highlighting.

### CSS

Token classes go in the existing `assets/index.html` `<style>` block
alongside the other source-viewer styles. Default theme:

```css
.syntax-keyword    { color: #4a9eff; }
.syntax-comment    { color: #6c72a0; font-style: italic; }
.syntax-string     { color: #4caf7d; }
.syntax-number     { color: #ff8c42; }
.syntax-operator   { color: #c0c4dd; }
.syntax-identifier { color: #e8eaf6; }
.syntax-plain      { color: #9ea3c0; }
.syntax-default    { color: #9ea3c0; }
```

These match the existing editor color palette (the comparator
branch colors, the inspector accents, etc.) so highlighted code
visually belongs.

### Reference lexer (Lua)

The Lua lexer ships in this issue and serves as the reference
implementation. Roughly:

```js
const KEYWORDS = new Set([
  'and','break','do','else','elseif','end','false','for','function',
  'goto','if','in','local','nil','not','or','repeat','return','then',
  'true','until','while',
]);

function tokenize(text) {
  const tokens = [];
  let i = 0;
  while (i < text.length) {
    const c = text[i];
    // ... small state machine for whitespace, comments (-- and --[[ ]]),
    // strings ("..." '...' [[...]]), numbers, identifiers, operators
    // ...
  }
  return tokens;
}

export { tokenize };
```

Hand-rolled, ~150 lines. Bash and C lexers come along once the Lua
one is settled.

## Suggested implementation sequence

1. Add token-class CSS to `assets/index.html`.
2. Modify `008-source-view.js` to consult a lexer registry by file
   extension when rendering text. Plain rendering remains the
   fallback.
3. Write `langs/lua/lexer.js` — reference implementation. Smoke
   test: open a Lua source in the viewer, confirm the colors look
   sensible.
4. Add a lazy-loader: `import('/langs/<name>/lexer.js')` on first
   need, cached.
5. Server-side (`src/006-server-main.lua`): make sure the
   `langs/<dir>/<file>` static-serve path is exposed. May already
   be implicit if the server has a generic static-file handler.
6. Write `langs/bash/lexer.js`.
7. Write `langs/c/lexer.js`.
8. Document the lexer interface in `docs/005-language-specs.md`
   (currently a stub) so users know how to add their own.

## Open questions

- Should the viewer re-tokenize on every render, or memoize per
  file content? Files in the viewer don't change between open and
  close (read-only), so caching the token list per `(ref, content
  hash)` is trivial and worth doing if a file is large.
- Live editing: this issue assumes read-only viewing. If we ever
  add an "edit in viewer" mode (currently out of scope, see issue
  215 open questions), the lexer needs to be cheap enough to run
  on each keystroke. Hand-rolled lexers comfortably hit that bar.
- Theme switching: keep the CSS in one place so a future
  light-theme effort just swaps the color values. No theming
  abstraction needed yet.

## Relevant files

- `assets/js/008-source-view.js` — viewer module that consumes
  lexer output
- `assets/index.html` — token-class CSS
- `langs/lua/lexer.js`, `langs/bash/lexer.js`, `langs/c/lexer.js`
  — shipped reference lexers (to be written)
- `issues/215-view-box-source.md` (completed) — the source viewer
  this builds on
- `issues/303-language-runtime-spec.md` — runtime spec architecture
  this mirrors
- `docs/005-language-specs.md` — to be updated with the lexer
  interface
