# 312 — Same-language wire fast path: dual-ring slot, JSON at language borders

## Status
complete on the dual-ring foundation (2026-05-21) and the
spec-contract cleanup (this round). The 2026-05-22 amendment
that aimed to "collapse to one `invoke`, native-only" was
walked back after discussion — the buffer-oriented contract it
proposed for dispatch-side format normalisation didn't fit
stateful runtimes like Lua, whose bridges read and write
runtime state (the `lua_State` stack) rather than byte buffers.
The pre-amendment dual-ring design is the right architecture
for stateful runtimes and is what the code has shipped all
along; the amendment was a paper detour. See the "Reverted
amendment" note below if the same complexity pressure surfaces
again.

## Reverted amendment (2026-05-22, walked back 2026-05-22)

The amendment proposed:
- Dispatch normalises every input to native bytes before
  invoke; spec sees one shape with no per-input or per-output
  flag.
- `invoke_native` / `invoke_json` deleted from `lang_spec_t`.
- Specs no longer reason about wire format at all.

The architectural conflict that walked it back: the contract
assumed every spec's bridges write to a `dst` byte buffer.
For C and Bash this works — both treat values as bytes. For
Lua (and any future stateful runtime — Python, Ruby,
JavaScript), `json_to_native` doesn't produce bytes; it
pushes a value onto the runtime's own working store
(`lua_State`'s stack, in Lua's case). There is no "Lua native
byte form" external to the state for dispatch to hand to
`invoke` ahead of time. Dispatch-side normalisation only
makes sense for buffer-oriented specs; the system as a whole
should not assume buffer-orientation.

The simpler shape that does work: **each spec defines its own
native byte format**, the slot's `ring_native` carries those
per-spec bytes, the `ring_json` carries JSON, and the
ordering ring sequences cells across mixed fan-in. The
producer picks ring at push time based on per-edge
classification; the consumer reads ring at pop time from the
ordering tag and uses the matching decoder. That's the
pre-amendment design, already shipping.

What we kept from the amendment's intent: the spec interface
ended up with a single `invoke` entry point. The collapse
happened in slice 5 (dispatch stopped branching to
`invoke_native` / `invoke_json`), which left the alternate
function-pointer slots in `lang_spec_t` unused. **This round
removes those dead slots.** That is the only spec-contract
change.

If the architectural pressure that led to the amendment
returns — usually phrased as "specs reason about wire format
too much" — the answer is to look at *which spec* is
reasoning about it. Stateful specs need to. Stateless ones
shouldn't. Trying to flatten that distinction at the dispatch
level is what produced the amendment, and it doesn't fit.

### Closing plan (this round)

1. **Delete `invoke_native` and `invoke_json` from
   `lang_spec_t`** in `langs/lang-spec.h`. Drop the unused
   slot initialisers in `langs/lua/spec.c`, `langs/c/spec.c`,
   `langs/bash/spec.c` (Lua never set them; C and Bash never
   set them).
2. **Update `lang-spec.h`'s top doc** to describe the single
   `invoke` entry point, the per-input / per-output flags
   as part of that signature, and the two bridges. Remove
   "three callbacks" framing.
3. **Add a dispatch-level mixed-fan-in test** — push one
   native-tagged and one JSON-tagged cell to the same dual-ring
   slot, spawn the consumer, verify both arrive at `invoke`
   with the right `input_native[i]` flag and the consumer
   decodes each correctly.
4. **Document the Lua-native-bytes optimisation as a future
   slice.** Lua's `output_native = 1` path currently writes
   bytes that are JSON-shaped (the same encoder as
   `native_to_json`). A faster Lua-internal byte format —
   msgpack, `lua_dump`, custom binary — would make same-lang
   Lua → Lua wires cheaper. Out of scope for this round; the
   contract supports the swap.

## Current behavior

Today every wire's bytes are whatever the producing spec wrote, and
the consuming spec reads them verbatim. The Lua spec emits JSON via
`lua_native_to_json` (`langs/lua/spec.c:335`); the C and Bash specs
have no bridge implementations yet and only carry whatever bytes
their bytes-in-bytes-out `invoke` produced.

