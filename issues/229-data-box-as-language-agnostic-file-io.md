# 229 — Data box: a language-agnostic file source (and a paired file sink)

## Status
open

## Current behavior

The `data` kind is recognized by the schema (`call` and `data` are
the two valid kinds) and accepted by the loader, but it has no
runtime semantics distinct from `call`. It carries a `ref` and an
`fn` like a call box and runs through whichever language driver
matches the file extension. The only difference is the green badge
on the canvas — pure cosmetic.

A user reading the editor reasonably expects `data` to mean "read
some data, emit it" — "if it walks like a duck…" — but the current
implementation doesn't deliver that. `libs/files.lua::read_text`
covers the actual "read a text file" need (issue 216, complete) via
a regular call box.

## Intended behavior

Repurpose the `data` kind into a **language-agnostic file source**.
A data box has:
- `ref` — an absolute or map-relative path to a file on disk.
- **No `fn`** — the dispatch layer is the function.
- **No inputs** — emits whenever its successors fire.
- A single output wire carrying the file's contents.

The runtime reads the file at the dispatch layer, encodes the bytes
as a string (UTF-8), and pushes the value onto the output wire. No
language driver is invoked. Same shape as the iterator box (issue
221): kind says "call," presence of a routing field marks the box
as a primitive the dispatch layer handles directly.

### Why language-agnostic matters

A user writing a Bash function that needs file contents shouldn't
have to wire through a Lua read box — and vice versa. A file's
bytes are a string; every language consumes a string; the
read-and-emit step has no language-specific anything. Pulling it
into the dispatch layer makes it available identically from every
language.

It also keeps the graph readable: a user reading a map sees
`config.json → parse → ...` and immediately knows where the data
came from. The current alternative — a call box pointing at
`files.read_text` — works but adds a layer of indirection that
buries what the user is doing.

### Paired sink: `kind: "sink"` (or similar) for writing files

The complement is a language-agnostic file sink. A user wires a
string into it; the dispatch layer writes the string to the
configured path. Inputs:
- `path` (string) — destination on disk
- `text` (string) — content to write

