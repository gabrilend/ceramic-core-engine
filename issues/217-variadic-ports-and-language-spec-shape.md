# 217 — Variadic ports and the language-spec variadic shape

## Status
open — Part A and Part B mechanics are in the codebase; Part C is
the active design question

## What this issue is now

The original issue was titled "Concat box with per-input variadic
ports and auto-grow/shrink." That framing has aged out:

- There is no dedicated **concat box kind**. `M.concat(sep, ...)`
  lives in `libs/text.lua` and is invoked by any call box that
  imports it. The variadic UI work was never specific to concat.
- The original "split returns a JSON-encoded array on the single
  output wire" story was a phase-2 transport assumption. Phase 3
  (issue 312, same-language wire fast path) classifies each edge
  and only JSON-encodes when crossing a language boundary. So
  `M.split` returning a Lua array is correct in-language and the
  runtime bridges if the downstream box is a different language.

The issue is now about the **editor's variadic-port mechanic** and
how it interacts with the **language spec** — specifically, what
the editor's `var` toggle means and what governs whether it's
offered.

## Part A — `libs/text.lua` *(shipped)*

`libs/text.lua` ships `M.concat(sep, ...)`, `M.split(text, sep)`,
and (via issue 237) `upper`, `lower`, `trim`, `replace`,
`replace_first`, `contains`, `starts_with`, `ends_with`, `length`,
`substring`. All are pure string operations consumed by call
boxes; no special box kind exists.

`M.split` is most useful paired with the iterator box (issue 221)
so each part fans out to its own dispatch. Without the iterator
it's just "an array on a wire" — legitimate but flat.

## Part B — Editor variadic data model *(shipped)*

A box JSON can carry a `variadic_inputs` array of base names; slots
are stored as `<base>_<index>` in the `inputs` array. The inspector
(`assets/js/004-inspector.js`) implements:

- `make_variadic(port_name)` — turn a plain port into a one-slot
  variadic group (`text` → `text_0`).
- `unmake_variadic(base)` — collapse back to a single port.
- `add_variadic_slot(base)` — append `<base>_<last+1>` (auto-grow).
- `remove_variadic_slot(slot_name)` — delete and renumber.
- `auto_grow_after_set(box, slot_name)` — append a new empty slot
  when the user uses the current last slot.

Auto-grow triggers on wire-connect and on value-set. Shrinking is
explicit-only — `×` button per slot. Wire-disconnect and value-
clear do **not** shrink, deliberately, so transient editing
doesn't lose structure.

The schema (`src/001-schema.lua`) accepts `variadic_inputs` as an
optional array of strings.

## Part C — What `var` actually means *(active design)*

### Wrong model (rejected)

The earlier framing said: regex-parse each source file in the
editor, mark functions whose signatures end in `...` / `"$@"` /
`va_list` as "variadic-capable," and gate the `var` toggle on
that per-function flag.

That model collapses on contact with the project's vision:

1. **It's not a per-function fact.** Every Lua function accepts N
   positional args. `function f(a)` called with five args silently
   keeps the first; `...` in the signature is the *author's choice
   to capture extras*, not a runtime constraint. Bash is the same
   — every function gets `"$@"` whether it touches it or not. C
   through the dispatch wrapper (issue 304) is the same —
   `argc`/`argv` is the universal call shape.
2. **It centralises language knowledge in the editor.** Per-
   language regex parsers in the editor are exactly the
   ossification the `langs/<name>/` model exists to prevent.
   Adding a new language would mean writing a new regex.
3. **It locks the editor to a static source view.** A function the
   user is in the middle of editing might briefly not have `...`
   in the signature; the editor would flicker the toggle on/off.

### Right model: variadic shape is a language fact

The language declares its calling-convention shape once, in
`langs/<name>/spec.js`:

```js
export const LANGUAGE_SPEC = {
  name: 'lua',
  file_ext: '.lua',
  variadic_shape: 'positional',
};
```

`variadic_shape: 'positional'` means the language accepts N
positional args at call time, period. Every shipped language today
(Lua, Bash, C) declares this. Future languages with different
shapes (Python `**kwargs`, streaming generators, tagged unions)
declare their own shape — the editor's variadic UI eventually
grows variants for them, but `positional` covers everything we
ship.

