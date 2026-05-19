# 317 — Language spec JSON bridge for data boxes

## Status
open

## Current behavior

A data box (`kind: data` in box JSON) emits a value the rest of
the graph routes downstream. Today the runner reads the box's
`ref` file as raw bytes and emits them — there's no concept of
"authoring this datum in a particular language using that
language's natural structures." If you want to declare a Lua
table as a data box's value, you either:

- Hand-translate the table to JSON, save it as a `.json` file,
  and lose the Lua syntax niceties; or
- Make it a call box that returns the table, paying for a full
  spec invocation just to read a constant; or
- Hard-code the bytes inline in the box's `data` field, with no
  syntactic help from any host language.

This is the wrong shape. **A box author writes idiomatic code in
the language of their choice.** That's the rule 307 establishes
for call boxes: a C-box author writes a plain C function, no
SoraMech-specific glue. The data-box equivalent should hold —
a Lua-data-box author should write a Lua file that returns
whatever they want returned (a table, a string, a number), and
SoraMech should know how to lift that into the graph as a JSON
wire value without the author writing a single SoraMech-specific
character.

## Intended behavior

Every language spec gains two callbacks, declared in
`lang_spec_t` (issue 303):

```c
/* Serialize a native-language value into JSON text.
 *
 *   ctx       — per-worker handle from spec.init.
 *   src       — caller-meaningful pointer to the native value.
 *               Format is spec-specific (see "Native handle
 *               representation" below).
 *   src_size  — size hint, or 0 if the spec interprets `src`
 *               without a length.
 *   out_buf   — destination buffer for JSON text (caller-owned).
 *   out_cap   — capacity of out_buf.
 *   out_size  — set to the bytes written.
 *
 * Returns 0 on success, -1 on error. Writes an error message
 * via the spec's existing error-reporting plumbing. */
int (*native_to_json)(void *ctx,
                      const void *src, uint32_t src_size,
                      char *out_buf, uint32_t out_cap, uint32_t *out_size);

/* Inverse: parse JSON text into a native-language value. The
 * native value is stored wherever the spec considers natural
 * (Lua: pushed to the worker's lua_State stack; C: written into
 * a typed struct supplied by `dst`; Bash: written as a textual
 * representation back into dst).
 *
 *   ctx       — per-worker handle.
 *   src       — JSON text.
 *   src_size  — JSON text length in bytes.
 *   dst       — caller-supplied target; semantics per spec.
 *   dst_cap   — capacity, where meaningful.
 *   dst_size  — set to the bytes / native units consumed.
 *
 * Returns 0 on success, -1 on error. */
int (*json_to_native)(void *ctx,
                      const char *src, uint32_t src_size,
                      void *dst, uint32_t dst_cap, uint32_t *dst_size);
```

These are the same fields 312's same-language fast path
reserves; this issue is the one that requires every spec to
**actually implement them**, driven by the data-box use case.
(312 needs them too, but its driver is performance; 317 needs
them for correctness — a data box can't function without them.)

### The rule, restated

The author of a data box writes a `.lua` / `.c` / `.sh` /
`.<whatever>` file that returns or produces a value using
*their language's native idioms*. No SoraMech macros. No
SoraMech includes. No SoraMech-specific JSON shape constraints
beyond "what your spec's native_to_json can convert."

If the author returns a Lua table, the Lua spec's
`native_to_json` converts it. If they return a C struct, the C
spec's `native_to_json` walks it according to the type declared
in the box. If they print JSON-ish text from a bash script, the
bash spec's `native_to_json` is a no-op identity (the script
already produced JSON).

The contract belongs to the spec author, not the box author.
That's the inversion: **SoraMech bends to the language, not the
other way around.**

## Native handle representation

The shape of `src` / `dst` in the callbacks is per-spec. Each
spec documents what it accepts:

