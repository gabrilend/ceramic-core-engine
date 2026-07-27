# 318 — Sentinel primitives in the cross-language JSON wire

## Status
complete (slice 1 — $lang_opaque emit + $ref machinery + capabilities)

## Current behavior

Three canonical JSON sentinel shapes are recognized everywhere
the wire format runs, with reserved key names that no plain
object will accidentally collide with:

```json
{"$ref":              {"chunk_ptr": "0x...", "len": N}}
{"$lang_opaque":      {"lang": "lua", "tag": N, "shape": "..."}}
{"$function_pointer": {...amendment: wrapper-binary subsystem...}}
```

A new spec-independent sentinel module
(`src/020-sentinels.{h,c}`, with unit tests in
`tests/020-sentinels-test.c` — 7 passing) carries the shared
machinery: kind detection on a parsed JSON node, emit-writers for
each kind, a process-wide `$ref` bytes store (allocate / lookup
/ clear), and a capability-mask validator.

`lang_spec_t` (`langs/lang-spec.h`) grows two capability fields:
`sentinel_emit_mask` and `sentinel_reconstruct_mask`. Each spec
declares which kinds its `native_to_json` may write and which
its `json_to_native` can rebuild. Lua publishes
`REF | LANG_OPAQUE` on both sides; C publishes `REF` on both
sides; Bash leaves both at zero. The capability fields are
consultable but the compile-time wire validation that uses them
to gate cross-language wires is itself a follow-on slice.

Lua's encoder (`encode_value` in `langs/lua/spec.c`) no longer
hard-errors on `LUA_TFUNCTION` / `LUA_TUSERDATA` / `LUA_TTHREAD` /
`LUA_TLIGHTUSERDATA`. Instead it registers the value in the
worker's `LUA_REGISTRYINDEX` via `luaL_ref` and emits a
`$lang_opaque` sentinel carrying the registry tag and a `shape`
hint (`lua_typename(t)`).

Lua's decoder (`decode_node`) detects sentinels at the top of
the `JSON_OBJECT` case via `sentinel_detect`:

- `$lang_opaque` with `lang: "lua"` looks up the tag in the
  registry and pushes the value back onto the stack.
- `$lang_opaque` with any other `lang` value hard-errors with a
  clear message — cross-language opaque values are not
  reconstructable by the Lua spec.
- `$ref` reads bytes from the process-wide store and pushes them
  as a Lua string.
- `$function_pointer` hard-errors with a "wrapper-binary
  subsystem not yet implemented" message that names the
  amendment's design as the future target.

## Validation

- **Unit tests** (7 passing under `tests/020-sentinels-test.c`)
  cover kind detection, emit-then-detect round trips for all
  three kinds, plain-object false-positive avoidance,
  `$ref` store alloc/lookup/clear, capability gap detection,
  and kind-name strings.
- **End-to-end emit test**
  (`tests/maps/318-lang-opaque/`) wires a Lua producer to BOTH a
  Lua consumer and a `write` box. The `write` box's
  language-agnostic kind forces the producer's output to JSON,
  and the producer's output table contains a Lua function. The
  test runner verifies the producer's output JSON carries the
  expected `$lang_opaque` sentinel shape including `"lang":
  "lua"` and `"shape": "function"`.

## Documented limit on intra-Lua $lang_opaque reconstruction

The end-to-end reconstruction of `$lang_opaque` even in the
intra-Lua case has a deeper structural issue worth flagging
clearly:

- The dual-ring slot's per-edge `input_native` flag for a
  Lua → Lua wire is 1 (native), because graph-load
  classification only knows the producer/consumer languages
  match — it doesn't know the producer DID actually write JSON
  (which only happens at call time when the producer has at
  least one cross-language consumer).
- When the consumer reads, it sees the JSON bytes but is told
  by `input_native[i] = 1` that they're native, so it skips the
  JSON-parse path and falls through to raw-string handling.
- The consumer ends up with the JSON text as a Lua string
  rather than a parsed value with the sentinel reconstructed.

