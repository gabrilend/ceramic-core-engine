# 217 — Concat box with per-input variadic ports and auto-grow/shrink

## Status
open

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

## Relevant files
- `libs/ollama.lua` — reference pattern for a library box
- `assets/js/006-wires.js` — wire connect/disconnect events
- `assets/js/004-inspector.js` — input port list, toggle rendering
- `src/001-schema.lua` — box validator (`variadic_inputs` field)
- `src/005-http-server.lua` — PUT box endpoint (rename propagation)
- `issues/208-port-literal-values.md` — `sep` will likely use literal port values
