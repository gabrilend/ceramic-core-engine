# 230 — Required inputs, the optional flag, and no-null-from-missing

## Status
complete (compile-time check; runtime "shorter arg list" deferred)

## Implementation notes

The compile-time guarantee landed end-to-end this round. Runtime
behavior for optional-and-absent ports (the "shorter arg list"
section of this issue) is deliberately deferred until the C spec
wrapper (issue 307) and the phase-3 dispatch layer (304) are
ready to honor it — `n_inputs` reflecting actual presence vs JSON
null padding is a contract on those layers, not this one.

The shape that shipped:

- **Schema** (`src/001-schema.lua`): `optional` accepted as a
  per-port boolean. Per-port shape check rejects non-boolean
  values; absence means false.
- **Graph-level validator** in the same file:
  `M.check_input_bindings(boxes)` walks every box's inputs and
  every connection record in the cache, returning a list of
  per-port errors for any port that has no wire, no literal,
  and no `optional: true`. Pre-computes the wired-port set once
  to avoid O(N²) over many boxes. Sorts results by box id for
  predictable error ordering.
- **CLI validator** (`src/002-validate-map.lua`): runs
  `check_input_bindings` after the existing cross-box checks
  (connections, branch-else, entry-box-exists). Same gating —
  if any earlier check failed, this one is skipped to avoid
  cascading noise.
