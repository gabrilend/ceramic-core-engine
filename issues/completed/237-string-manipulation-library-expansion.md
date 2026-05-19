# 237 — Expand the bundled string-manipulation library

## Status
complete

## Implementation notes

Ten new functions appended to `libs/text.lua` after the existing
`concat` and `split`: `upper`, `lower`, `trim`, `replace`,
`replace_first`, `contains`, `starts_with`, `ends_with`,
`length`, `substring`. The information file
`libs/text.lua.info.md` carries one short entry per function in
the same shape as the existing `concat` / `split` rows.

Two design specifics worth keeping:

- All matching is **plain string**, never Lua pattern. The
  `find`/`prefix`/`suffix` arguments may carry any characters
  without escaping. `replace` / `replace_first` use a private
  `replace_n` helper that walks the input with
  `string.find(..., plain=true)` and emits an interleaved slice +
  replacement list, sidestepping the otherwise-needed
  pattern-metacharacter escape pass for both the search needle
  and the replacement.
- Booleans cross the wire as real JSON booleans, not as the
  strings `"true"` / `"false"`, because the Lua driver does
  `json.encode(result)`. The info.md spells this out so a user
  doesn't expect string-formatted booleans when wiring
  `contains` / `starts_with` / `ends_with` into a downstream
  consumer.

Tested via a 28-case luajit script covering each function plus
`nil` and edge inputs (empty string args, out-of-range
substring, pattern-metacharacter haystacks for `replace`, etc).
The file browser picks all twelve public functions up via the
existing `function M.<name>` parser — no editor-side changes
needed since the bundled-dirs work in issue 238 already makes
`libs/` visible to every map.

## Current behavior

`libs/text.lua` (created in issue 217) exposes two functions:

- `M.concat(sep, ...)` — variadic join with a separator
- `M.split(text, sep)` — split a string on a plain separator,
  returns a JSON-encoded array

That covers two of the most common string operations and was
deliberately minimal — just enough for the demo map that drove 217.
A user wanting to upper-case, lower-case, find-and-replace, trim,
or substring has to either write the function themselves into a
project-local file, or fall back to a Lua call box pointing at
`string.upper` / `string.gsub` directly (which works but is more
typing per box).

There is also no canonical home for "the string library that ships
with the editor." `libs/text.lua` exists but its scope was set by
one issue's needs, not by a broader plan.

## Intended behavior

`libs/text.lua` grows into a small, opinionated standard library
of string operations, each wired to be variadic-friendly and
single-output where it makes sense. The full set in this issue:

| function                          | inputs                          | output                          |
|-----------------------------------|----------------------------------|---------------------------------|
| `M.concat(sep, ...)`              | sep, variadic text               | joined string (already exists)  |
| `M.split(text, sep)`              | text, sep                        | JSON array of parts (exists)    |
| `M.upper(text)`                   | text                             | upper-cased string              |
| `M.lower(text)`                   | text                             | lower-cased string              |
| `M.trim(text)`                    | text                             | leading + trailing ws stripped  |
| `M.replace(text, find, repl)`     | text, find, repl                 | every occurrence replaced       |
| `M.replace_first(text, find, repl)`| text, find, repl                | first occurrence only           |
| `M.contains(text, needle)`        | text, needle                     | boolean (as `"true"`/`"false"`) |
| `M.starts_with(text, prefix)`     | text, prefix                     | boolean                         |
| `M.ends_with(text, suffix)`       | text, suffix                     | boolean                         |
| `M.length(text)`                  | text                             | integer (as string on wire)     |
| `M.substring(text, start, len)`   | text, start, len (1-indexed)     | substring                       |

All `find` / `prefix` / `suffix` arguments are **plain strings**,
not Lua patterns — same convention as `M.split`. A user who wants
patterns can call `string.gsub` directly via a call box. The
library's job is "common case, no surprises."

`M.replace_first` is a separate function rather than a flag on
`replace` because the editor has no boolean-port primitive yet
(literal values are strings; "all or first" toggles cleanly
between two function names). Could collapse later when a primitive
boolean exists.

### Companion info.md

Update `libs/text.lua.info.md` so each function gets its own entry
with inputs / outputs / one-line description. Issue 207's file
browser parses signatures from source; the info.md is the
human-skim version. (Per CLAUDE.md: prefer reading info.md over
source for an overview.)

### Why these specifically

Skim a handful of maps and the same operations show up: cleaning
input (trim), normalizing case (upper / lower), simple
substitution (replace), boolean guards (contains / starts_with /
ends_with), array slicing (substring). The list above covers the
"would you have written this as a helper anyway?" cases without
sprawling into a kitchen-sink std lib.

Operations explicitly **not** in this issue:

- Regex / pattern matching — a separate `libs/regex.lua` issue.
  Pattern syntax is its own UI problem (which dialect? escape
  rules?) and deserves a dedicated home.
- Encode / decode (base64, URL, hex) — separate `libs/codec.lua`.
- Format strings (`printf`-style) — single-output-wire constraint
  makes the call shape awkward; revisit when the language spec for
  format string handling firms up.

### Bundled location convention

Files in `libs/` are the bundled, edit-as-default modules every
map can `ref` directly without per-map setup. This issue does not
change that — it just fills out one of those bundled modules.

A future per-language convention (issue 303 territory) might split
`libs/` by language (`libs/lua/text.lua`, `libs/c/text.c`,
`libs/bash/text.sh`). Today everything in `libs/` is Lua; revisit
the split when a second language ships a bundled standard library.

## Suggested implementation steps

1. `libs/text.lua` — append the new functions. Each is a 1-3 line
   wrapper over a `string.*` builtin or a small primitive. Keep
   them order-aligned with the table above for readability.
2. `libs/text.lua.info.md` — add one entry per new function
   following the existing format. Include the "plain string, not
   pattern" note where applicable.
3. A small test map (`tests/maps/text-lib/`?) — one box per
   function, hand-traced expected outputs. The phase-2 integration
   tests can pick it up automatically (issue 311 wiring).
4. The demo for the current phase should pick up at least one or
   two of these on its display path so the user sees the library
   in action.

## Relevant files

- `libs/text.lua` — host of the new functions
- `libs/text.lua.info.md` — documentation companion
- `issues/completed/217-concat-box-and-dynamic-inputs.md` — founded
  this library with concat + split
- `issues/238-editor-http-server-config-file.md` — paired issue
  that makes "where do bundled libs live" configurable, so a user
  can add their own bundled-default directory alongside `libs/`
