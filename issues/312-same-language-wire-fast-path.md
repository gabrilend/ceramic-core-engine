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

## Per-edge format negotiation

Classification is **per edge**, not per box. A box always
executes its user function in native form. The format on the
wire is decided independently for each input edge and each
output edge based on the language pair across that edge:

```
input edge native?   = (producer.lang == this_box.lang)
output edge native?  = (this_box.lang == consumer.lang)
```

So a Lua box sitting in the middle of `[C → Lua → C]` works
like this:

- Input edge from the C producer: cross-lang, so the slot
  carries JSON. On read, the Lua spec calls `json_to_native`
  to lift it into a Lua value.
- The box's user function runs natively in Lua. It returns a
  Lua value.
- Output edges to the two C consumers: cross-lang, so the spec
  calls `native_to_json` once and pushes JSON bytes to each
  consumer's slot.

A Lua box in `[Lua → Lua → C]` works differently *along its
own edges*:

- Input edge from the Lua producer: same-lang, slot carries
  native binary. No conversion on read.
- User function runs.
- Output edges: one is same-lang (push native), the other is
  cross-lang (call `native_to_json`, push JSON). The producer
  pushes once to each consumer's slot in the right format.

This means **same-lang neighbors get the fast path even when
some other neighbor is cross-lang**. The classification doesn't
collapse on the first cross-language edge it finds; it stays
per-edge.

### Why per-edge, not per-box

The earlier design considered an "all same-lang or fall back
to JSON for all" rule (the simpler classification). It's
strictly dominated by the per-edge rule:

- The dominant case is "a Lua sub-graph occasionally talks to
  C." Per-box would force the Lua-to-Lua interior to round-trip
  through JSON for every box that has *any* C neighbor.
- Per-edge keeps the cost local to the actual cross-lang
  boundary. The interior of any same-lang island skips JSON;
  only the boundary edges pay.
- The implementation cost is identical: same number of bits per
  box, just stored per-port/per-edge instead of per-box.

### What each box records at graph load

For each box:

- `input_edge_native[i]` — one bit per input port. True iff the
  producer feeding that port has the same language as this box.
- `output_edge_native[j]` — one bit per outgoing connection
  (not per output port — a single output port can fan to
  multiple consumers, each of which is classified independently
  because the language pair may differ across consumers).

Both arrays are computed during `graph_attach_runtime` and live
on the box record. No runtime re-evaluation.

## Runtime dispatch

The dispatch action calls `invoke_native` always (when the spec
provides one). The boundary work is per-edge in two places:
input-read and output-push.

### Input-read: per-input-port

```c
for (int i = 0; i < n_inputs; i++) {
    slot_pop(input_slots[i], wire_buf, wire_buf_cap);
    if (b->input_edge_native[i]) {
        // wire bytes are native form already; hand to invoke_native as-is
        native_inputs[i] = wire_buf;
    } else {
        // wire bytes are JSON; lift into native form
        spec->json_to_native(ctx, wire_buf, wire_size,
                             native_buf, native_cap, &native_size);
        native_inputs[i] = native_buf;
    }
}
spec->invoke_native(ctx, fn, native_inputs, n_inputs,
                    native_out, native_out_cap, &native_out_size);
```

### Output-push: per-outgoing-connection

```c
for (int j = 0; j < b->n_connections; j++) {
    if (b->output_edge_native[j]) {
        // consumer is same-lang; push native bytes directly
        slot_push(consumer_slot, native_out, native_out_size, tag);
    } else {
        // consumer is cross-lang; serialize to JSON once per such edge
        spec->native_to_json(ctx, native_out, native_out_size,
                             json_buf, json_buf_cap, &json_size);
        slot_push(consumer_slot, json_buf, json_size, tag);
    }
}
```

If multiple cross-lang consumers share the producer's language,
the dispatch can cache the JSON encoding from the first such
edge and reuse it for the rest (optimization, not required for
correctness).

If the spec's `invoke_native` is NULL (e.g., bash), the dispatch
falls back to the universal `invoke` (JSON-in, JSON-out) and
treats every edge as JSON. This is the slow path; it's always
correct.

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

## Implementation note: ask the user about type-annotated cross-language encodings

Before implementing this issue, the implementer must ask the user
to decide on **type-annotated wire encodings** for cross-language
hops — primitives go raw bytes (4-byte int as 4 bytes, not as the
JSON string `"42"`), strings go length-prefixed UTF-8, structured
types go JSON. That decision affects the spec interface (whether
`json_to_native` is enough or whether we need a `decode_wire(type,
bytes)` shape) and the wire-format byte layout. Don't proceed with
the implementation without explicit user direction on this; the
two designs aren't trivially interchangeable later.

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

## Implementation status

What's in place (per the round summaries on 2026-05-12 to
2026-05-19):

- `lang_spec_t` extended with `invoke_native`, `invoke_json`,
  `native_to_json`, `json_to_native` callback slots (issue 303
  spec interface).
- Each of the three specs declares all four callbacks. Today
  they all alias to the single existing `invoke` implementation;
  the actual specialization (real native_to_json / json_to_native
  bodies, real per-language binary native form) is not yet
  implemented.
- `graph_attach_runtime` computes a per-box `use_native_invoke`
  bit (true iff all input producers and all output consumers
  share this box's language). The dispatch picks
  `invoke_native > invoke_json > invoke` based on that single
  bit.

What's still ahead:

- **Replace per-box `use_native_invoke` with per-edge
  classification.** The current bit is the simpler approximation
  of the per-edge design above. It needs to be split into
  `b->input_edge_native[i]` and `b->output_edge_native[j]` arrays
  computed at graph load, with the dispatch's input-read and
  output-push paths each consulting the relevant array.
- **Per-language native binary serializations.** Each spec
  needs a real `native_to_json` and `json_to_native` (also
  required by issue 317 for data boxes), plus a real
  `invoke_native` that operates on whatever the spec considers
  its native handoff format.
- The type-annotated cross-language encoding decision flagged
  above is still pending user direction.