- **Lua spec**: `src` is the index of a value already pushed
  onto the worker's `lua_State`. `src_size` is unused. The
  spec's `native_to_json` reads the value at that stack index
  and writes JSON. `json_to_native` pushes the parsed JSON value
  onto the stack, and `dst_size` reports the number of stack
  slots used (always 1 for SoraMech's flat-value model).
- **C spec**: `src` is a typed pointer + the box's declared
  output type drives the walk. `src_size` is the type's
  storage footprint. Mirror image for `json_to_native`. (This
  ties into 307's typed-wrapper-generation work — the same
  type system describes both the wrapper signature and the
  JSON shape.)
- **Bash spec**: `src` is text the script produced.
  `native_to_json` validates and reshapes to JSON if needed (a
  bash script that prints `42` becomes JSON `42`; a script that
  prints `{"a":1}` passes through unchanged). `json_to_native`
  is similarly a near-identity. The spec author is encouraged
  to require scripts produce well-formed JSON in the first
  place rather than write a bash-syntax parser.

The dispatch layer doesn't know or care which representation a
spec uses. It only ever passes `src` and `src_size` to the
spec's callback. The spec is opaque to the runtime; that's the
whole point.

## Data box semantics

A data box's `data` field becomes optional. The box gains
optional `lang` and `ref` fields:

```json
{
  "id": "config",
  "kind": "data",
  "lang": "lua",
  "ref": "config.lua"
}
```

At graph load, the runner:

1. Resolves the spec for `lang`.
2. Calls the spec's `init` (already done at worker startup).
3. At box-fire time, asks the spec to execute the `ref` file
   and produce a native value (this is essentially the same
   "load this file, run it, get its return value" path the
   spec already implements for call boxes — call boxes call a
   named function; data boxes take the file's return value).
4. Calls the spec's `native_to_json` to convert the native
   value to JSON text.
5. Pushes that JSON text down the wire as the box's output —
   same as today's data path, just with the value sourced
   from a language-native file.

If the box has no `lang` and a literal `data` field, the legacy
inline-data path applies unchanged.

## Why every spec must implement these

The same JSON the data box emits is consumed by some downstream
box (call, comparator, file_write, whatever). That downstream
box, if it's in a different language, will eventually need
*its* spec's `json_to_native` to convert the JSON back. So the
two callbacks are paired — every spec that participates in the
graph needs both. A spec that implements `native_to_json` but
not `json_to_native` can only be data-box sources; a spec with
only `json_to_native` can only be call-box sinks. We require
both so any language can play any role.

Specs that haven't been updated yet should set both callbacks
to a stub that returns -1 with a clear error message ("spec
`<name>`: native_to_json not implemented; this box's value
can't be routed cross-language"). The hard-fail makes the gap
loud rather than producing wrong output.

## Suggested implementation steps

1. Extend the `lang_spec_t` declaration in
   `langs/lang-spec.h` if not already done by 312's
   infrastructure work. (Per the round summary, the slot is
   reserved.)
2. Implement `native_to_json` and `json_to_native` in each
   spec:
   - **Lua spec**: walk the value at the stack index. Use the
     existing C JSON writer (`libs/json/json.c`). Handle Lua
     tables both as arrays (1-indexed contiguous integer keys)
     and as objects. Refuse functions, userdata, threads.
   - **C spec**: depends on 307's typed-wrapper machinery. For
     now, a stub that only supports scalar types (int, double,
     string) is enough to get data boxes working in C.
   - **Bash spec**: validate-and-pass-through. The script is
     expected to produce JSON. `native_to_json` runs the JSON
     parser to validate; `json_to_native` is identity.
3. Extend the data box runtime path in `src/012-dispatch.c`
   (or wherever data boxes dispatch from) to consult `lang` /
   `ref` when set and route through the spec callback chain.
4. Add a fixture: `tests/maps/cross-lang-data/` with a Lua
   data box, a C consumer, and a Bash consumer all reading the
   same value. The integration check asserts each consumer
   sees the right data.
5. Update each spec's `*.info.md` to document the native handle
   shape its callbacks expect.

## Relevant files

- `langs/lang-spec.h` — extend the contract
- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` —
  implement the callbacks
- `langs/{lua,c,bash}/spec.info.md` — document native handle
  semantics per spec
- `src/012-dispatch.c` — data-box dispatch path
- `tests/maps/cross-lang-data/` — new fixture (see step 4)
- `issues/307-c-language-spec.md` — same author-writes-natural-
  code principle; this issue extends it from call boxes to data
  boxes
- `issues/312-same-language-wire-fast-path.md` — reuses the same
  spec callbacks for a different (performance) reason

## Open questions

- Bash's `json_to_native` is a near-identity because the
  scripts already deal in text. Is there value in having it
  validate and reformat for consistency? Probably yes; cheap
  insurance against malformed downstream input.
- Should the Lua spec auto-detect array-vs-object tables, or
  require the box author to annotate? Auto-detect (contiguous
  1-indexed integer keys → array; otherwise object) is what
  every other lua-to-json library does and is intuitive enough.
- C spec's `native_to_json` for arbitrary structs is a real
  rabbit hole. Tie scope to 307 — the same type declarations
  drive both the wrapper and the JSON walk. Until 307 lands,
  only scalar types are supported for C data boxes.
