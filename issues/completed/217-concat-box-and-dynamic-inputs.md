# 217 — Concat box with per-input variadic ports and auto-grow/shrink

## Status
complete

## Implementation notes

**Part A** — `libs/text.lua` ships `M.concat(sep, ...)` and
`M.split(text, sep)`. Concat is variadic to match the editor's
variadic-input model. Split returns a Lua array; the driver shim
JSON-encodes for the single output wire. The accompanying
`libs/text.lua.info.md` documents both functions.

**Part B** — Variadic UI lives in `assets/js/004-inspector.js`:

- Helpers (`parse_variadic_name`, `is_variadic_slot`,
  `variadic_slots_for`, `last_variadic_index`) parse and query the
  `<base>_<index>` naming convention for variadic slots.
- `make_variadic(port_name)` toggles a non-variadic port into a
  one-slot variadic group (rename `text` → `text_0`, add base to
  `box.variadic_inputs`, update connection records on this box and
  every upstream box).
- `unmake_variadic(base)` collapses a group back to a single port
  (drop higher slots and their wires, rename slot 0 back to base).
- `add_variadic_slot(base)` appends `<base>_<last+1>`. Used by the
  auto-grow path; also reachable directly when needed.
- `remove_variadic_slot(slot_name)` drops a single slot and renames
  higher slots down by one, updating connections atomically per
  slot.
- `auto_grow_after_set(box, slot_name)` is the public-from-Inspector
  helper. If `slot_name` is the last slot of its variadic group, it
  appends a new empty slot. Idempotent — calling again on the same
  slot is a no-op once the slot is no longer last.

The wire-create path (`006-wires.js::create_connection`) calls
`Inspector.auto_grow_after_set(dst_box, to_input)` after a
successful PUT. The inspector's per-port value-input handler also
calls it when a non-empty literal value is set on a slot. Together
these cover both growth triggers — wire-connect and value-set.

Per-port UI (`mk_port_display`):

- Plain port → `var` toggle (turn variadic on).
- First slot of a variadic group → `var ×` toggle (collapse back).
- Non-first variadic slot → `×` (remove this slot only; higher
  slots rename down).
- No `+` button — slots auto-grow. Wire-disconnect and
  value-clear do NOT shrink (only `×` removes structure).

Schema accepts `variadic_inputs` as an optional array of strings
(`src/001-schema.lua`).

Comparator toggle (`004-inspector.js::cmp_btn.onclick`) now severs
all outgoing wires on either toggle direction, fixing the prior
behavior where lt/eq/gt wires from a previous comparator state
silently re-appeared when the user re-enabled it.

## Deliberate omissions / follow-ups

- **Auto-shrink on wire-disconnect** is intentionally not done —
  the user explicitly chose "explicit `×` is the only way to shrink"
  so transient editing doesn't lose structure.
- **Editable port names** and **value editing on the canvas
  itself** belong in issue 224 — bigger redesign than 217 covers.
- **Future ideas** (language-spec-driven variadic detection,
  alternative variadic shapes for keyword args / streams /
  discriminated unions) are captured below.

## Current behavior
No standard concat box exists. No mechanism exists for a box to have a variable
number of input wires on a single logical port.

## Intended behavior

### Part A — text library box
`libs/text.lua` exposes two functions:

`M.concat(sep, ...)` — join N strings:
- `sep` — separator string (e.g. `"\n\n"`)
- `...` — any number of text values to join (variadic)
- returns a single `result` port

The function is variadic so its `text` input can expand to any count without
modifying the source file.

`M.split(text, sep)` — split one string into parts:
- `text` — the input string
- `sep` — the separator to split on (plain string, not a pattern)
- returns a single `result` port containing a JSON-encoded array of the parts

Output ports are named at wire-design time, so a variable number of runtime
parts cannot map to separate wires. A JSON array on one port is the only
consistent design — the downstream box decodes it.

Note: one output wire per box is a deliberate design constraint, not a missing
feature. Positional multi-output was explicitly rejected as fragile (multiple
return statements with the same tuple width are indistinguishable). The
comparator (lt/eq/gt branching) is the only multi-path mechanism. Whatever the
function returns goes down the single output wire as one value — arrays or JSON
objects if multiple values need to travel together.

### Part B — per-input variadic flag

Variadic is a property of a specific input port, not the box as a whole. A box
could have `sep` (fixed) and `text` (variadic). The inspector shows a toggle on
each input to mark it variadic.

#### Data model

The box JSON gains a `variadic_inputs` set (array of base names):
```json
{
  "inputs": ["sep", "text_0", "text_1", "text_2"],
  "variadic_inputs": ["text"]
}
```

Slots of a variadic input are stored as `<base>_<index>` in the `inputs` array.
The executor already passes all inputs positionally — no executor changes needed.
Connections reference slots by their indexed name (`"text_0"`, `"text_1"`, etc.).