The dispatch layer picks the spec callback (`invoke_native` /
`invoke_json` / fallback `invoke`) based on a single per-box
`use_native_invoke` bit computed at graph load — true only when
**every** input edge and **every** output edge of the box stays
within one language. That's the pessimistic per-box approximation:
a box whose output fans to one same-language consumer and one
cross-language consumer is classified as cross-language for the
whole box, so the same-language neighbor eats the JSON cost too.

The wire-format design question — what shape the slot has when
same-language hops should stay native and JSON should only show up
at language borders — has not been settled in code. This issue
settles it.

## Intended behavior — dual-ring slot

**Each input slot is two ring buffers plus an ordering ring.**

```
slot {
    ring_native  : ring of cells holding consumer-lang's native bytes
    ring_json    : ring of cells holding JSON bytes (cross-language values)
    ring_order   : ring of (which_ring, idx) tuples in arrival order
    ... existing peek / pop modes apply to ring_order ...
}
```

**Producer push** (per outgoing connection):

- If the connection's per-edge bit `output_edge_native[j]` is true,
  the producer's spec writes its native bytes to the consumer
  slot's `ring_native`.
- Otherwise the spec converts to JSON via `native_to_json` and
  writes to `ring_json`.
- Either way, push a `(which_ring, idx)` tuple to `ring_order`.

**Consumer pop**:

- Pop `ring_order` to learn which ring the next value lives in.
- Pop the cell at the indicated index in the indicated ring.
- The consumer's spec receives a `(native | json, bytes)` pair per
  input and picks the matching decoder.

This shape is precise on **mixed fan-in ports**: a port fed by one
Lua producer and one C producer keeps the Lua values native in
`ring_native` and the C-translated-to-JSON values in `ring_json`.
The consumer pays JSON-decode cost only on cross-language cells,
not on the entire port. The per-port-classification alternative
(slot is all-JSON if any fan-in is cross-lang) is the simpler
fallback — strictly correct, but forces native producers to eat
JSON on mixed-fan-in ports. Dual-ring keeps each cell at the
right cost.

### Per-language native bytes

Each spec picks its own native byte format. The only contract is
internal symmetry: the spec's own native-write and native-read have
to round-trip its own runtime values.

- **Lua** — bytes the spec can deserialize back to a Lua value
  quickly. Candidates: msgpack, `lua_dump` for closures, a custom
  binary form. `cjson.encode` is a valid starting point (a JSON
  string the Lua spec parses with `cjson.decode`); refine later if
  profiling shows the encode/decode is hot.
- **C** — `memcpy` of the typed struct. The C spec's generated
  wrapper (issue 307) knows the layout, so the ring cell holds the
  struct's bytes verbatim.
- **Bash** — strings. Bash has no other native form; its native
  ring stores string bytes directly.

### Why JSON for the cross-language ring

JSON's structure is self-describing — six kinds (object, array,
number, string, bool, null), each with an obvious mapping into
every language's runtime. Adding a new language K means K's spec
implements one pair of bridges: `native_to_json` and
`json_to_native`. No spec ever needs code that knows another
language's types. The N-language matrix collapses to N spec-pair
implementations, not N² pair-wise paths.

The harder cases — closures, opaque language values, function
pointers — don't fit JSON's structure. JSON carries them via
sentinel primitives:

- `$ref` — pointer + length + type tag for bytes in the large-value
  heap. In-process specs deref directly; cross-process boundaries
  inline the bytes at the UDS marshaller.
- `$function_pointer` — in-process address + signature for
  callables. Wire compile fails if the consumer's language can't
  reconstruct.
- `$lang_opaque` — "these bytes are language-X-internal." The
  destination spec either knows what to do or wire compile fails.

First-cut behavior: `encode_value` in the Lua spec hard-errors on
function / userdata / thread (`langs/lua/spec.c:317`). Sentinels
land in a later slice.

## Per-edge classification at graph load

The graph loader's single-threaded phase walks every connection
and computes per-box arrays:

- `input_edge_native[i]` — one bit per input port. True iff every
  producer feeding that port shares the consumer's language.
- `output_edge_native[j]` — one bit per outgoing connection. True
  iff the consumer on the other end shares this box's language.

Both arrays are already declared and populated — see
`src/010-graph-loader.h:145` and `src/010-graph-loader.c:689-754`.

Under dual-ring, only `output_edge_native[j]` is consulted on the
push path (it picks the ring at write time). The consumer-side bit
`input_edge_native[i]` is now informational — the actual per-cell
ring tag lives in `ring_order` and is precise per cell.

