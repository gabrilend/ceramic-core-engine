# 312 — Same-language wire fast path: skip JSON for adjacent same-language boxes

## Status
open

## Current behavior

The wire format between boxes is JSON (`docs/004-ipc-and-threading.md`,
issue 302's "no type tags" — slot store stores opaque bytes; the
language spec encodes/decodes). Every wire pays the JSON encode +
decode cost on every value pass, even when the producer and the
consumer are the same language and could use a faster
language-native serialization.

For a Lua → Lua wire passing a 10-element table:
1. Producer's spec: `cjson.encode(table)` → string of N bytes
2. `slot_push(slot, string, N)`
3. `slot_peek(slot, buf, N)`
4. Consumer's spec: `cjson.decode(buf)` → Lua table

Steps 1 and 4 are the cost. They're language-agnostic and unnecessary
when both endpoints share a language.

## Intended behavior

Each language spec exposes **two invoke paths**:

```c
typedef struct {
    const char *name;
    const char *file_ext;

    void *(*init)(int worker_idx);
    void  (*teardown)(void *handle);
    int   (*compile)(...);

    // Universal path — JSON bytes in, JSON bytes out
    int   (*invoke_json)  (void *handle, /* common args */);

    // Fast path — language-native bytes in, language-native bytes out
    int   (*invoke_native)(void *handle, /* common args */);

    // Bridges used when a value crosses a language boundary
    int   (*native_to_json)(const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size);
    int   (*json_to_native)(const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size);
} lang_spec_t;
```

`invoke_native` is required (every spec implements one). Per-language
"native" format:
- **Lua**: `lua_dump` for closures, `cjson` or msgpack for tables, raw
  bytes for strings/numbers. Spec's choice as long as it's
  symmetric with its own decoder.
- **C**: raw struct memcpy. The compile-time wrapper (issue 307)
  knows the types; serialization is `memcpy(slot_buf, &val, sizeof
  val)`.
- **Bash**: strings only — Bash has no other native form, so the
  fast path is identical to the JSON path. Implement as a thin
  alias.

`invoke_json` is the universal interop format. Required for any
language that participates in cross-language wires. A spec that
only ever talks to itself can skip `invoke_json` (compile error if
a wire crosses to it from another language).

## Compile-time wire classification

Every wire gets a `fast_path: true | false` flag set at build time
(issue 309 build step):

```python
for wire in graph.wires:
    src_lang = box_at(wire.from).lang
    dst_lang = box_at(wire.to).lang
    wire.fast_path = (src_lang == dst_lang)
```

The flag becomes part of the compiled manifest. Runtime never
re-evaluates language pairing.

## Producer-side decision

A producer's output goes into one slot, but multiple consumers may
read from it. If they're all the same language, the producer can
write native bytes. If any consumer is in a different language, the
producer must write JSON (and same-language consumers convert via
`json_to_native` to recover native form).

Two ways to decide:
1. **Per-output decision** — analyze all consumers of the output
   slot at compile time. All same lang → native; mixed → JSON.
2. **Always JSON producer / convert at consumer** — simpler, but
   pays the encode cost even when no cross-language consumer is
   present.

Option 1 is the right call. The compile step records the producer's
output format on the box record (`output_format: "native" | "json"`),
and same-language consumers know to read native bytes directly,
cross-language consumers call `json_to_native` first.

## Runtime dispatch

The dispatch action's input-read path branches on the wire flag:

```c
for (int i = 0; i < n_inputs; i++) {
    slot_peek(input_slots[i], buf, buf_capacity);
    if (input_wires[i].fast_path && producer_format == NATIVE) {
        // bytes are already this language's native form
        ...
    } else if (producer_format == JSON) {
        // bytes are JSON, decode into native form
        json_to_native(buf, buf_size, native_buf, native_capacity, &native_size);
        ...
    }
}
```

Then `invoke_native` is called with native-form bytes regardless of
how they were obtained. Same on the output side: if the box's output
format is `native`, write native bytes; if `json`, encode then
write.

## Edge cases

- **All-same-language graph**: every wire is fast-path; `invoke_json`
  never runs. Spec writers can stub it out.
- **All-different-language graph**: every wire is JSON;
  `invoke_native` handles bytes that came back from `json_to_native`.
- **One same-language wire, several cross-language wires from the same
  producer**: the producer writes JSON. Same-language consumer
  decodes via `json_to_native` (still cheaper than full encode +
  decode round-trip — only one direction of conversion is paid,
  not two).
- **Type changes mid-development**: a wire that was Lua→Lua becomes
  Lua→C as the user changes a downstream box's language. Compile
  flips the flag; the producer now writes JSON. No runtime
  surprises, no migration.

## Implementation cost

The two-invoke spec interface adds maybe 30 lines per language
spec. The compile-time wire classification is one pass over the
graph. The dispatch action's input-read branch is one `if` on the
wire flag. None of this is hot-path complex; the gain is one less
JSON encode/decode per same-language hop, which is the whole point.

## Future: whole-program same-language compilation

A bigger optimization beyond this issue: at compile time, merge
all same-language boxes into one translation unit. The C spec
becomes whole-program; the Lua spec produces one bytecode chunk
covering every Lua function. Box-boundary crossings become regular
function calls — no slot store, no dispatch layer, no spec
invocation. Only cross-language wires retain the slot machinery.

This is a phase-3.5+ optimization and a much bigger lift —
requires the spec's `compile` callback to coordinate across all
boxes of its language, plus a runtime mode that bypasses the
dispatch layer for fully-merged regions of the graph. The
wire-level fast path in this issue is the immediate, smaller win.

## Suggested implementation sequence

1. **Spec interface** (issue 303): add `invoke_native`,
   `invoke_json`, `native_to_json`, `json_to_native` callbacks.
   Update the contract doc.
2. **Compile step** (issue 309): walk wires, set `fast_path` flag,
   set `output_format` per box.
3. **Dispatch action** (issue 304): branch on `fast_path` /
   `output_format` when reading inputs and when writing the
   output.
4. **Per-language specs** (306, 307, 308): implement both invoke
   paths. Bash's fast path is a thin alias around the JSON path.
   C's fast path is `memcpy`. Lua's fast path picks a native
   encoding (cjson for tables; raw bytes for primitives).
5. **Tests**: smoke tests confirming a 100k-iteration Lua→Lua
   pipeline with the fast path runs measurably faster than the
   same pipeline with the JSON path, and that mixed-language
   pipelines still work.

## Relevant files

- `issues/303-language-runtime-spec.md` — adds the new callbacks
- `issues/304-task-dispatch-layer.md` — branch on fast_path
- `issues/309-build-system.md` — wire-flag enrichment
- `issues/306-lua-language-spec.md`, `307-c-language-spec.md`,
  `308-bash-language-spec.md` — per-spec native implementations
- `issues/302-wire-value-slot-store.md` — slot still stores opaque
  bytes; the format negotiation is at the spec layer, not the
  store

## Open questions

- **Whether a spec must implement `invoke_json`**: leaning yes, so
  any spec can talk to any other spec. A spec that wants to
  shortcut can declare cross-language wires unsupported via
  compile-time error, but that's an opt-out, not the default.
- **How to share the bridge functions across consumers**: native_to_json
  on the producer side runs once per output (not per consumer), and
  the JSON bytes go in the slot. Each cross-language consumer pays
  json_to_native independently. Same-language consumers pay nothing.
  Symmetrically: if the producer chose native and there's one
  cross-language consumer, that consumer pays native_to_json then
  json_to_native_their_lang. Two conversions for one cross-language
  hop in this case. Probably fine; it's a corner case.
