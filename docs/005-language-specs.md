# SoraMech — Writing a Language Spec

## Status
Stub. Filled out as the phase 3 specs are implemented (issues
306–308). For the formal contract, see issue 303. For the existing
overview, see `docs/003-driver-system.md`.

## What this doc will cover (when complete)

A walk-through of writing a new language spec, intended for a user
adding Python, Rust, Haskell, or any language not shipped by default.
Sections to fill in once the reference specs are written:

- **The contract.** What `lang_spec_t` requires you to provide
  (`init`, `teardown`, `compile`, `invoke`). Pointer to issue 303.
- **In-process vs out-of-process.** Lua and C run in the worker
  thread directly; Bash talks to a persistent subprocess over a
  socket. Which model fits your language depends on whether it has
  an embeddable runtime. Pointer to `docs/004-ipc-and-threading.md`.
- **Walkthrough: the Lua spec.** The simplest reference. Covers
  per-worker `lua_State`, registry-based file caching, typed input
  marshalling, return-value serialization. Pointer to issue 306.
- **Walkthrough: the C spec.** Adds compile-time wrapper generation
  for arbitrary user functions. Pointer to issue 307.
- **Walkthrough: the Bash spec.** The out-of-process model. Wire
  protocol, framed length-prefix messages, persistent subprocess
  lifecycle. Pointer to issue 308.
- **Adding a new language.** Directory layout, Makefile, the bare
  minimum to link against the spec interface, dropping the result
  into `langs/<name>/`.
- **Thread safety.** Per-worker state isolation. The C spec's
  `static` and `globals` race story (issue 307); cooperative
  patterns for languages with mutable global state.
- **Error handling.** Hard-crash policy (issue 303). What
  language-level errors look like at the spec boundary. How to
  catch them without losing context.

## Editor lexer (`langs/<name>/lexer.js`)

Independent of the runtime spec, each language carries an editor
lexer that the source viewer (issue 215) uses to syntax-highlight
file content. The lexer ships in the same `langs/<name>/` directory
as the runtime spec — co-located by language, not by concern.

### Contract

A lexer is an ES module that exports a single function:

```js
export function tokenize(text) {
  return [
    { type: 'keyword', start: 0,  end: 5 },
    { type: 'plain',   start: 5,  end: 6 },
    { type: 'string',  start: 6,  end: 17 },
    // ...
  ];
}
```

The returned tokens must be ordered, gap-free, and cover the entire
input. Each token's `type` is a free-form string; the viewer applies
a CSS class `syntax-<type>`. The standard set is:

| type        | typical use                                      |
|-------------|--------------------------------------------------|
| `keyword`   | language keywords                                |
| `comment`   | line and block comments                          |
| `string`    | quoted strings, char literals, here-docs         |
| `number`    | numeric literals                                 |
| `operator`  | `+`, `-`, `==`, `=`, `:`, …                      |
| `identifier`| variable / function names                        |
| `plain`     | whitespace and unclassified runs                 |

Unknown types fall back to `syntax-default`. Lexers are free to
introduce custom types (`regex`, `annotation`, `decorator`); the
viewer just renders them with the default style unless someone adds
matching CSS.

### Loading

The viewer maps a file's extension to a language name (e.g.
`.lua` → `lua`, `.sh` → `bash`, `.c` / `.h` → `c`) and dynamically
imports `/langs/<name>/lexer.js` on first use. Result is cached for
the rest of the session. Files with an unknown extension or a
missing lexer fall back to plain rendering.

### Reference implementations

- `langs/lua/lexer.js` — hand-rolled state machine, ~150 lines.
  Covers `--` and `--[[ ]]` comments, single/double/long-bracket
  strings, hex / decimal / exponent numbers, identifiers,
  multi-char operators.
- `langs/bash/lexer.js` — same shape, with `$var` / `${...}` /
  `$(...)` expansions tagged as identifiers and `#` line comments
  detected only at line-start or after whitespace (not mid-token).
- `langs/c/lexer.js` — `//` line and `/* */` block comments,
  `#directive` lines tagged as keyword spans, hex and float
  literals with C suffix support.

The reference lexers are starting points — extend or replace as
needed for languages with subtler syntax (regex literals, nested
template strings, indentation-sensitive parsing).

## Why a stub now

Issues 303, 306, 307, and 308 reference this document. Creating a
placeholder establishes the file path and the table of contents
above so cross-references resolve. The full content is written
incrementally as the reference specs are implemented and there is
real code to walk through.

## Relevant issues

- 303 — language runtime spec (the contract)
- 306 — Lua language spec
- 307 — C language spec
- 308 — Bash language spec
- `docs/003-driver-system.md` — phase 2 driver overview, phase 3
  spec overview
- `docs/004-ipc-and-threading.md` — IPC options and threading
  roadmap