The per-box `use_native_invoke` summary is **removed**. The
dispatch reads `ring_order` per pop and consults the cell's tag
directly.

### Why per-edge, not per-box

The earlier per-box approximation collapsed on the first
cross-language edge it found — a Lua box with one C neighbor went
JSON across all its other edges. Per-edge keeps the cost local to
the actual cross-language boundary. The interior of any
same-language island skips JSON; only the boundary edges pay. The
implementation cost is identical: same number of bits per box,
stored per-port / per-connection instead of per-box.

Dual-ring takes precision one step further: even on a single port
with mixed fan-in, each individual cell is in the right ring. The
boundary tightens from "edges that cross languages" to "cells
that crossed languages."

## Runtime dispatch

### Input-read

For each input port:
```c
(which_ring, idx) = ring_order_pop(slot->ring_order)
cell_bytes        = ring_at(which_ring == NATIVE ? slot->ring_native
                                                  : slot->ring_json,
                            idx)
input_data[i]   = cell_bytes
input_native[i] = (which_ring == NATIVE)
```

`input_native[i]` is a per-input flag the spec consults to know
whether `input_data[i]` is its native form or JSON.

### Spec invoke

**Superseded by the Amendment (2026-05-22)** — the final design
drops the per-input and per-output flags from invoke and moves
format conversion into dispatch. The historical signature
below is what slices 2–4 implemented; the next slice removes
the flags.

The spec's `invoke` signature gains per-input and per-output flags:

```c
int invoke(handle, file_path, fn_name,
           const void **input_data, const int *input_sizes,
           const int *input_native, int n_inputs,
           int output_native,
           void *out_buf, int out_buf_capacity, int *out_size);
```

The spec dispatches per input: if `input_native[i]`, decode as
native; else decode as JSON. The output is written in whatever
form `output_native` indicates — one form, one write. The previously
separate `invoke_native` and `invoke_json` callbacks collapse into
this single dispatching `invoke`; the per-input dispatch is what
the design actually needs.

For boxes whose output fans to multiple consumers in different
languages, the dispatch picks one form for `output_native` based
on whichever ring is dominant (typically native if any
same-language consumer exists; the cross-language consumers eat a
`native_to_json` conversion at push time). The detail is a runtime
optimization, not a correctness concern.

### Output-push

```c
for each outgoing connection j:
    if output_edge_native[j]:
        push native_bytes      to consumer->ring_native
        push (NATIVE, idx)     to consumer->ring_order
    else:
        if output_was_native: json_bytes = spec->native_to_json(native_bytes)
        push json_bytes        to consumer->ring_json
        push (JSON, idx)       to consumer->ring_order
```

If the spec produced native and multiple cross-language consumers
exist, the dispatch calls `native_to_json` once and reuses the
JSON bytes across every cross-language push (optimization, not
required for correctness).

## Edge cases

- **All-same-language graph**: every connection's
  `output_edge_native[j]` is true; `ring_json` stays unallocated
  across every slot; activity is on `ring_native` and `ring_order`
  only.
- **All-different-language graph**: every connection routes to
  `ring_json`; `ring_native` stays unallocated.
- **Mixed fan-in**: both rings active on the same slot. Consumer
  sees a mix of native and JSON cells, picked apart by
  `ring_order`.
- **Type changes mid-development**: a wire that was Lua→Lua
  becomes Lua→C when the user changes a downstream box's language.
  The graph loader recomputes the per-edge bits on the next load;
  no data conversion at runtime, no migration.

## Implementation cost

- **Slot store**: two rings per slot instead of one, plus the
  ordering ring. Allocate `ring_json` and `ring_order` lazily —
  same-language-only slots pay nothing for them. Per-cell cost:
  one extra ordering-ring write on push, one extra read on pop.
- **Spec interface**: one signature change to the `invoke`
  callback adding `input_native[]` and `output_native`. Every
  spec's `invoke` gains a per-input branch. Lua's branch is
  almost free — both decoders (`json_to_native` and the future
  native one) already exist as bridges. C and Bash need their
  JSON bridges written, which is shared with the broader
  cross-language work in 307/308.
- **Dispatch**: one extra step on read (consult ordering ring),
  one extra branch on push (which ring to write).

The win is exactness: same-language values stay native even when
their port has a cross-language sibling producer.