A language that genuinely cannot accept variadic input declares
`variadic_shape: 'none'`, and the editor disables the `var` toggle
for all its boxes. We don't have one of those today.

### Editor behaviour under this model

- The `var` toggle on a port is gated by **language**, not by
  function. For every Lua / Bash / C box today, the toggle is
  available.
- The user is responsible for matching wired arity to the
  function's intent. The editor surfaces what the source declares
  (via `parser.js`'s named parameters) so the user can see the
  arity the author wrote. If they wire five inputs to a function
  whose body only reads the first, that's a user choice — same
  class as wiring a string into a port the function treats as a
  number. The runtime doesn't police it.
- Compile-time (issue 309) and validator (issue 002) checks
  surface obvious mismatches as documentation, not as gates.

### What `parser.js` does and doesn't do

`langs/<name>/parser.js` continues to enumerate functions in a
source file and extract named parameter and return names. That
feeds the file browser. It does **not** compute any per-function
variadic property — the early `variadic_tail` field was removed
in this issue's pass.

### Implementation status

- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
  shipped. Each exports `LANGUAGE_SPEC` with `variadic_shape:
  'positional'`.
- `langs/lua/parser.js`, `langs/bash/parser.js` — shipped without
  `variadic_tail`. Still produce inputs/outputs for the file
  browser.
- `tests/234-language-spec-js-test.mjs` — shipped. Asserts every
  spec.js exports a valid `LANGUAGE_SPEC` with a known
  `variadic_shape`.
- Inspector consumption of `LANGUAGE_SPEC.variadic_shape` —
  **not yet wired**. Today the `var` toggle is offered
  unconditionally; the gate-by-language step is the remaining
  work.

### Remaining work

1. `assets/js/004-inspector.js::mk_port_display` — load the active
   box's language spec (the same dynamic-import pattern used for
   lexers), and hide the `var` button when
   `variadic_shape !== 'positional'`. Today every shipped language
   is positional, so this is a future-proofing step rather than a
   visible change.
2. Inspector renders the parsed signature near the box header so
   the user always sees the function's declared arity. This is the
   documentation half of the design — replaces the rejected "gate
   the toggle per function" approach with "show the arity, let the
   user judge."
3. Update `docs/005-language-specs.md` with a "language spec for
   the editor" section pointing at `spec.js` as the canonical
   home for editor-facing language facts (variadic shape, future
   additions).

## Future development ideas

### Variadic shapes that aren't `positional`

- `keyed` — Python `**kwargs`, Ruby keyword splats. Slot model is
  `(key, value)` pairs, user labels per slot, not auto-numbered.
- `tagged` — Rust enum lists, Haskell heterogeneous lists. Per-
  slot `(tag, value)`.
- `stream` — generators, channels, iterators. One slot, queued.
  Closer to issue 213's queued-input model than to positional
  variadic.

Each shape has its own auto-grow rule and inspector controls. The
language spec declares which shape applies; the editor renders
accordingly. None of these are shipped — the positional shape
covers every language so far. Captured here so the design door
stays open.

### Outputs

The single-output rule (issue 218) is unchanged. Variadic is an
input-side concern.

## Relevant files

- `libs/text.lua` — Part A library functions
- `assets/js/004-inspector.js` — Part B variadic mechanics, future
  home for the spec-driven `var` gate
- `src/001-schema.lua` — accepts `variadic_inputs`
- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
  the language-level `variadic_shape` declarations
- `langs/lua/parser.js`, `langs/bash/parser.js` — function
  enumeration (no per-function variadic detection)
- `tests/234-language-spec-js-test.mjs` — spec.js contract tests
- `issues/231-move-signature-parsers-into-langs.md` — moved
  signature parsing into `langs/<name>/parser.js`
- `issues/312-same-language-wire-fast-path.md` — supersedes the
  old "JSON-encode every wire" story Part A was framed against
- `issues/221-iterator-box.md` — the canonical companion to
  `M.split` for fan-out
