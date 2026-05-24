# 246 — Custom translation specs per input port

## Status
complete

## Current behavior

Per-port custom translation shims are live for Lua and C boxes:

- The box JSON schema gains an optional per-input
  `custom_translation` field that names a file under
  `translations/<file>` relative to the map dir. Absence means
  "use the default decode for this port."
- The graph loader (`src/010-graph-loader.{h,c}`) reads the
  field onto `input_decl_t->custom_translation`.
- The language spec interface (`langs/lang-spec.h`) grows a
  `translate` callback that takes `(handle, shim_path, raw,
  raw_size, raw_native, out_buf, out_capacity, out_size)`.
- Lua's spec (`langs/lua/spec.c`) implements `lua_translate`:
  loads the shim file expecting it to return a function, caches
  the function reference per-worker in the Lua registry under
  `LUA_SHIM_CACHE_KEY`, calls it with `(raw, raw_native)` and
  returns the result.
- C's spec (`langs/c/spec.c`) implements `c_translate`:
  lazy-compiles the shim .c to .so via the existing
  `maybe_lazy_compile` + `get_or_load` infrastructure, dlsym's
  `sm_translate`, and calls it.
- Bash's spec doesn't implement `translate` (its protocol needs
  extension; deferred per the 319e/Bash pattern).
- Dispatch (`src/012-dispatch.c`) walks each box's inputs after
  `read_inputs`; for any port with a `custom_translation` it
  routes the raw bytes through `spec->translate`, allocates an
  output buffer from the unified allocator, replaces the per-port
  data/size, and sets `input_native[i] = 1` so the spec's invoke
  sees the translated bytes as native.