- **Editor mirror** (`assets/js/005-app.js`): a JS port of the
  same algorithm, run inside `compile_map()`. The Compile button
  blocks on input-binding errors before whatever compile
  pipeline eventually lands (issue 309). Errors print to the
  console with one line per violation; the status bar shows
  either the single error (when there's one) or a "N errors,
  see console" summary (when there are many).
- **Inspector toggle** (`assets/js/004-inspector.js`): each port
  block's top row now has an `opt` button alongside `var` / `×`.
  Click flips `port.optional` between `true` and `undefined`.
  Amber border when on, plain border when off — distinct
  category from the blue variadic family.

The check uses `\0` as the (to_box, to_input) join separator in
the wired-set, matching the same trick on both sides.

### What's *not* in this round

- **Runtime arg-list shortening**: `src/004-executor.lua` still
  emits a JSON null for any port without a value, optional or
  not. For Lua and Bash this is invisible (null arrives as nil /
  empty string, same as an omitted arg in practice); for C it
  matters once the typed wrapper lands. The wrapper convention
  is 307's territory and the dispatch layer for `n_inputs` is
  304's. Following-on work for those issues should switch the
  arg-assembly path to skip optional-and-absent ports when the
  spec layer is ready.
- **Explicit null literal** (the `null` typed into the value
  field, distinct from absence): noted in the original spec.
  Deferred — needs a small change to how the literal-value parser
  interprets the string `null`, plus per-spec confirmation that
  each language receives null natively. Pick up alongside the
  arg-assembly change above.
- **Live editor warnings** (red-outlined boxes as the user
  edits): nice-to-have, not blocking. The Compile-button check
  catches everything at the natural decision point.

Tested via a six-case Lua unit script plus end-to-end against
existing maps. User-verified visually in the editor.

## Current behavior

A box's input port can be left without a wire and without a literal
value. The phase 2 executor runs the box anyway; the function on
the other end gets `nil` (Lua), unset (Bash), or whatever default
the language hands a missing positional arg. Errors only surface if
the function happens to reference the missing arg in a way the
language can't tolerate (e.g. `string.upper(nil)` in Lua).

This is a class of silent-error bug. The user had a wiring intent,
forgot to plug something in, and the runtime quietly papered over
the mistake.

## Intended behavior

Every input port is in one of three states by **compile time**:

1. A wire is connected to it.
2. A literal value is set on it.
3. It carries `optional: true` on the port record.

**Compile fails** when any port is in none of those states. The
error is precise: which box, which port. The Compile button (issue
222) is the natural enforcement point, with the same check
duplicating in the phase 3 graph loader (issue 305).

### "Optional" is not "null"

`optional: true` says "this argument doesn't have to be passed." It
does not mean "pass null." At the spec boundary, an optional
unwired un-littered port produces a shorter arg list — the language
sees fewer arguments, not a null argument. Each language handles
that natively:

- **Lua**: omitted args are `nil`. That's a language convention,
  not a wire-level value. Inside the Lua function, `arg or default`
  works.
- **Bash**: a positional arg that wasn't passed is unset.
  `${arg:-default}` provides a default.
- **C**: the spec's compile callback (issue 307) generates a wrapper
  that reads `n_inputs` and only forwards the args that are present.

### Null as an explicit value (allowed)

If the user needs to pass null because some downstream library
expects it, they can — by typing the literal `null` into the value
field. That's a string on the wire encoded as JSON null. Each spec
maps it to its language's native null on the way in:

| JSON value  | Lua    | Bash       | C       |
|-------------|--------|------------|---------|
| `null`      | nil    | empty `""` | `NULL`  |
| `42`        | 42     | `"42"`     | 42      |
| `"foo"`     | `foo`  | `foo`      | `"foo"` |
| `true`      | true   | `"true"`   | `1`     |

The crucial distinction: **explicit null arrives through the
function call as a real argument**; **optional-and-absent does not
arrive at all**. Bash empty-string-vs-unset disappears in the
optional path because the arg position simply isn't passed.

### Schema

Add `optional` as an optional boolean on input port records:

```json
{
  "name": "config_path",
  "type": "string",
  "optional": true
}
```

Absence means `false`. No migration; existing maps work unchanged.

## Compile-time check

The check walks every box, every input. For each:
- If the port has `value` set (literal): pass.
- If any connection has `to_box === box.id && to_input === port.name`:
  pass.
- If `port.optional === true`: pass.
- Otherwise: error, with a precise message:
  `"box <id>: input '<port>' has no wire, no literal, and is not optional"`

The check runs:
- **Editor** — on Compile button click (issue 222 → 309). Errors
  surface in the editor without writing the compiled output.
- **Loader** — at run time (phase 3 issue 305). Errors abort the
  run. Belt and suspenders: the editor catches it but a hand-edited
  map JSON can still trip the loader check.

## Spec API for "is this arg present?"

The spec doesn't need to ask. The dispatch layer assembles
`input_data[]` and `input_sizes[]` from the slots that actually
have values. An optional port that's absent isn't in those arrays;
`n_inputs` is shorter by one. The spec invokes the language with
the args it received. The language's native call convention does
the rest.

For C specifically, the compile-callback-generated wrapper takes
`int n_inputs` and a count-prefixed array. The wrapper picks the
first M args that match its declared parameters and forwards them;
trailing optionals get omitted from the call (or filled with C's
zero-valued types, depending on the wrapper convention). This is
issue 307's territory.

## Inspector UI

A small per-port flag in the inspector's port block:

```
[name] [var] [opt]
[ value ........... ]
```

`opt` toggles `port.optional` on the port record. Visual: same
small button as `var` but distinct color.

## Open questions

- **Whether to allow optional + literal**: yes. A literal is an
  explicit value; the optional flag is irrelevant when the value
  is set. The compile check just needs all three signals
  available; literal wins over optional.
- **What syntax means "explicit null literal"**: lean toward
  literally typing `null` (case-insensitive) in the value field.
  An ambiguity arises when the user actually wants the string
  `"null"` — escape it as `"null"` (with quotes) which the value
  parser interprets as a string literal. Worth nailing down before
  shipping.

## Relevant files

- `src/001-schema.lua` — accept `optional` field
- `src/003-loader.lua` (phase 2) — compile-time check
- `assets/js/004-inspector.js` — `opt` toggle in port block
- `assets/js/005-app.js` — Compile button hooks check before
  compile
- `issues/309-build-system.md` — compile/package step calls the
  check
- `issues/305-c-graph-loader.md` — phase 3 check at load time
- `issues/303-language-runtime-spec.md` — `invoke`'s n_inputs
  reflects what was actually passed
- `issues/307-c-language-spec.md` — wrapper handles short-arg case
- `issues/308-bash-language-spec.md` — null vs unset distinction
  evaporates in the optional path