Fixing this requires the dual-ring slot to track per-CELL the
actual form written (not just per-edge), and the consumer's
read path to consult that per-cell flag. The dual-ring design
already has the machinery (each cell knows which ring it came
from); plumbing it through the input-decode path is the
follow-on. Tracked here as "intra-Lua $lang_opaque round-trip
needs per-cell native flag at read time" — separate from the
amendment's wrapper-binary subsystem.

## What's deferred to follow-on slices

- **`$function_pointer` reconstruction.** The amendment's
  wrapper-binary + persistent-subprocess design is fully
  specified but not implemented. Producer emit writes the
  shape; consumer reconstruct rejects with a clear message.
- **C / Bash sentinel emit-side wiring.** C and Bash specs
  declare their capabilities but don't yet emit sentinels from
  their `native_to_json` — typed-wrapper sentinel emission is a
  C-spec enhancement, Bash has no native sentinel-emit case
  since its values are always strings.
- **Compile-time wire validation.** Capability masks are
  declared; the compile-step walker that checks each cross-
  language wire against producer.emit_mask vs
  consumer.reconstruct_mask and surfaces warnings is a
  separate slice.
- **Lifetime-correct `$ref` store.** Current store is leak-
  per-run (`sentinel_ref_alloc` + `sentinel_ref_lookup`,
  cleared at process exit or via `sentinel_ref_store_clear`).
  Refcount-driven release tied to consumer reads is the
  follow-on.
- **Per-cell native-flag on the dual-ring slot** so intra-Lua
  $lang_opaque reconstruction round-trips correctly when the
  producer was forced to JSON by a cross-language sibling
  consumer.

## Amendment (2026-05-22) — function pointers via wrapper binaries, not FFI

## Amendment (2026-05-22) — function pointers via wrapper binaries, not FFI

