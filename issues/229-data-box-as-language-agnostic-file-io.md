# 229 — `read` and `write` box kinds (language-agnostic file IO)

## Status
open (redesigned 2026-05-18 — supersedes the earlier "data source +
file_write sink" proposal kept below for history)

## Current behavior

The `data` kind is recognized by the schema (`call` and `data` are
the two valid kinds) and accepted by the loader, but it has no
runtime semantics distinct from `call`. It carries a `ref` and an
`fn` like a call box and runs through whichever language driver
matches the file extension. The only difference is the green badge
on the canvas — pure cosmetic.

`libs/files.lua::read_text` (issue 216, complete) covers actual
"read a text file" via a regular call box, but only from Lua. A
Bash function that needs file contents has to wire through a Lua
read box, which is a language-coupling no graph reader expects.

## Intended behavior — three kinds: `call`, `read`, `write`

Three primitive box kinds. `call` is unchanged. `read` and `write`
are new, dispatch-layer primitives — no language driver runs.

### `read` — file path in, contents out

A `read` box has one input port and one output port:

| port | direction | type   | meaning                            |
|------|-----------|--------|-------------------------------------|
| path | in        | string | path to the file on disk            |
| (single output) | out | string | the file's contents, UTF-8 |

No `ref`, no `fn`. The dispatch layer reads the file when the box
fires and pushes the contents onto the output wire.

Static path option: `read` boxes may also carry a `path` field in
their JSON for the common "read this fixed file" case. If the
`path` input wire is connected, the wire wins; otherwise the JSON
field provides the path. (Same override rule wires already follow
for literal port values everywhere else.)

### `write` — file path + value in, completion signal out

A `write` box has two input ports and one optional output port:

| port  | direction | type   | meaning                            |
|-------|-----------|--------|-------------------------------------|
| path  | in        | string | destination path                    |
| value | in        | any    | the content to write (see below)    |
| (single output) | out | boolean | `"true"` when the write completed |

The `value` input is "any kind" — whatever comes through the wire
gets serialized to a UTF-8 string and written to disk. For string
values that means write-as-is; for other types it means a sensible
text serialization (JSON-encode if structured, `tostring` if
primitive). The exact serialization rule belongs in the dispatch
layer's value-handling code, shared with how `call` already moves
values across language boundaries.

The output port carries the literal string `"true"` after a
successful write. **This output is optional.** A `write` box with
no downstream wire on its output is perfectly valid — the box runs,
the file is written, the boolean is produced and discarded. A box
with a downstream wire receives the `"true"` and can chain (e.g.
fan into a `read` of the same path, fire a notify-LLM box, etc).

### The general rule this exposes

> **If an output value is produced but no wire carries it
> downstream, the value is discarded.**

This is a runtime invariant that already holds informally for every
box; `write` makes it visible. State it once, in one place
(probably `docs/001-architecture.md` and the dispatch-layer info.md),
so the user reading any box's output understands the rule applies
universally: outputs are pushed onto the wire if a wire exists, or
silently dropped if not. No box is required to consume what another
box emits.

### Schema

**read:**
```json
{
  "id": "load_cfg",
  "kind": "read",
  "path": "config.json",
  "ui": { "x": 100, "y": 100 }
}
```
or with the path wired in:
```json
{
  "id": "load_cfg",
  "kind": "read",
  "inputs": ["path"],
  "ui": { "x": 100, "y": 100 }
}
```

**write:**
```json
{
  "id": "save_result",
  "kind": "write",
  "inputs": ["path", "value"],
  "ui": { "x": 400, "y": 100 }
}
```
The `value` slot accepts whatever upstream sends; type erasure is
the point.

### Why language-agnostic

A user reading a Bash output that needs to land in a file
shouldn't have to wire through a Lua write box. Files are bytes;
every language emits bytes; the read-and-emit / receive-and-write
steps have no language-specific anything. Pulling them into the
dispatch layer makes them available identically from every
language.

It also keeps the graph readable: a user sees
`config.json → parse → ...` and `... → save → result.json` and
immediately knows where the data flows in from and out to. The
current alternative — call boxes pointing at language-specific
read / write functions — works but buries the IO behind a function
reference.

### Atomic writes

`write` writes to `path.tmp`, fsyncs, then renames to `path`. This
matches how `soramech-data` already handles its writes. Crash-safe
by default; no flag to disable. If someone needs append semantics
or non-atomic writes later, that is a different kind (e.g.
`append`) rather than a flag on this one.

### Binary data — out of scope

UTF-8 encoding of arbitrary bytes can fail. Both `read` and `write`
treat their contents as UTF-8 strings; non-UTF-8 input on `read`
surfaces a clear error rather than silently mojibake-ing. Binary
data revisits when a real use case appears (a separate kind, or a
flag, or a base64 convention — pick when needed).

### JSON parsing stays downstream

`read` emits raw text. Parsing (JSON, YAML, CSV) belongs to a
downstream `call` box — `libs/json` for the language-agnostic path,
or a per-language parser via a call. Two reasons:

- Parse produces a structured value; the wire protocol carries
  strings. Moving structured values across the wire is a separate
  change with bigger implications.
- Different downstream uses want different parsings (decoded
  object, JSON-path extraction, schema validation). Keeping `read`
  narrow keeps it composable.

## Migration

Existing `data` boxes (if any) carry `ref`/`fn` and behave like
`call`. Migration in the loader (`src/003-loader.lua` and
`src/010-graph-loader.c`):

- `kind=data` + has `ref`/`fn` → rewrite to `kind=call` (existing
  behavior preserved).
- The `data` kind name is then retired entirely (or aliased to
  `read` if any non-rewriting tooling still says "data"; lean
  toward retire-and-error for clarity).

Schema rejects `data` after the migration runs.

## Suggested implementation sequence

1. **Schema** (`src/001-schema.lua`, `src/010-graph-loader.c`):
   accept `read` and `write` kinds with the shapes above. Retire
   `data`.
2. **Loader migration** (`src/003-loader.lua`): rewrite legacy
   `data` boxes to `call` on read; never write `data` back out.
3. **Dispatch layer** (issue 304 / `src/012-dispatch.c`): handle
   `read` and `write` as primitives. No driver invocation either
   way. Reuse the same value-serialization helpers `call` uses for
   cross-language boundaries.
4. **Phase 2 executor** (`src/004-executor.lua`): mirror handling
   for the Lua-side executor so editor "test run" works the same.
5. **Inspector** (`assets/js/004-inspector.js`):
   - `read`: show a `path` text input (the static-path JSON field)
     plus the standard input port for the wired-path case.
   - `write`: show the two input ports `path` and `value`. No `ref`
     / `fn` fields. The output port renders normally (issue 236's
     `fn()` label rule doesn't apply — pick a sensible default like
     `done` for `write` and `contents` for `read`).
6. **Box rendering** (`assets/js/002-boxes.js`): each kind gets its
   own header default per issue 236's kind-to-header map (e.g.
   `read` / `write` as the header when no label override).