No outputs (per issue 226's sink semantics). No `ref` or `fn`. The
kind name needs picking — `sink` is taken metaphorically already;
`file_write` is unambiguous; `write` is short. Defer the bikeshed.

Both the source and the sink atomic-write where it matters: write
to `path.tmp`, fsync, rename to `path`. Standard pattern; matches
how `soramech-data` handles its writes.

### Schema

For the source:
```json
{
  "id": "config",
  "kind": "data",
  "path": "config.json",
  "ui": { "x": 100, "y": 100 }
}
```

`ref` is gone — repurposed as `path` so the field name says what it
is at the new abstraction level (read a file at this path, not call
a function at this ref). Inputs are absent; outputs are absent (the
single-output rule still gives one wire).

For the sink, the kind name TBD; data layout:
```json
{
  "id": "save",
  "kind": "file_write",
  "inputs": [
    {"name": "path",  "type": "string"},
    {"name": "text",  "type": "string"}
  ],
  "ui": { "x": 100, "y": 100 }
}
```

No `path` field on the box — it's wired in. Lets the user compute
paths upstream (e.g. by a function that builds a timestamped name).
Could also add an optional `path` field for the static case where
the user just wants a file written to a fixed location.

### Concerns about language-agnostic file IO

The original "data files have language semantics" concern (paraphrasing
from prior conversations: "different languages store strings
differently") doesn't apply here. The runtime owns the read/write,
not any language; it always emits bytes-as-UTF-8 string and always
accepts the same shape on the way in. Languages each receive a
plain string from the wire and turn it into whatever their native
string type is — that conversion is already part of the language
spec contract (issue 303).

There is one wrinkle: **binary data**. UTF-8 encoding of arbitrary
bytes can fail (invalid sequences). Two ways to handle:
1. Reject non-UTF-8 files at read time with a clear error. Forces
   the user to acknowledge the binary case explicitly.
2. Pass through as raw bytes in some marker (base64?) and let the
   downstream box decode. Adds complexity most users won't need.

Recommendation: option 1 for now. Binary data is out of scope
until someone needs it; the error surfaces clearly so the user
can switch to a language-specific binary reader.

### JSON parsing

The user asked whether parsing should happen in the data box or in
a downstream box. Recommendation: **the data box is text-only**;
JSON parsing belongs to a downstream call (Lua: `dkjson.decode`,
language-agnostic option: a parse box). Reasons:
- Parsing a JSON file produces a structured object, not a string;
  the wire protocol between boxes is currently strings (per the
  language spec contract). Moving structured values across the
  wire is a separate change with bigger implications.
- Different downstream uses want different parsings (decoded
  table, JSON path extraction, schema validation). Letting the
  data box stay narrow keeps it composable.

## Migration

Existing `data` boxes (if any) currently behave like `call`. They
have a `ref` and `fn`. Two options:
1. Convert `data` → `call` for old boxes, then redefine `data` as
   the new file-source shape.
2. Leave old `data` boxes as-is; the editor refuses to interpret
   them as the new shape until the user explicitly converts.

Option 1 is cleaner since the existing usage was incidental
(nobody was treating `data` as semantically distinct). A small
migration in `src/003-loader.lua` rewrites old `data` boxes on
read.

## Suggested implementation sequence

1. **Schema** (`src/001-schema.lua`): allow `data` boxes with
   `path` and no `ref`/`fn`; the new kind for writes (TBD name)
   takes inputs `path` and `text` and no `ref`/`fn`.
2. **Loader** (`src/003-loader.lua`): convert legacy `data` boxes
   (`kind=data` with `ref`/`fn`) to `kind=call`. Validate the new
   shapes.
3. **Phase 2 executor** (`src/004-executor.lua`): handle the new
   kinds directly — read the file and push to output for the
   source; receive inputs and write to disk for the sink. No
   driver invocation either way.
4. **Phase 3 dispatch** (issue 304): same handling at the dispatch
   layer. Source and sink are dispatch-layer primitives, alongside
   iterator and comparator routing.
5. **Inspector** (`assets/js/004-inspector.js`): when `kind=data`,
   show a `path` text input in place of `ref`/`fn`/inputs. When
   `kind=file_write` (or final name), show the inputs section as
   usual but no ref/fn/output.
6. **Box rendering** (`assets/js/002-boxes.js`): keep the green
   badge for the source; pick a color for the sink.
7. **Documentation**: update `docs/001-architecture.md` to describe
   the new `data` semantics. The old "library box" notes (if any)
   need a small rewrite.

## Open questions

- **Sink kind name**: `file_write`, `write`, `out`, `save`,
  `to_file`, `sink`. Lean toward `file_write` for "says what it is."
- **Static vs wired path on the sink**: support both, or pick one?
  Wired path is more flexible; static path is a common case worth
  one fewer wire. Probably accept both — `path` in the box JSON
  is used as the default; if a wire is connected to the `path`
  input, it overrides.
- **Binary data**: explicitly out of scope per above; revisit when
  a real use case appears.
- **Atomic-write vs simple write**: atomic-write (`tmp + rename`)
  is the safe default. If a user wants log-append semantics,
  that's a different sink kind (or a flag on this one).

## Relevant files

- `src/001-schema.lua` — kind validation, new shape
- `src/003-loader.lua` — legacy `data` migration
- `src/004-executor.lua` — phase 2 runtime handling
- `assets/js/004-inspector.js` — kind-specific inspector layout
- `assets/js/002-boxes.js` — render
- `docs/001-architecture.md` — update box-kinds section
- `issues/completed/216-read-file-box.md` — Lua-side equivalent
  that the language-agnostic source supersedes for primary use
- `issues/304-task-dispatch-layer.md` — phase 3 home for these
  primitives, alongside iterator and comparator
- `issues/completed/109-data-files-persistent-and-ephemeral.md`
  — soramech-data is a separate concern; this issue does not
  affect it
