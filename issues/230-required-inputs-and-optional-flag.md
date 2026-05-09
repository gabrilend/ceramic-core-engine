# 230 — Required inputs, the optional flag, and no-null-from-missing

## Status
open

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