7. **Docs**: add the "outputs with no wire are discarded" invariant
   to `docs/001-architecture.md` and to the dispatch-layer info.md.
   Add `read` and `write` to the box-kinds section.

## Open questions

- **Output port names for `read` and `write`**: defer to convention
  — `contents` for `read`, `done` for `write`. Bikeshed if needed.
- **Output type on `write`**: literal string `"true"`, or omit
  emission on failure (raise instead)? Probably "emit `true` on
  success, raise on failure" — failures shouldn't be silent, and
  `false` would invite users to wire failure handling into the
  graph, which the runtime is not yet built to support.
- **Path validation**: relative paths resolve against… what? The
  map's directory is the only invariant the runtime has cleanly.
  Use map-relative as default, allow absolute paths through
  unchanged (and `~` expansion if it comes up).
- **`value` serialization rule**: needs one canonical spec. Strings
  pass through; numbers / booleans → `tostring`; tables / objects →
  JSON. Document alongside the language-spec value-marshalling
  contract (issue 303).

## Relevant files

- `src/001-schema.lua` — kind validation, new shapes
- `src/010-graph-loader.c` — C-side schema for phase 3
- `src/003-loader.lua` — legacy `data` migration
- `src/004-executor.lua` — phase 2 runtime handling
- `src/012-dispatch.c` — phase 3 dispatch primitives (issue 304)
- `assets/js/004-inspector.js` — kind-specific inspector layout
- `assets/js/002-boxes.js` — render
- `docs/001-architecture.md` — update box-kinds section, add the
  unwired-output discard rule
- `issues/completed/216-read-file-box.md` — Lua-side equivalent
  superseded by language-agnostic `read`
- `issues/304-task-dispatch-layer.md` — phase 3 home for these
  primitives, alongside iterator and comparator
- `issues/236-box-header-filename-output-shows-call-form.md` —
  per-kind header default lives in the same kind map this issue
  extends
- `issues/completed/109-data-files-persistent-and-ephemeral.md` —
  `soramech-data` is a separate concern; this issue does not
  affect it

---

## Historical proposal (superseded)

The earlier version of this issue proposed a `data` kind as the
file *source* (single output, no inputs, static `path` field) and a
separate `file_write` / `sink` kind as the file *destination* (two
inputs, no outputs). The redesign above keeps the same
source / sink split but renames to `read` / `write` (verbs match
what the boxes do better than `data` does) and adds a "done"
boolean output on `write`, with the explicit discard-if-no-wire
rule that makes the optional output principled rather than ad-hoc.

The questions about binary data, atomic writes, JSON parsing, and
the dispatch-layer home all carry over unchanged.