## Future: whole-program same-language compilation (issue 313)

313 describes merging all same-language source into one
translation unit per language at compile time. The merge does
**not** eliminate the slot store for same-language wires — boxes
still run as separate tasks in the thread pool, possibly on
different workers, so values still need a place to wait for
sibling inputs to arrive. The thread pool's parallelism depends
on tasks being scheduled independently; collapsing same-language
hops to direct function calls would serialize execution and defeat
the pool.

What 313 thins is the **spec invocation overhead**: for boxes in
the merged region, the dispatch can call the function directly on
the worker's `lua_State` (or compiled C code) without going
through dlsym + bytes-marshal + bridge. The dual-ring slot shape
remains; 313 makes the native-ring's read/write cheaper. The two
layers stack, they don't cancel.

## Resolved design choice: JSON, not type-annotated raw bytes

An earlier note asked whether cross-language wires should use
type-annotated raw byte encodings (length-prefixed ints, raw
floats, etc.) instead of JSON. **Decided: JSON across the board
for the cross-language ring.** Rationale:

- JSON's self-describing structure makes a new language one
  spec-pair of `native_to_json` / `json_to_native` — no per-pair
  language paths.
- The native ring's format is each spec's private choice, so the
  per-language performance flexibility is there without universal
  type-tag complexity.
- Sentinel primitives in JSON (`$ref`, `$function_pointer`,
  `$lang_opaque`) carry the structurally awkward cases without
  forcing the wire format to grow new top-level shapes.

## Suggested implementation sequence

1. **Extend the slot data structure**. Add `ring_native`,
   `ring_json`, `ring_order` to the slot type in
   `src/009-slot-store.{c,h}`. Allocate `ring_json` and
   `ring_order` lazily.
2. **Extend the spec `invoke` signature**. Update `lang_invoke_fn`
   in `langs/lang-spec.h` to add `const int *input_native`,
   `int output_native`. Touch every spec's `invoke` stub to accept
   (and initially ignore) the new parameters.
3. **Update dispatch input-read** (`src/012-dispatch.c`). Pop
   `ring_order` first, then the named ring. Populate
   `input_native[]` per input port.
4. **Update dispatch output-push** (`src/012-dispatch.c`). Select
   the consumer's ring based on `output_edge_native[j]`. Write
   the ordering-ring tuple. Reuse JSON across cross-language
   consumers when the producer fanned to multiple.
5. **Lua spec** (`langs/lua/spec.c`). Branch per `input_native[i]`
   — decode native or JSON accordingly. Pick the native byte
   format (start with `cjson.encode`; refine later).
6. **C and Bash spec bridges**. Implement `native_to_json` and
   `json_to_native` for C and Bash so cross-language pushes work.
   C's native form is `memcpy` of the wrapper's declared struct;
   Bash's native form is plain strings.
7. **Remove `use_native_invoke`**. Drop the per-box field from
   `box_t` and its computation from `graph_load`. The dispatch
   consults the ordering ring's per-cell tag, not a per-box
   summary.
8. **Tests**. A 100k-iteration Lua→Lua pipeline uses `ring_native`
   exclusively. A mixed-lang pipeline exercises both rings. A
   mixed-fan-in port shows both rings active on one slot.

## Relevant files

- `src/009-slot-store.{c,h}` — slot data structure; the
  dual-ring extension lands here.
- `src/010-graph-loader.{c,h}` — per-edge classification (already
  done at `010-graph-loader.c:689-754`).
