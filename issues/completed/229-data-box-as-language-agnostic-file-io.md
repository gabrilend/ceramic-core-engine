# 229 — `read` and `write` box kinds (language-aware file IO)

## Status
complete (phase 3 + editor surface, 2026-05-19). Phase 2 mirror
skipped — phase 2 is queued for teardown so the cost of keeping it
in sync with phase 3 doesn't pay back. The "language-aware IO"
slice (true native-value handoff on cross-language wires, via the
317 bridges) is documented as a deferred next step — see the
implementation log below for the reason.

## Current behavior

The `data` kind is recognized by the schema and accepted by the
loader, but has no runtime semantics distinct from `call`. It
carries `ref` and `fn` like a call box and runs through whichever
language driver matches the file extension. The green badge on
the canvas is purely cosmetic — the box behaves exactly like
`call`.

`libs/files.lua::read_text` and friends cover the actual "read a
text file" case (issue 216, complete) via regular call boxes,
but only from Lua. A Bash function that needs file contents has
to wire through a Lua read box, which is a language-coupling no
graph reader expects.

## Intended behavior — two kinds, `read` and `write`

Both are dispatch-layer primitives — no language driver runs for
their core IO. The language specs DO participate at the wire
boundary so the value crosses cross-language seams natively (see
"language-aware IO" below). The phase-3 entry point lives in
`src/012-dispatch.c`; the phase-2 mirror lives in
`src/004-executor.lua` so the editor's "test run" works the same.

### `read` — emits a value

A `read` box has one input port and one output port. The output
is **always trusted to produce a value** — that's the box's
single job, and downstream consumers can rely on it.

| port | direction | type   | meaning                            |
|------|-----------|--------|-------------------------------------|
| path | in        | string | path to the file on disk            |
| (single output) | out | any | the value to emit                 |

No `ref`, no `fn`. The box decides what to emit using this
precedence:

1. If the inspector carries an **inline literal value** for the
   box, emit that directly. **The `path` input port hides on the
   canvas** in this mode — there's no file involved, and a path
   port on a box that doesn't read a file would just confuse the
   reader.
2. Else if `path` is supplied (via wire or via an inline literal
   path on the input port), read that file from disk and emit its
   contents.
3. Else error — a `read` with neither a literal nor a path is a
   misconfiguration the validator should catch.

### `write` — emits a done signal

| port  | direction | type   | meaning                            |
|-------|-----------|--------|-------------------------------------|
| path  | in        | string | destination path                    |
| value | in        | any    | the content to write                |
| (single output) | out | boolean | `true` when the write completed |

The output is intentionally minimal — a boolean done signal
that downstream graphs can fan into "and now the file is on
disk, fire the next thing" patterns. It's also easy to ignore
when no one cares: per the unwired-output discard rule
(documented when this issue lands), a `write` with no wire on
its output produces the boolean and drops it.

Atomic write: write to `path.tmp`, fsync, rename to `path`.
Same convention `soramech-data` already uses.

### Canvas colors

- `read` keeps the existing green for data boxes (`#4caf7d`) so
  source-of-truth-for-a-value boxes read consistently from old
  maps that still call them `data`.
- `write` gets **orange cheddar — `#d4762a`** (vivid, dark side,
  warm). The contrast against `read`'s green makes source vs
  sink visually obvious at a glance.

### Why two kinds and not one unified `data` box

A previous revision of this issue proposed a single `data` kind
that could be source, sink, or constant depending on its
configuration. That collapsed to ambiguity:

- Two ways to source the output (literal vs file) needed a
  precedence rule.
- Read and write both want `path` — a "wrote without meaning to"
  footgun lurks when the user forgets to wire `value`.
- The output port carries fundamentally different semantics by
  mode (file contents vs done signal vs literal).
- A canvas reader can't tell what the box does from its shape.

Source vs sink IS the natural seam. `read` always emits a value
(literal or file). `write` always commits a value to disk.
Different action, different identity, different color.

## Language-aware IO (depends on issue 317)

Every cross-language seam in the graph goes through the
`native_to_json` / `json_to_native` callbacks issue 317 adds to
each `lang_spec_t`. `read` and `write` use that same machinery
so a Lua data file feeding a C consumer arrives as a native C
value, and a C struct flowing into a `write` is serialized via
the C spec rather than blindly stringified by the dispatch
layer.

### `read` per-downstream-wire fan-out

A `read` box's single output port can have **N downstream
wires**, each ending at a box in possibly different languages.
The dispatch layer:

1. Sources the value (literal or file bytes).
2. Iterates the box's outgoing wires. For each:
   a. Looks up the target box's language via its `ref` extension
      (or the routing layer's pre-classified `lang` from issue 305).
   b. Calls that language's `json_to_native` to lift the value
      into the target language's native form.
   c. Hands the native value to the target box's input slot.

If three wires go to a C box, a Lua box, and a Bash box, the
dispatch loop runs the conversion three times — once per spec.
**Downstream boxes treat the `read` box's output as native to
their own language.** No SoraMech-specific glue at the consumer.

For files on disk, `read` treats the file contents as JSON text —
the canonical wire format — and feeds those bytes to each
downstream spec's `json_to_native`. A Lua user writing a file by
hand still writes JSON (or uses a `write` box upstream, which
produces JSON anyway).

### `write` per-upstream-wire dispatch

A `write` box's `value` input is fed by exactly one upstream
wire. The dispatch layer:

1. Looks up the source box's language.
2. Calls that language's `native_to_json` to canonicalize the
   value to JSON text.
3. Writes the JSON bytes to `path` atomically.

If the source is a Lua box returning a table, the Lua spec
encodes the table to JSON. If the source is a C box returning a
struct, the C spec walks the type. The user never sees the
serialization step — it's the spec's job, not the box author's.

### Why this matters

The user's mental model is "I write code in my language, the
data crosses the wire, the other side gets it in their
language." `read` and `write` should reinforce that, not break
it. Either box would otherwise be the place where the user is
forced to think about JSON shapes, byte order, or string
escapes. With the spec-driven IO callbacks, all of that lives
where it belongs — in the language specs.

### Dependency relationship with 317

229 cannot land cleanly without 317. The cross-language emit/write
behavior described above IS 317's contract; this issue just names
the runtime primitives that make first use of it. Implementation
order: 317 → 229. If 229 lands first with a stub "everything goes
through bash-style text identity," the result is plain string IO
with no Lua-table / C-struct support, which is a regression from
the design goal.

## Schema

**read** (default file-source form):
```json
{
  "id": "load_cfg",
  "kind": "read",
  "inputs": [{ "name": "path" }],
  "ui": { "x": 100, "y": 100 }
}
```

**read** with inline literal value (no path port):
```json
{
  "id": "default_prompt",
  "kind": "read",
  "value": "You are a helpful assistant.",
  "ui": { "x": 100, "y": 100 }
}
```

**read** with inline literal path (path port still present
on canvas if no `value` is set, but the port's label shows
the literal — same convention as issue 235):
```json
{
  "id": "load_cfg",
  "kind": "read",
  "inputs": [{ "name": "path", "value": "config.json" }],
  "ui": { "x": 100, "y": 100 }
}
```

**write**:
```json
{
  "id": "save_result",
  "kind": "write",
  "inputs": [
    { "name": "path" },
    { "name": "value" }
  ],
  "ui": { "x": 400, "y": 100 }
}
```

## Inspector behavior

### `read`

- **Inline value** field (textarea / text input): a free-form
  literal that, when non-empty, becomes the box's output and
  HIDES the `path` input port on the canvas. Same hide-when-
  literal precedence is the obvious progression of issue 235's
  "literal value replaces port name" idea, applied to the port
  itself.
- **Path** field: the literal-on-port equivalent for the `path`
  input. Disabled when the inline value is set.
- The two fields are mutually-illuminating: typing into one
  visibly disables the other so the user always knows which
  mode the box is in.

### `write`

- Two input port rows (`path`, `value`) with the standard
  port literal / wire UI.
- No inline-value field (a `write` with no value to commit
  is meaningless).

### Kind dropdown

Replace the current `['call', 'data']` list with
`['call', 'read', 'write']`. Picking a kind from this dropdown
swaps the box's input/output shape per the schemas above.

## Migration

Legacy `data` boxes (the cosmetic-only kind) get rewritten by
the loader. Two paths:

- Boxes with a `ref` / `fn` (the call-equivalent shape they
  carry today) → rewrite `kind` to `call`. Behavior identical
  to current.
- Boxes without a `ref` / `fn` (rare — would be invalid today)
  → rewrite to `read` with no path; the validator then flags
  them so the user notices and fills in a path or literal.

The `data` kind is retired entirely. The schema rejects it
after migration runs once.

## Unwired-output discard rule

The general rule this issue makes use of, applicable to every
box:

> **If an output value is produced but no wire carries it
> downstream, the value is discarded.**

A `write` box's done boolean is the most visible case — a user
who doesn't need a chain-after-write trigger leaves the output
unwired and the runtime drops the value. The rule already holds
informally for every box; documenting it here (and in
`docs/001-architecture.md`) makes it explicit.

## Suggested implementation sequence

1. **Block on 317**: cross-language emit/write callbacks must be
   in place. Without them this issue degrades to plain-text IO.
2. **Schema** (`src/001-schema.lua`, `src/010-graph-loader.c`):
   accept `read` and `write`; retire `data`.
3. **Loader migration** (`src/003-loader.lua`): rewrite legacy
   `data` boxes per the migration section.
4. **Phase 2 executor** (`src/004-executor.lua`): handle `read`
   (literal or file) and `write` directly. Cross-language
   conversion uses the spec callbacks via the same path as
   call-box value transport.
5. **Phase 3 dispatch** (`src/012-dispatch.c`): same handling at
   the dispatch layer. `read`'s fan-out loops through downstream
   wires per spec; `write` looks up source-spec on its `value`
   input.
6. **Inspector** (`assets/js/004-inspector.js`): kind-specific
   layout per "Inspector behavior" above. The inline-value field
   for `read` is new; the port-hide-when-literal-value behavior
   is new and needs a small render-side hook in `draw_box`.
7. **Box rendering** (`assets/js/002-boxes.js`):
   - Add `read: '#4caf7d'` and `write: '#d4762a'` to `KIND_COLOR`.
   - Add `read`/`write` to the `KIND_HEADER` map from issue 236.
   - Honor `hide_port` flag (or equivalent) on the path input
     when the inline value is set on a `read` box.
8. **Documentation**: update `docs/001-architecture.md` with the
   new kinds, the unwired-output rule, and the language-aware
   IO behavior. Update the dispatch-layer info.md.

## Open questions

- **Output port labels** for `read` and `write`: `contents` for
  read, `done` for write — pick once, document, move on. Issue
  236 already established the per-kind output-label table; add
  these two rows.
- **Static path on `write`**: should `write` also accept an
  inline literal path on its `path` port (same way `read`
  does)? Probably yes — the use case "save to a fixed location
  every time" is common. Comes free if we apply the existing
  port-literal-value mechanism.
- **`read` with both literal and path set on disk** (a user who
  edits the JSON by hand and sets both): validator rejects, or
  literal-wins? Validator rejects — explicit error rather than
  hidden precedence.
- **Cross-language type fidelity** for the C spec: walking
  arbitrary C structs is rabbit-hole territory (issue 317
  tracks this). Until C's `native_to_json` handles structs,
  C-source `write` boxes are limited to scalar values. Same
  limitation issue 317 documents.

## Relevant files

- `src/001-schema.lua` — kind validation, new shapes
- `src/010-graph-loader.c` — C-side schema, migration
- `src/003-loader.lua` — legacy `data` rewrite
- `src/004-executor.lua` — phase 2 runtime handling
- `src/012-dispatch.c` — phase 3 dispatch primitives
- `assets/js/004-inspector.js` — kind-specific inspector layout
- `assets/js/002-boxes.js` — color, header, port-hide on literal
- `langs/{lua,c,bash}/spec.c` — `native_to_json` / `json_to_native`
  (issue 317 owns these)
- `docs/001-architecture.md` — kinds section, unwired-output rule
- `issues/317-spec-json-bridge-for-data-boxes.md` — hard dependency
- `issues/235-...completed/` — literal-replaces-port-name idea
  extended to "literal hides port entirely" here
- `issues/236-...completed/` — per-kind header/output-label map
- `issues/completed/216-read-file-box.md` — Lua-side read superseded
- `issues/304-task-dispatch-layer.md` — phase 3 home for these
- `issues/completed/109-data-files-persistent-and-ephemeral.md` —
  `soramech-data` is a separate concern; not affected

---

## Implementation log

### 2026-05-19 — phase 3 + editor

What landed:

- **Kind rename** in the C runtime. `BOX_DATA` → `BOX_READ`,
  `BOX_FILE_WRITE` → `BOX_WRITE`. The schema accepts only the new
  string names; the old `"data"` / `"file_write"` strings are
  unrecognised. Per the project's "no users to migrate" reality,
  fixtures were edited by hand rather than building a one-shot
  loader migration.

- **Inline literal `value` on `read`.** A read box with `"value":
  "..."` in its JSON emits those bytes directly — no file IO, no
  path required. When `value` is unset the read box falls back to
  the existing file-source path (static `path` field). The
  "path arrives via input wire" case is left as a follow-on
  slice; today's fixtures all use the static or literal path.

- **Boolean done output on `write`.** Write boxes are no longer
  silent sinks. After a successful write, the box pushes `"true"`
  through `push_routed` like any other producer. Unwired output
  is discarded — the existing fan-to-zero-consumers path in
  `push_to_downstream` is the unwired-output rule made concrete.
  The pipeline fixture verifies both: the file gets `"11!"`, and
  the runner's outputs section shows `output → true`.

- **Editor surface.**
    * `assets/js/002-boxes.js` — `KIND_COLOR` gains `read: #4caf7d`
      (green, kept from the legacy data colour) and `write:
      #d4762a` (warm cheddar, contrasts visibly against read).
      `KIND_HEADER` gains both kinds. A new `visible_inputs()`
      helper hides the `path` port on a read box when `value` is
      set — the layout (height, port positions, hit testing) all
      route through it, so the port disappears consistently.
    * `assets/js/004-inspector.js` — kind dropdown is now `['call',
      'read', 'write']`. A new `render_io_box()` branch attaches
      a `value` textarea for read boxes (typing into it
      re-renders so the canvas port-hide toggles immediately) and
      skips the call-box machinery (ref/fn/routing don't apply).
    * `src/001-schema.lua` — `valid_kinds` accepts `call` / `read`
      / `write`. The shape check for read / write is permissive:
      `value` and `path` must be strings if set, `inputs` an
      array. Semantic enforcement ("has a value or a path")
      lives in the graph loader.

- **Integration fixture.** `tests/maps/read-literal/` exercises
  the inline-value path end-to-end: a read box with `"value":
  "hello, world!"` feeds a Lua echo box. The new entry in
  `run-tests.sh` asserts `src → hello, world!` and `echo → hello,
  world!` appear in the runner output.

### What was deferred and why

**Language-aware IO on cross-language wires.** The design above
calls for the dispatch layer to call `source.native_to_json` and
`target.json_to_native` at every cross-language seam, so a Lua
box returning a table reaches a C consumer as a typed struct (or
vice versa). That cannot happen with today's invoke / slot-store
contract: the spec's `invoke` writes bytes to `out_buf`, and the
bridges from issue 317 use a stack-protocol (read top of
`lua_State`, push to top). The two contracts don't align — a
bridge can't read a stack value that the spec already
discarded after writing to `out_buf`.

Bridging that gap is a real change with two viable shapes:

1. The slot store grows a "native handle" alternative — a slot
   carries either bytes or a "look at top of producer's stack"
   token, and the consumer's spec is responsible for consuming
   it.
2. The invoke chain is restructured so producers leave values on
   the working store and the dispatcher pulls them via
   `native_to_json`.

Either is a larger refactor than 229's box-kind work, and
neither is on the critical path for the current fixtures (every
existing cross-language wire is text-shaped today). The deferral
is documented here and in issue 317's "Next implementation
slices" section.

### Files touched

- `src/010-graph-loader.{h,c}` — kind rename; `value` field on box_t;
  schema parses `"read"` / `"write"`; comments updated.
- `src/012-dispatch.c` — `do_data_box` → `do_read_box` with literal
  path; `do_file_write_box` → `do_write_box` that pushes `"true"`;
  dispatch action's switch updated; the sink-skip guard removed.
- `src/010-graph-loader.info.md` — public-surface enum names.
- `tests/010-graph-loader-test.c` — kind name updated in `who`
  assertions.
- `tests/maps/{pipeline,hello}/boxes/*.json` — fixture rename.
- `tests/maps/pipeline/boxes/shout.json` — connection's `to_input`
  follows the rename to `value`.
- `tests/maps/read-literal/` — new fixture exercising the inline
  literal path.
- `scripts/run-tests.sh` — read-literal entry + the
  `pipeline file_write output` label renamed to `pipeline write
  output`.
- `src/001-schema.lua`, `assets/js/002-boxes.js`,
  `assets/js/004-inspector.js` — editor surface per the kind list.

## Historical proposals (superseded)

### Revision 2 (single `data` kind, literal-or-file)

Proposed one unified source box: literal value typed in the
inspector OR file at `path`. Plus a separate `write` sink.
Rejected because the source-and-sink-in-one-kind ambiguity
analysis (above, in "Why two kinds") flagged real footguns —
the `path`-input-without-`value` case silently reading from
a file the user meant to write to.

### Revision 1 (`data` source + `file_write` sink)

Proposed a `data` kind (no inputs, just a `path` field; emits
file bytes) and a separate write-sink. Renamed in revision 2
to `read` / `write` because verbs match what the boxes do
better than `data` does. Both revisions kept the same source
/ sink split.

The binary-data, atomic-write, JSON-parsing, and dispatch-layer
home questions all carry over unchanged through every
revision.