- The HTTP backend (`src/005-http-server.lua`) gains four routes:
  `GET /maps/<name>/translations` (list),
  `GET /maps/<name>/translations/<file>` (read),
  `PUT /maps/<name>/translations/<file>` (write — used by the
  inspector's "+xlate" affordance to create with default content),
  `DELETE /maps/<name>/translations/<file>` (hard delete — used
  by the file browser, not by the inspector).
- The editor's `API` module (`assets/js/003-api.js`) exposes
  `list_translations` / `get_translation` / `put_translation` /
  `delete_translation`.
- The inspector (`assets/js/004-inspector.js`) grows three button
  states per input port row on call boxes:
  - **`+xlate`** (no shim) — creates a default shim file with
    identity pass-through content matching the box's language
    and sets `port.custom_translation`.
  - **`xlate`** (shim present) — opens the shim in the
    source-view window.
  - **`×`** (shim present) — clears the box's reference to the
    shim (file stays on disk per the issue's delete rule).
- The source-view (`assets/js/008-source-view.js`) detects
  `translations/` paths and routes to `API.get_translation` so
  the `xlate` view button works.
- The wire-color module (`assets/js/006-wires.js`, from issue
  245) already paints shim-port wires in goldenrod because it
  detects `port.custom_translation`.
- The compile pipeline (`scripts/soramech-compile.sh`) copies
  `translations/` from the map into the compiled artifact so
  shims survive packaging.
- The runtime self-construction path (`src/018-runtime-builtins.c`)
  reads `custom_translation` from spec JSON in
  `runtime_create_box`, so dynamically-created boxes can carry
  shims too.

## Validation

Two integration test fixtures pass end-to-end:

- `tests/maps/246-c-shim/` — a C box whose `msg` port has a
  shim that uppercases the input. Expected output
  `seen:HELLO-FROM-SEED` confirms the shim ran before the
  box's `echo` function.
- `tests/maps/246-lua-shim/` — a Lua box whose `msg` port has
  a shim that reverses the input and prepends `REV-`. Expected
  output `seen:REV-fedcba` confirms the same on the Lua side.

All 16 integration tests pass. The slot store's 23 unit tests
still pass.

## Deferred

- **Bash spec `translate`** — the bash server's line protocol
  needs extension to route shim invocation back to the C
  runtime. Same shape of follow-on as the 319e Bash deferral.
- **Compile-time validation** of `custom_translation` paths —
  per the issue's open questions, easy add when the feature
  matures.
- **Default-content generators that match the port's declared
  type more closely** — the current default is identity
  pass-through. Type-aware decoders (e.g. JSON-parse for
  `type: "json"` inputs) are an enhancement.
- **Box-delete confirmation when ports have shims** — the
  current delete path doesn't warn. Easy add when needed; the
  files staying on disk regardless means data isn't lost,
  the user just loses the reference.

## Concept

The default language specs (issues 306 / 307 / 308) handle most
data flow via the dual-ring shape in issue 312: producer's spec
serializes native or JSON per the per-edge bit; consumer's spec
dispatches per input flag and decodes accordingly. The default
contract is sufficient for primitives, strings, arrays, and
objects — the cases JSON's structure covers cleanly and the cases
native pass-through covers cleanly.

A custom translation spec lets the user intervene at a specific
input port for **any reason** — not just cross-language
ambiguity. Two motivating cases:

**Cross-language ambiguity.** Some translations cannot be
inferred by the spec because the language constructs don't have
universal mappings:

- A C++ class with both data fields and method functions arrives
  at a C box. C has no notion of methods; whether to expose them
  as function pointers, drop them, or restructure into a tagged
  union is a development-time decision the user has to make.
- A `void *` arrives carrying a libcurl handle the receiving
  language's spec has no general way to interpret.
- A union type whose discriminator lives in a separate input
  port — the spec doesn't know which port is the tag.

**Same-language transformation the user wants.** Even when no
language boundary is crossed, the default native pass-through
may not be what the user wants on a particular port:

- A C box emits a float; a downstream C box wants it truncated
  and rounded up before consumption. The default native pass
  copies the bytes verbatim and downstream code sees the wrong
  precision. A custom shim on the destination port applies
  truncate-then-round before invoke sees the value. No language
  ambiguity — just user-specific semantics at that wire.
- A consumer wants units normalized on one port but not on a
  sibling port (degrees → radians on the angle input, leave the
  magnitude input alone).
- A consumer wants to clamp or range-check a value before its
  function sees it.

In both cases the principle is the same: **the user has knowledge
about how this specific port should interpret its input, and the
default spec doesn't.** A per-port shim is where that knowledge
gets written down.

**A custom translation spec is a per-input-port shim** the user
writes to handle these cases. It runs at the start of the
consumer box's task, replaces the default decode (or pass-through)
for that one port, and otherwise leaves the dispatch path
unchanged.

### Why per-port, not per-box

> "the custom spec should be defined on a per-slot basis. Each
> slot should (ideally) only need to handle one particular type
> of information. That's why we have separate arguments at all,
> instead of just using void pointers for everything." —
> 2026-05-21

Per-port matches the principled reason input ports exist in the
first place: each port carries one kind of value, and the user
declares the kind. If a box has three input ports — two of them
ordinary strings and one of them a C++ class instance — only the
class-instance port needs custom translation. The two string ports
keep the default decode. A per-box shim would force the user to
write decode code for every port even when only one is weird.

### Why no output-side custom translation

A producer's spec always knows how to serialize every one of its
own language's values losslessly — Lua's spec knows every Lua type,
C's spec knows every typed struct field. Writing depends only on
the sending language, which has full reflective access to its own
type system. The Lua spec's `encode_value` is expected to grow
sentinel primitives (`$function_pointer`, `$lang_opaque`) for the
values JSON can't represent structurally; that's a spec-author
responsibility, not a user-shim responsibility.

Reading is the asymmetric direction. The receiver doesn't know
what the sender meant by an opaque pointer or a sentinel-tagged
value — that interpretation is the user's call. Hence the custom
shim lives on the input side only.

## Lifecycle (editor)

The box inspector's input-port rows each grow a small affordance
for managing the port's custom translation:

- **New** — creates a translation file in
  `maps/<name>/translations/<box_id>__<port_name>.<lang_ext>`
  and sets the port's `custom_translation` field in the box JSON
  to that path. The file is initialized to a copy of what the
  default spec would do for that port's declared type — the user
  can run it without edits and behaviour is identical to no
  custom shim. Edits add or replace the decode logic.
- **View** — opens the translation file in the editor's read-only
  source view (same affordance as "view source" on the box's main
  function). The user-facing message is "this is what gets baked
  into the map at compile time."
- **Delete** — clears the port's `custom_translation` field. The
  translation file itself stays on disk. Box-delete (the whole
  box) with any custom translations attached prompts: "this box
  has N custom translation specs; the files stay on disk for
  re-use, but the box's references will go." Confirm or cancel.

The "file stays on disk if you delete the box" rule means custom
translations are identifiable by path, not by box. The user can
attach the same file to a different box's port later, or delete
the file explicitly through a file-browser pane.

## Lifecycle (runtime)

The dispatch's input-read step gains a per-port branch:

```
for each input port i:
    (which_ring, idx) = ring_order_pop(slot)
    raw_bytes         = ring_at(...)
    if box->inputs[i].custom_translation:
        translated[i] = custom_translation[i](raw_bytes, which_ring)
        input_native[i] = NATIVE   # custom shim output is always native
    else:
        translated[i] = raw_bytes
        input_native[i] = (which_ring == NATIVE)
```

When the port has a custom shim, the shim does the decode and the
spec's `invoke` sees native bytes (with `input_native[i] = NATIVE`).
When the port has no custom shim, the default decode runs inside
the spec's invoke as today.

Three consequences fall out from running on the consumer's worker:

- A custom shim that does something slow or surprising holds the
  consumer's worker, not the producer's. The producer is free as
  soon as it's done writing raw bytes to the ring.
- Each consumer eats its own translation cost; mixed fan-in
  consumers translate cell-by-cell as they pop.
- The custom shim is in the same language as the box's main
  function, so it has full access to that language's runtime
  (Lua tables, C structs, etc.) without crossing another
  language boundary first.

## Where it sits relative to the dual-ring (issue 312)

The dual-ring routing still applies. The producer chooses a ring
per `output_edge_native[j]` as today. The consumer's per-port
custom shim sees the raw bytes plus the `which_ring` tag — so it
can distinguish "these bytes are my own language's native format"
from "these bytes are JSON from a cross-language producer" and act
accordingly.

In the easy cases (same-language same-shape), no custom shim is
needed; the default native pass-through still works. The custom
shim earns its keep on the cells where the default
`json_to_native` would error or produce something useless — that's
where the user-written code takes over.

## File layout

```
maps/<name>/
    boxes/
        <box_id>.json                ← per-input port may reference translations/
    translations/
        <box_id>__<port_name>.<lang_ext>     ← per-port shim
    src/
        <box_id>.<lang_ext>                  ← box's main function
```

The box JSON's `inputs` array grows an optional per-port field:

```json
{
    "id": "consume_class",
    "lang": "c",
    "ref": "consume.c",
    "fn": "consume",
    "inputs": [
        { "name": "instance", "type": "bytes",
          "custom_translation": "translations/consume_class__instance.c" },
        { "name": "weight",   "type": "int" }
    ],
    "...": "..."
}
```

If a port omits `custom_translation`, the default-spec decode runs
for that port as today. The `translations/` directory may contain
shims with no attached port (orphaned, kept on disk by the
delete-doesn't-delete-file rule).

## Custom translation function signature

Per-port shims take one input's raw bytes and the `which_ring`
flag, return one translated value in the consumer's native form.
Working draft for C:

```c
int sm_translate(const void *input_raw, int input_size,
                 int input_native,
                 void *output_native, int output_capacity,
                 int *output_size);
```

Lua's shape is analogous but in Lua-native terms — the function
takes a Lua string (the raw bytes) and a boolean (the native
flag), returns the translated Lua value.

One signature per port keeps each shim's surface area exactly
matched to the port's job. A shim that needs to consult other
ports has a problem this design doesn't try to solve (per-port
isolation is the principled choice); the user can fall back to
plain decode-then-validate inside the box's main function for
cross-port logic.

## Default content of a freshly-created shim

The "New" button writes a file containing a literal copy of what
the default spec would do for that port's declared type. For C,
that's the small generated wrapper that parses `input_raw` per
the port's declared type and writes the result. For Lua, it's a
few lines that decode via the existing JSON / native helpers
based on `input_native`.

The user can run the shim without edits and behaviour matches the
no-shim default. Edits typically replace one branch (the
specific weird case) and leave the rest alone. The "starts as
default, edits in place" shape matches the project's preference
for upgrade paths over from-scratch authoring.

## Compile pipeline (issue 309)

The compile pipeline already walks every box's `ref` to copy
source into `compiled/src/`. The same walk inspects each input
port's `custom_translation` field and copies referenced files
into `compiled/translations/`. For C, each translation file
compiles to a `.so` alongside the box's main function. For Lua,
files load into the worker's `lua_State` like any other Lua
source.

When issue 313 (same-language source merge) lands, translation
files get concatenated into the per-language bundle the same way
box main functions do — a translation file is just another
function in the merged module.

## Why this is the right escape hatch

The default spec contract is "JSON is the universal interop, each
spec handles its own native form, sentinels carry the in-process
exotic cases on the producer side." That's enough for the
overwhelming majority of data flow. The cases where it fails are
exactly the cases where the user has knowledge the system can't
ever infer — what a `void *` points to, how a serialized object
is structured, which port is a discriminator for which other port.

Pushing those back to the user via a per-port shim is honest
about where the knowledge has to come from. It also lets the
default spec contract stay narrow and predictable — the spec
authors don't have to chase every possible ambiguous receive-side
type with new special cases.

## Relevant files

- `assets/js/002-boxes.js` — box state; per-input
  `custom_translation` field read/write.
- `assets/index.html` — inspector UI (New / View / Delete buttons
  on each input-port row).
- `src/006-server-main.lua` — HTTP backend for translation file
  CRUD (path validation against `maps/<name>/translations/`).
- `src/010-graph-loader.{c,h}` — load each input port's
  `custom_translation` onto `box_t->inputs[i]`.
- `src/012-dispatch.c` — input-read branch on
  `box->inputs[i].custom_translation`.
- `scripts/soramech-compile.sh` — package translation files into
  the compiled map (issue 309).
- `issues/245-wire-color-by-cross-language-classification.md` —
  goldenrod-cheddar wires for ports with custom translations.
- `issues/312-same-language-wire-fast-path.md` — dual-ring shape
  the translation slots into.
- `issues/313-research-whole-program-same-language-merge.md` —
  custom translations get merged alongside box source.

## Suggested implementation sequence

1. **Box JSON schema**: add the optional per-input
   `custom_translation` field. Graph loader reads it; absence
   means default path for that port.
2. **HTTP backend**: CRUD routes for translation files. Path
   validation against `maps/<name>/translations/`.
3. **Editor inspector**: New / View / Delete affordances on each
   input-port row. "New" generates the file with
   default-spec-equivalent contents parameterized on the port's
   declared type. The 245 wire-coloring change activates
   automatically once the field exists.
4. **Box-delete confirmation**: detect any ports with attached
   translations; prompt before deletion; preserve the files on
   disk regardless.
5. **Dispatch**: input-read per-port branch on
   `custom_translation`; route bytes through the shim instead of
   the default decode for that port.
6. **Compile pipeline**: walk each input port's
   `custom_translation`; package files into
   `compiled/translations/` and the per-language source bundle.

## Open questions

- **Shared translations across multiple ports.** Two ports on
  different boxes that need the same shim can both reference the
  same file path — the system supports it implicitly. The
  editor's "New" button always creates a fresh file with the
  default naming so users don't accidentally share by default.
  Library imports inside translation files (for shared helpers
  inside a shim) are out of scope here — the user can do that via
  the language's normal import mechanism.
- **Compile-time validation.** Should the compile button check
  that every `custom_translation` path exists and parses? Easy
  add to the existing compile validation; do once translations
  exist as a feature.
- **Shim author seeing cross-port context.** Per-port isolation
  means a shim only sees its own input. A union whose
  discriminator is in another port has to validate inside the
  box's main function rather than in the shim. If a real
  cross-port case shows up, the workaround is a follow-on
  ticket — not a reason to abandon per-port isolation.