- `src/012-dispatch.{c,h}` — input-read, output-push.
- `langs/lang-spec.h` — `invoke` signature extension.
- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` — per-spec
  implementations.
- `issues/302-wire-value-slot-store.md` — original single-ring
  slot design (completed); the dual-ring extension is the structural
  amendment captured here.
- `issues/306-lua-language-spec.md`, `307-c-language-spec.md`,
  `308-bash-language-spec.md` — per-spec typed marshalling lands on
  top of this issue.
- `issues/313-research-whole-program-same-language-merge.md` —
  follow-on that thins spec invocation for merged regions.
- `issues/245-wire-color-by-cross-language-classification.md` —
  editor-side visual reflection of the per-edge classification
  (green for JSON, gray for native).

## Implementation status

What's in place:

- `lang_spec_t` carries `invoke_native`, `invoke_json`,
  `native_to_json`, `json_to_native` slots (current shape — to be
  collapsed to a single `invoke` with per-input/per-output flags
  per this issue).
- Lua's `native_to_json` and `json_to_native` are real
  (`langs/lua/spec.c:335` and `:437`).
- `input_edge_native[]` and `output_edge_native[]` are computed at
  graph load (`src/010-graph-loader.c:689-754`).
- Per-box `use_native_invoke` summary is computed and consulted by
  dispatch — to be removed.
- **Slice 1 complete (2026-05-21).** Dual-ring slot data structure
  in `src/009-slot-store.{c,h}`: `SLOT_FLAG_DUAL_RING`,
  `SLOT_RING_NATIVE` / `SLOT_RING_JSON` constants, three internal
  rings per slot (native data, JSON data, ordering) under one
  per-slot spinlock, new API `slot_push_native`, `slot_push_json`,
  `slot_pop_ordered`. Single per-slot lock makes the data+ordering
  write atomic so consumers see push pairs intact. On non-dual
  slots `slot_push_native` and `slot_pop_ordered` alias to the
  existing `slot_push` / `slot_pop`; `slot_push_json` returns -1.
  On dual slots the old single-ring API returns -1 (caller must
  pick a ring). `slot_has_value` and `slot_fill_count` consult
  the ordering ring on dual slots so spawn-on-input-ready and
  test introspection still work. Five new tests in
  `tests/009-slot-store-test.c` cover the happy paths
  (native-only, mixed arrival order, wraparound), the
  order-ring-as-binding-capacity-constraint, and the back-compat
  cases.

- **Slice 2 complete (2026-05-21).** `lang_invoke_fn` typedef in
  `langs/lang-spec.h` gained `const int *input_native` (between
  `input_sizes` and `n_inputs`) and `int output_native` (after
  `n_inputs`, before `out_buf`). Because all three callback slots
  (`invoke`, `invoke_native`, `invoke_json`) share the same
  typedef, the change propagates automatically. Every spec's
  invoke function picked up the new params and currently ignores
  them — the per-input branching is slice 4 (Lua) and 307 / 308
  (C and Bash). Dispatch's `do_call_box` passes `NULL` /
  `0` placeholder values; slice 3 wires up the real per-cell
  flags from the dual-ring's ordering pop. Eleven direct
  call sites in the spec test files updated to the new signature.
- **Slice 3 complete (2026-05-21).** Call-box input ports become
  dual-ring slots (in `graph_attach_runtime`) when their shape
  permits — restrictions inherited from slice 1: not LARGE_VALUE,
  not TAGGED. Dispatch's `read_inputs` consults
  `slot_flags(slot_id)` and routes dual-ring slots through
  `slot_pop_ordered`, populating `input_native[i]` from the
  per-cell `which_ring` tag returned by the pop; single-ring
  slots keep the existing `slot_peek` / `slot_pop` path and
  derive `input_native[i]` from the per-port
  `input_edge_native[i]` classification. Push side
  (`push_one_connection`) now takes the producer's connection
  index, branches on the consumer slot's flags, and picks
  `slot_push_native` vs `slot_push_json` based on the producer's
  `output_edge_native[j]`. Literal pushes from
  `dispatch_push_literals` go through `slot_push_native` (literals
  are configured in the consumer's own box JSON, so semantically
  same-language). `do_write_box`'s `read_inputs` call passes
  `NULL` for the flag array since write boxes aren't spec-invoked.
  Added `slot_flags(s, id)` accessor to the slot store API so
  dispatch can branch without storing per-port bookkeeping.
  Existing tests stay green (22 slot-store, 14 graph-loader,
  7 dispatch, 11 integration including multi-language `pipeline`
  and `driver-test`); test fixtures that pushed directly via
  `slot_push` on what's now a dual-ring slot were migrated to
  `slot_push_native`.

- **Slice 4 complete (2026-05-21).** Lua spec's `lua_invoke`
  branches per `input_native[i]`: NATIVE (or NULL slice-2 callers)
  push via `lua_pushlstring` exactly as before; JSON
  (`input_native[i] == 0`) calls a new shared `parse_json_to_stack`
  helper that lifts the bytes into a Lua value via the existing
  `decode_node`. On JSON parse failure the spec falls back to
  `lua_pushlstring` silently — that's the bridge to today's
  cross-language producers (C / Bash) which haven't started
  emitting real JSON yet; preserves backward compatibility while
  the input-format contract tightens. `lua_json_to_native` was
  refactored to use the same helper, just with `verbose=1` so the
  bridge path remains loud on parse failures (a cross-language
  wire that ought to carry JSON but doesn't is a real wire
  error). Output-side branching on `output_native` deferred —
  that's a coordinated change with the C and Bash specs and the
  dispatch's per-call output format computation; see "what's
  still ahead" below. Three tests in `tests/306-lua-spec-test.c`
  cover the new behavior: `invoke_json_input` (JSON table parses
  and indexes correctly), `invoke_native_input_with_flag`
  (NATIVE flag keeps the raw-string path), and
  `invoke_json_input_falls_back_to_raw` (non-JSON bytes with
  JSON flag fall back gracefully).

- **Slice 4.5 (2026-05-21) — per-wire writing style + Lua JSON
  output.** Two coordinated halves:
  - **Per-wire push.** Each wire keeps its own format. The
    dispatch picks the consumer slot's native or JSON ring per
    outgoing connection based on `output_edge_native[j]`. A box
    with mixed fan-out (some same-lang consumers, some
    cross-lang) lets each consumer get the right format for its
    own wire.
  - **Lua writes JSON when any consumer is cross-language.** The
    spec passes `output_native = 1` to invoke if every outgoing
    wire is same-language, else `0`. On the `0` branch the Lua
    spec serializes the return via the same encoder that powers
    the `native_to_json` bridge, so cross-language consumers
    receive parseable JSON (numbers, strings, tables, arrays).
    On `1` the spec keeps the existing tostring coercion. C-side
    JSON-input acceptance landed in 307 alongside this, so a Lua
    → C wire with a primitive payload now round-trips through
    real JSON in both directions. The pipeline integration test
    exercises this end-to-end: Lua produces a number, JSON-
    encodes to bare digits, C unwraps and reads.
  - **Known limitation for mixed fan-out.** A producer with
    both same-lang and cross-lang consumers writes one format
    (JSON, picked by the AND of `output_edge_native[j]`). The
    same-lang consumers in that case receive JSON bytes through
    their native ring — Lua's input-side parse-with-fallback
    absorbs this, so primitives still round-trip. A full per-wire
    output (producer leaves the value in its runtime; dispatch
    calls `native_to_json` per cross-lang wire to materialize
    JSON only where needed) is a deeper contract change tracked
    as a future slice.
- **Slice 5 complete (2026-05-21).** The per-box
  `use_native_invoke` field and its computation are gone. The
  dispatch's invoke-fn selector collapsed to a single
  `spec->invoke` call — the per-input and per-output format flags
  carry the information the old `invoke_native` / `invoke_json`
  callback split was meant to encode. The spec interface's
  `invoke_native` and `invoke_json` slots remain as optional
  no-ops; future specs can specialize on them if profiling
  justifies, but the dispatch no longer requires them. Three
  test-side references in `tests/010-graph-loader-test.c` that
  asserted on the per-box bit were updated to assert on the
  per-edge bits they were already alongside.

What still depends on this issue but lives elsewhere:

- **307 / 308 — C and Bash JSON-input handling**, plus the
  symmetric output side. Once those land, slice 4.5's deferred
  Lua-side JSON write can also land and cross-language tables /
  structured values will round-trip.
- **318 — sentinel primitives**, the JSON-wire shapes that carry
  closures / pointers / opaque language values across the
  cross-language ring.

## Open questions

- **Lua's native byte format**: start with `cjson.encode` (still a
  win over the json-ring path because the consumer's spec
  recognises its own format and skips a parse), or jump straight
  to msgpack? Defer the decision; the dual-ring shape doesn't
  depend on the choice.
- **Sharing JSON across multiple cross-language consumers**: the
  dispatch can call `native_to_json` once and push the same bytes
  to N cross-language consumer slots. Worthwhile when N > 1; the
  push path needs a small buffer to hold the JSON across the fan
  loop. Implement after the basic path works.
- **Bash native = JSON?**: Bash has no native form distinct from
  strings, and JSON is already a string. Bash's native ring may
  hold JSON-shaped strings (effectively aliasing the JSON ring's
  content). Resolve by inspection once the bash spec writeup
  starts.