The original design had the consumer's spec reconstruct a
`$function_pointer` by FFI-casting the raw process-local
address (LuaJIT's `ffi.cast`, ctypes, etc.). That path is
language-pair-specific by nature: it only works for languages
that ship an FFI binding to the target language's ABI, which
biases SoraMech toward the Lua↔C corner and leaves Bash and
any future agnostic language out in the cold. SoraMech's
cross-language story must be agnostic — N spec implementations,
not N² pair-wise paths — so FFI reconstruction is the wrong
shape.

**The replacement design:**

1. **At map-compile time**, when the compiler sees a
   `$function_pointer`-shaped wire crossing a language
   boundary, it asks the *producing* spec to compile the
   referenced function (and its enumerable dependencies) into
   a standalone binary on disk. The binary speaks a small
   request/response protocol over stdin/stdout or, when
   performance matters, a Unix domain socket the runtime
   spawned at startup.
2. **The compiler then asks the *consuming* spec for a
   wrapper recipe**: "in your language, how do I invoke a
   subprocess with these arguments and parse the response?"
   Every language can answer that question — Lua via
   `io.popen` / `socket.connect`, C via `execve` /
   `socket(2)`, Bash via literal `$(./bin args)` or
   `socat` / `nc`. The consuming spec stamps a wrapper
   function on disk (or inlines it into the merged module
   per issue 313) that the consumer's source can call as if
   the foreign function were local.
3. **The `$function_pointer` sentinel's payload becomes**:

   ```json
   {"$function_pointer": {"binary": "compiled/cross-lang/<box>__<port>.bin",
                          "signature": "int(int,int)",
                          "socket":    "compiled/cross-lang/<box>__<port>.sock"}}
   ```

   The `socket` field is present when the runtime started the
   binary as a long-lived subprocess and routes calls through
   the socket; absent when the binary is spawned per call
   (the cheap-and-stateless fallback).

4. **State preservation comes from the persistent subprocess.**
   The threading roadmap already commits to Unix domain
   sockets for cross-language IPC; the same mechanism is what
   makes long-lived foreign functions work. Each cross-language
   function pointer used by a running map gets one subprocess
   spawned at map start (or first use), and the wrapper does
   socket round-trips per call. Latency: tens of microseconds,
   not the millisecond cost of fork+exec. Closure state,
   loaded models, cached connections — all survive between
   calls in the subprocess's own memory.

5. **Argument enumeration is the user's responsibility at
   map-compile time.** Dynamic-language closures capture
   upvalues (Lua) and `_ENV` (Lua) and lexical scope (most);
   statically enumerating those is the FFI problem under
   another name. The map compiler walks what it can — declared
   inputs and statically reachable references — and fails the
   compile with a clear error if the function pointer's
   closure can't be enumerated. Honest failure mode.

### New spec responsibility: foreign-call wrappers

This adds one responsibility to `lang_spec_t`. Each spec
provides a function that emits a wrapper in its own language
given a foreign-call protocol descriptor:

```c
int (*emit_foreign_call_wrapper)(void *handle,
                                 const char *binary_path,
                                 const char *socket_path_or_null,
                                 const char *signature,
                                 const char *out_wrapper_src_path);
```

- For Lua: writes a `.lua` file containing a function that
  opens the socket (or popen's the binary) and round-trips
  arguments / response.
- For C: writes a `.c` file with the same logic, compiled
  alongside the rest of the C-side merged module.
- For Bash: writes a function definition that wraps a
  `socat`-or-`nc`-or-`./bin` call.

The wrapper-recipe is the *only* per-language code the
foreign-call story needs. The binaries themselves are produced
by the producing spec's existing `compile` function. The
protocol on the wire is a single shared definition — not
per-language — so the consuming spec doesn't need to know what
language wrote the binary.

### What this leaves of the original sentinel design

The two other sentinels survive unchanged:

- **`$ref`** — pointer + length into the large-value heap; same
  in-process semantics as before, same cross-process inlining
  at the UDS marshaller.
- **`$lang_opaque`** — still a "this language's private value,
  consumer either understands or fails compile" sentinel; the
  fail-compile path stays the default.

`$function_pointer`'s wire shape stays the same JSON-object
form; only the reconstruction recipe changes. Specs that used
to publish "I can reconstruct `$function_pointer` via FFI" in
their capability declaration now publish "I can emit a
foreign-call wrapper" instead — same capability slot, different
meaning.

### Why agnostic wins here

LuaJIT FFI is fast and would have been the easy path for
Lua↔C. But locking SoraMech's wire format to LuaJIT's ABI
expectations would (a) tie the project to one specific Lua
implementation, (b) leave Bash with no `$function_pointer`
reconstruction at all, (c) require every future language we
add to either ship an FFI binding or be a second-class
citizen, and (d) make windows / non-POSIX targets harder than
they need to be. Subprocess-and-socket is slower per call than
FFI but it works everywhere, for every language, today, and
on every OS that speaks sockets. The cost is real but bounded;
the agnosticism is structural and worth more.

## Concept

JSON's structure covers six kinds of value cleanly — object,
array, number, string, bool, null — and the cross-language ring
in the dual-ring slot (issue 312) carries JSON for any wire that
crosses a language boundary. The structural cases that don't fit
those six kinds — Lua closures, libcurl handles, C++ class
instances, file handles, function pointers, large byte buffers
where serialization would be expensive — need somewhere to live
in the JSON ring without forcing the wire format to grow new
top-level shapes.

**Sentinel primitives** are JSON object shapes with reserved key
names that mean "this isn't a structural value, it's a reference
to something the consumer must reconstruct using language-specific
machinery."

Three kinds, each solving a different structural gap:

```json
{"$ref":              {"ptr": "0x...",  "len": 1048576,
                       "type": "bytes"}}
{"$function_pointer": {"addr": "0x...", "signature": "int(void*,void*)"}}
{"$lang_opaque":      {"lang": "lua",   "tag": 12345,
                       "shape": "coroutine"}}
```

Sentinels are **emitted by the producer's spec** when its language
has a value JSON can't represent structurally. They are
**reconstructed by the consumer's spec** using whatever runtime
machinery that language provides (LuaJIT FFI, ctypes for Python,
direct cast for C, etc.). Wires whose consumer cannot reconstruct
a sentinel kind **fail at compile time** — the editor catches the
mismatch when the user wires a function-pointer output into a
Bash input.

## Why these three, not more

The three sentinels carve up the structural gap by ownership:

- **`$ref`** — bytes in a shared heap. The producer wrote the
  bytes to the large-value heap (issue 302 / `src/015-large-value-heap.c`)
  and emits a pointer-and-length reference. The bytes are
  immutable for the wire's lifetime; refcount tracks consumers.
  Solves: "I have a megabyte to share; copying it through the
  slot would be wasteful."
- **`$function_pointer`** — a callable, identified by a raw
  address and a signature interpretable in C-function-pointer
  syntax. The address is process-local. The signature is the
  contract between producer and consumer. Solves: "I have a
  function I want my consumer to be able to call directly."
- **`$lang_opaque`** — a language-private value. The producer
  tags it with the language name and a worker-private reference
  the producer's spec can resolve back. Solves: "I have a value
  with no general structural representation; only my own
  language knows what it is."

Anything that doesn't fit one of the three either (a) has a
JSON-structural representation and shouldn't be a sentinel, or
(b) is genuinely unrepresentable and the wire is wrong, not the
format.

## Producer protocol

Each language spec's `native_to_json` walks its native values and
emits the appropriate sentinel when it encounters a non-structural
value. The spec author writes the emit logic; it's not generic.

### Lua

`encode_value` in `langs/lua/spec.c:188` currently hard-errors on
function / userdata / thread / lightuserdata. The extension:

- **Function** (`LUA_TFUNCTION`) — if the function is a C function
  registered with FFI, emit `$function_pointer` with its FFI
  signature; if it's a Lua function, emit `$lang_opaque` with
  `lang: "lua"` and a tag the spec can use to resolve the
  function back later (a registry index, for example).
- **Userdata** with an FFI ctype — emit `$function_pointer` if
  the type is a function pointer; `$ref` if the type is a sized
  buffer; `$lang_opaque` otherwise.
- **Coroutine** (`LUA_TTHREAD`) — `$lang_opaque` with
  `shape: "coroutine"`.
- **Large strings or tables** — at the spec's discretion, the
  producer may write the value to the large-value heap and emit
  `$ref` instead of inlining the bytes. Decision is per-call,
  driven by size threshold or output annotation.

### C

The C spec's generated wrapper (issue 307) knows the box's
declared output type. The wrapper emits the right sentinel
shape based on the type:

- A declared `function_pointer` return — `$function_pointer`
  with the type's signature.
- A declared `bytes` return whose size exceeds the inline
  threshold — `$ref` with the bytes written to the heap.
- A declared opaque struct — `$lang_opaque` if the struct's
  shape is C-only.

### Bash

Bash has no native values besides strings; sentinels don't apply
on the emit side. A Bash producer always emits structural JSON
strings.

## Consumer protocol

Each spec's `json_to_native` walks the JSON tree and, when it
sees a sentinel key, reconstructs the value using its language's
machinery. The reconstruction is the spec author's responsibility.

### Lua

**Superseded by the Amendment (2026-05-22)** for the
`$function_pointer` case — the FFI-cast reconstruction below
is no longer the target; instead, the consumer's spec emits a
foreign-call wrapper that round-trips through a compiled binary
or a long-lived subprocess. `$ref` and `$lang_opaque`
reconstruction below remains current.

```c
// inside lua_json_to_native, on seeing {"$function_pointer": {...}}:
const char *sig = json_string_value(obj, "signature");
uintptr_t   addr = json_int_value(obj, "addr");
lua_getglobal(L, "ffi");
lua_getfield(L, -1, "cast");
lua_pushstring(L, sig);                 // "int (*)(void*, void*)"
lua_pushinteger(L, (lua_Integer)addr);
lua_call(L, 2, 1);
// stack top: an FFI callable Lua can invoke as compare_fn(a, b)
```

`$ref` reconstruction in Lua copies the bytes (`lua_pushlstring(ptr, len)`)
since Lua's value model needs values in Lua's heap. The
producer's refcount handles the lifetime.

`$lang_opaque` with `lang: "lua"` resolves the tag through the
worker's per-worker registry; with `lang != "lua"` it's a wire
compile error.

### C

Direct casts. `$function_pointer` becomes a typed function
pointer the wrapper can invoke. `$ref` is a `(void*, size_t)`
pair the wrapper hands to the user's function. `$lang_opaque`
with `lang: "c"` is a typed-struct pointer; other langs fail.

### Bash

Bash cannot reconstruct any sentinel except possibly `$ref`
(by reading the bytes from the file the marshaller staged at the
process boundary). `$function_pointer` and `$lang_opaque` fail
unless the producer chose the cross-process escape hatch (see
below).

## Compile-time validation

The compile step walks every wire and checks:

- If the producer's declared output type can emit sentinel kind
  K (per the producer language's spec capability), then
- The consumer's language must be able to reconstruct sentinel
  kind K (per the consumer language's spec capability).
- If not, **the compile fails** with the specific wire and the
  specific sentinel kind named.

This is editor-time feedback. The user fixes the wire
(reroute it, change the consumer's language, attach a custom
translation per issue 246) before the program ever runs.

The per-spec capability is a static fact each spec declares.
Adding a new language K declares K's emit set and reconstruct
set; the compile validator consults the declaration.

## Cross-process escape hatch for `$function_pointer`

**Superseded by the Amendment (2026-05-22)** — what this section
described as an opt-in escape hatch is now the *only* path. The
default-is-FFI / escape-hatch-is-binary framing below is no
longer accurate; both cases now use a compiled binary, optionally
backed by a long-lived subprocess + socket. Keeping the text for
context.

Bash (and any other process-isolated language) can't dereference
an in-process function-pointer address. Two options when a
function pointer's wire targets such a consumer:

1. **Fail at compile time** (the default). Clear, honest.
2. **Producer-side compiled binary** (opt-in). When the C spec
   detects that a function-pointer output is wired to a
   cross-process consumer, it compiles the function into a
   standalone executable in `compiled/cross-process/<box>__<port>.bin`.
   The sentinel becomes:

   ```json
   {"$function_pointer": {"binary": "compiled/cross-process/...",
                          "signature": "int(int,int)"}}
   ```

   Bash invokes the binary with `$(./bin args...)` per call. Cost:
   process spawn per invocation (1-10 ms). The escape hatch is
   opt-in per box output — the user explicitly chooses the cost
   in the inspector.

The default behaviour is option 1 because hiding the spawn cost
would mislead the user about runtime behaviour. The user picks
their language; the cost follows from the choice. Option 2 is
the escape hatch for cases where the user genuinely needs
C-from-Bash and accepts the cost.

## Relationship to custom translation specs (issue 246)

Sentinels are the **spec author's tool** for representing
non-structural values. Custom translations are the **user's
tool** for interpreting them (or any other value) when the
default reconstruction isn't what they want.

The escalation path:

1. Default spec encode / decode handles JSON-structural values
   transparently.
2. When the producer's value can't be encoded structurally, the
   spec author's emit logic produces the right sentinel.
3. When the consumer's spec can reconstruct the sentinel
   generically (FFI cast, ctypes, direct pointer), the default
   reconstruction runs.
4. When the user wants a different interpretation than the
   default (treat a function pointer as a method on a class
   instance, dereference a `$ref` and parse its bytes as a
   custom format, etc.), they attach a per-port custom
   translation that overrides the default reconstruction.

So sentinels and custom translations are layered: sentinels
provide the *bytes that arrive at the wire* for non-structural
values; custom translations decide *what to do with those bytes*
when the default isn't enough.

## Where in the dispatch this lives

Sentinels live inside the JSON ring of the dual-ring slot. The
slot store (issue 312) doesn't know about them — it just carries
bytes. The producer's `native_to_json` emits them when writing
to the JSON ring; the consumer's `json_to_native` recognizes
and reconstructs them when reading from the JSON ring.

Inside the native ring, sentinels don't appear — native values
are encoded in the language's own binary form, which can
represent its own callables / opaque values directly without
needing a JSON-side hack.

## Relevant files

- `langs/lang-spec.h` — bridges (`native_to_json`,
  `json_to_native`) are where sentinel handling lives per spec.
- `langs/lua/spec.c:188` (`encode_value`) — emit-side extension
  point. Currently hard-errors on function / userdata / thread;
  this issue replaces those errors with sentinel emission.
- `langs/lua/spec.c:370` (`decode_node`) — reconstruct-side
  extension point. Add sentinel recognition before generic
  object handling.
- `langs/c/spec.c` — C spec's `native_to_json` / `json_to_native`
  (currently NULL); both grow when this issue lands.
- `langs/bash/spec.c` — Bash spec; only `$ref` is reconstructable.
- `src/015-large-value-heap.c` — `$ref` storage.
- `scripts/soramech-compile.sh` — compile-time wire validation
  walks producer/consumer sentinel capabilities.
- `issues/312-same-language-wire-fast-path.md` — dual-ring shape;
  sentinels ride inside its JSON ring.
- `issues/246-custom-translation-specs-per-port.md` — user-side
  override layered on top of sentinel reconstruction.
- `docs/007-architecture.md` — wire format section; gains a brief
  sentinel reference.

## Suggested implementation sequence

1. **Define the canonical JSON shapes**. Three sentinel kinds,
   exact key names, value-object schemas. Document in
   `docs/007-architecture.md`.
2. **Lua emit**: replace the hard-errors in `encode_value` with
   sentinel emission per the producer protocol above. Start with
   `$function_pointer` for FFI callables and `$lang_opaque` for
   Lua functions / coroutines.
3. **Lua reconstruct**: extend `decode_node` to recognize the
   three sentinels and reconstruct them.
4. **Per-spec capability declarations**. Each spec declares its
   emit set (sentinel kinds it can produce) and reconstruct set
   (sentinel kinds it can consume). Lives next to the
   `lang_spec_t` declaration.
5. **C spec sentinels**. Emit via the generated wrapper based on
   declared output types; reconstruct via direct cast.
6. **Bash spec sentinels**. Only `$ref` on the reconstruct side
   (read bytes from the marshaller's staged file).
7. **Compile-time validation**. Walk every wire; check the
   producer's emit set against the consumer's reconstruct set;
   fail with a clear message on mismatch.
8. **Cross-process escape hatch for `$function_pointer`**.
   Opt-in flag in the box output declaration; the C spec emits a
   standalone binary instead of (or alongside) the in-process
   address.

## Open questions

- **`$ref` lifetime across iterator-fed slots**. A producer's
  output is referenced by an N-cell ring of consumer slots; each
  cell holds the `$ref`, each pop decrements. Already supported
  by the large-value heap's refcount, but worth verifying the
  test coverage when sentinels land.
- **Sentinel inside sentinel.** A `$ref` whose bytes are
  themselves JSON containing a `$function_pointer` is legal but
  weird. Document the recursive case; probably falls out of the
  walker for free.
- **Versioning the sentinel format**. The reserved key names
  (`$ref`, `$function_pointer`, `$lang_opaque`) and the
  value-object schemas are part of the compile-time contract.
  Changing them is breaking. Worth a one-line stability note
  somewhere.
- **Cross-process `$ref` semantics**. In-process `$ref` is a raw
  pointer + length. Cross-process (Bash via UDS), the marshaller
  stages the bytes at the socket boundary. Decide whether the
  Bash-side `$ref` reconstruction reads from a file path or
  inlines the bytes in the request. The latter is simpler; the
  former scales to bigger values.
- **`bytes` returns on the C spec currently passthrough raw bytes
  into the JSON ring.** The C spec's typed-output-JSON path
  (issue 307 item 2) wraps strings in JSON quotes and passes
  ints / doubles / bools through as valid JSON fragments — but for
  a declared `bytes` return it writes the raw payload verbatim,
  which is *not* valid JSON in the general case (non-UTF-8 bytes
  break parsers; embedded quotes / backslashes mangle the wire).
  The proper resolution belongs here, not in 307: small `bytes`
  outputs should be base64-wrapped into a JSON string (or its own
  sentinel kind), and large `bytes` outputs should follow the
  `$ref` path already described for the heap. Until that lands,
  cross-language `bytes` wires from C producers are an honest
  hazard — flag in the compile-time validator if possible, or at
  minimum document the gap so users don't wire a `bytes`
  C-output into a Lua / Bash consumer expecting clean JSON.