The parser (Lua: detects `...`, bash: detects `$@` after shift) can auto-populate
`variadic_inputs` when a file is selected via the file browser. For other languages
(C, etc.) the user sets the toggle manually in the inspector — the executor contract
already supports N positional args via `arg_count`, so C functions reading `argv[]`
in a loop work without any changes.

#### Auto-grow
When a wire is connected to the LAST slot of a variadic input, the editor appends
a new empty slot (`text_N+1`) to `box.inputs` and saves the box. The new slot
appears immediately as an open port.

#### Auto-shrink
When a wire is disconnected from any slot of a variadic input:
1. Remove that slot from `box.inputs`.
2. Rename all subsequent slots down by one (`text_2` → `text_1`, etc.).
3. Update every connection that referenced a renamed slot:
   - `to_input` in the connection record on this box
   - The corresponding side on the source box (connections are stored in both
     endpoint files; both must be updated atomically).
4. Keep at least one slot for the variadic input (never shrink below `text_0`).

Middle-port removal is handled by the same rename-and-compact step: remove the cut
slot, shift all higher-indexed slots down, update their wires. The user always sees
a dense, gap-free list of ports.

## Open questions before implementing Part B
- Rename atomicity: the server's `PUT /maps/:name/boxes/:id` writes one box at a
  time. Updating the connection in the source box requires a second PUT. These are
  two separate writes — consider whether a partial failure leaves the map in an
  inconsistent state, and whether a `PUT /maps/:name/connections` batch endpoint
  is worth adding first.
- Should the file browser parser set `variadic_inputs` automatically when it
  detects `...` in a Lua signature, and if so, which argument specifically?

  -- nah let's just make the user set the variadic property manually. Unless we can put it in the language driver
     somehow? but I don't think that's really possible in the design. Thoughts?

## Suggested implementation sequence
1. Part A: implement `libs/text.lua` with `M.concat(sep, ...)`.
2. Part B schema: add `variadic_inputs` to box schema and validator.
3. Part B editor: auto-grow on wire connect; auto-shrink + rename on wire disconnect.
4. Part B inspector: variadic toggle per input port.
5. Part B parser: auto-detect `...` in Lua and set `variadic_inputs`.

## Future development ideas

### Language-spec-driven variadic detection

Currently the editor uses hard-coded heuristics per language to find
variadic indicators in a function signature (Lua: literal `...`, Bash:
`$@` after `shift`). The cleaner architecture mirrors the phase 3
language-spec model (issue 303): each language declares its own way
to recognize and surface variadic parameters, and the file-browser
parser asks the language spec rather than embedding the rules.

Concretely, the editor side of a language spec
(`langs/<name>/lexer.js` or a sibling `parser.js`) could expose:

```js
// Returns { name, inputs: ["sep", "text"], variadic_inputs: ["text"] }
// for `function M.concat(sep, ...)` in Lua, or
// `concat() { local sep="$1"; shift; ... "$@" }` in Bash.
function parse_function_signature(source, fn_name) { ... }
```

The editor still calls the same on-select handler; the language spec
is responsible for deciding how to translate native variadic syntax
into the editor's `(inputs, variadic_inputs)` model.

### Variadic shapes that don't map cleanly to an array

The current model assumes variadic = an ordered series of positional
slots. That fits Lua `...`, C `va_list`, Bash `"$@"`, JavaScript rest
params, etc. It does not fit:

- **Keyword arguments** (Python `**kwargs`, Ruby keyword splats) —
  these are key/value pairs, not positional. A box would need
  named-slot pairs the user labels per slot, not auto-numbered
  `_0` / `_1` / `_2`.
- **Tagged unions / discriminated lists** (Rust enum lists, Haskell
  variadic via heterogeneous lists) — each entry has a type tag in
  addition to a value. Two-field slots, or per-slot type metadata.
- **Streaming inputs** (generators, channels, iterators) — the
  consumer doesn't know in advance how many values arrive. This is
  closer to issue 213's queued-input model than to variadic.

A future revision of the editor's variadic UI could let language
specs choose between several "variadic shapes":

| shape           | slot model                          | example use      |
|-----------------|-------------------------------------|------------------|
| `positional`    | `<base>_<index>` ordered (current)  | Lua `...`        |
| `keyed`         | per-slot `(key, value)` pairs       | Python `**kwargs`|
| `tagged`        | per-slot `(tag, value)` pairs       | discriminated unions |
| `stream`        | one slot, queued (issue 213)        | iterators        |

Each shape would have its own auto-grow rule and inspector controls.
The language spec declares which shape applies to a given variadic
parameter, and the editor renders accordingly.

This is speculative — the positional shape covers every shipped
language so far. Capturing it here so the design door stays open.

## Relevant files
- `libs/ollama.lua` — reference pattern for a library box
- `assets/js/006-wires.js` — wire connect/disconnect events
- `assets/js/004-inspector.js` — input port list, toggle rendering
- `src/001-schema.lua` — box validator (`variadic_inputs` field)
- `src/005-http-server.lua` — PUT box endpoint (rename propagation)
- `issues/208-port-literal-values.md` — `sep` will likely use literal port values
