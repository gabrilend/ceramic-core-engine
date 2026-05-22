# 306 — Lua language spec implementation (reference spec)

## Status
complete

## Current behavior
`langs/lua/spec.{c,Makefile}` builds `langs/lua/spec.so`, dlopened by
the pool runner via the spec registry (issue 303). Each worker thread
gets its own `lua_State`; states are not shared and need no locking.
Per-worker module cache keyed by file path lets the same source be
reused across calls without reloading. Input bytes are pushed as
either native Lua strings or parsed JSON values per the per-edge
classification the loader hands the dispatch layer (issue 312); return
values are written back as either raw strings (single-string path) or
JSON (everything else) via the project's JSON encoder. Errors at any
step (file load, chunk run, function lookup, pcall) surface as
nonzero return with a stderr line and crash the dispatch layer per
issue 303.

## Concept
The Lua language spec implements `lang_spec_t` (issue 303) for Lua. It
is the first reference implementation of the spec interface and the
proof that the in-process FFI model works. It is also the simplest
case — no compile step, no out-of-process server.

Each worker thread gets its own `lua_State`. States are not shared.
Per-thread states need no locking on the call path; the state's
internals are only ever touched by the worker that owns it.

`langs/lua/spec.c` is built into `langs/lua/spec.so` and loaded by
the pool runner via `dlopen` at startup, like any other spec.

## Per-worker state

Init creates a fresh `lua_State` and configures it:

```c
void *lua_init(int worker_idx) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    // Seed an empty per-worker module cache in LUA_REGISTRYINDEX
    return L;
}

void lua_teardown(void *handle) {
    lua_close((lua_State *)handle);
}
```

Box source file paths arrive at `invoke` already resolved against the
map directory by the dispatch layer (issue 304) — the spec does not
manipulate `package.path`. Maps are expected to be self-contained:
everything the map's Lua needs lives under the map's own `src/`, and
the compile step (issue 222) is what resolves dependencies into that
location.

## File-and-function caching

A naive `invoke` would `luaL_loadfile` and look up the function on
every call. Both are unnecessary work — the file's chunk does not
change during a run, and the function reference is stable.

The Lua spec keeps a per-handle cache: for each box source file
loaded, store the table returned by the file's chunk as a Lua
registry key (`LUA_REGISTRYINDEX`). On first invocation:
1. `luaL_loadfile(L, file_path)` — loads the chunk.
2. `lua_pcall(L, 0, 1, 0)` — runs the chunk, expecting a returned
   table.
3. Store the table in the registry under a key derived from `file_path`.

On subsequent invocations:
1. Look up the registry entry by `file_path`.
2. Index it by `fn_name` to get the function reference.
3. Skip `luaL_loadfile` entirely.

The cache is per-`lua_State` and therefore per-worker; no
synchronization is needed.

## Box file convention

A Lua box file returns a table whose keys are the box's exported
function names:

```lua
-- maps/<name>/src/classify.lua
local M = {}

function M.classify(text)
    if text:match("^%d+$") then return "numeric"
    elseif text:match("[a-z]") then return "alpha"
    else return "other"
    end
end

return M
```

The box JSON references it by `ref = "classify.lua"`, `fn = "classify"`.
The spec loads `classify.lua`, stores its returned table, and indexes
into it with `"classify"` to get the function.

This is the existing phase 2 convention — kept as-is.

## Input marshalling

Inputs arrive at `invoke` as `(bytes, size, is_native)` triples. The
`is_native` bit per input is set by the graph loader's per-edge
classification (issue 312) — true when the producer feeding this port
is a call box in the same language (Lua → Lua), false when bytes are
JSON-encoded by an upstream of a different language (or by anyone
deliberately writing JSON).

- `is_native == 1` → `lua_pushlstring(L, bytes, size)`. The user's
  function sees the bytes as a Lua string and is free to call
  `tonumber`, `tostring`, etc. on them — same convention as the
  phase 2 driver.
- `is_native == 0` → parse the bytes as JSON via the project parser
  and push the resulting Lua value (table for arrays/objects,
  number/string/boolean/nil for primitives). On parse failure the
  bytes are pushed as a raw string so that producers still emitting
  non-JSON keep working (transitional fallback inherited from
  issue 312's slice 4).

The original "declared input type" idea — a per-port type tag that
the spec would honor — never shipped. Per-edge classification proved
sufficient: the producer's language and the consumer's language are
known at load time, which is enough to decide native-or-JSON per
edge. Adding a per-port type tag would have duplicated that decision
in two places.

## Return marshalling

The Lua function returns one or more values. The spec converts the
return into bytes for the output slot, with the format chosen by an
`output_native` flag passed in from the dispatch layer (set by the
loader's per-connection classification):

- `output_native == 1` and a single string return → `lua_tolstring`,
  raw bytes into `out_buf`.
- Anything else (multi-return, non-string, or `output_native == 0`)
  → JSON-encode via the project's `json_writer_t`. Lua tables become
  JSON arrays or objects depending on shape; primitives become
  their JSON spellings; `nil` becomes a zero-byte output.

`nil` is a valid return — zero bytes, downstream consumers
interpret per their wire's classification (a native-input port sees
an empty string; a JSON-input port parses zero bytes as `null`).

Multiple return values are wrapped into a table `{v1, v2, ...}` and
JSON-encoded. Phase 2 silently dropped extras; phase 3 keeps them.

Output buffer is sized from the producer's declared
`output_capacity` (issue 305). If the serialized value exceeds that,
abort per the hard-crash policy in issue 303. Variable-size outputs
declare `output_capacity = 0` and use the large-value heap (issue
302); the spec writes through the same `out_buf` pointer and the
slot store handles the heap path transparently.

The JSON encoder is the project's own (`libs/json/json.c`, designed
in issue 314) — dkjson was the original plan but the project parser
shipped first and avoided a vendored Lua dependency on the encode
path.

## Error handling

Every `lua_pcall` is wrapped:
```c
int rc = lua_pcall(L, n_args, 1, 0);
if (rc != LUA_OK) {
    fprintf(stderr, "[lua spec] %s:%s — %s\n",
            file_path, fn_name, lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;  // dispatch layer aborts the program
}
```

Lua errors (`error(...)`, runtime errors, syntax errors during file
load) all surface here as nonzero `pcall` returns. The spec prints
the box context and the Lua error message to stderr, then returns
nonzero. The dispatch layer aborts.

There is no try-and-recover. Per issue 303, errors crash the program.

## Build

```
langs/lua/
    spec.c          ← lang_spec_t implementation
    Makefile        ← builds spec.so
    spec.so         ← built artifact (gitignored)
```

The Makefile target:
```make
spec.so: spec.c
	$(CC) -shared -fPIC -O2 -Wall \
	    -I$(LUA_INCLUDE) \
	    -o $@ $< \
	    -L$(LUA_LIB) -lluajit-5.1
```

LuaJIT is the assumed runtime — it matches the phase 2 driver and the
project preference (LuaJIT-compatible Lua, no 5.4 syntax). The build
links against the system LuaJIT shared library.

## Open questions

(none currently — earlier questions resolved as follows:)

- nil return: valid. Zero-byte output, downstream consumers
  interpret per their declared input type.
- Multiple return values: wrapped into a table and JSON-encoded via
  dkjson. No values dropped.
- Table serializer: dkjson, already vendored. Used in both
  directions (input parsing and output serialization).

## Suggested implementation sequence

1. Set up `langs/lua/` directory with a Makefile that builds an empty
   `spec.so` exporting a stub `soramech_lang_spec`.
2. Implement `init` and `teardown`. Verify the pool runner can
   `dlopen` the spec and call `init` per worker without errors.
3. Implement file-and-function caching — load a file, store its
   returned table in the registry, look it up later.
4. Implement input marshalling: push bytes as strings, count args.
5. Implement `lua_pcall` invocation with error wrapping.
6. Implement string return marshalling. Smoke test: `maps/hello`
   end-to-end through the pool.
7. Implement number return marshalling.
8. Implement table-via-JSON return marshalling.
9. Add `package.path` resolution for libs/, src/, and `meta.src_dirs`.
10. Test against `maps/classify-demo` and `maps/driver-test` (Lua
    portions).

## Relevant files

- `drivers/lua.sh` — phase 2 Lua driver, retired by this spec
- `langs/lang-spec.h` — interface header (issue 303)
- `issues/303-language-runtime-spec.md` — spec contract this implements
- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
  `invoke`
- `issues/302-wire-value-slot-store.md` — large-value heap for
  variable-size returns
- `libs/dkjson` (vendored) — table-to-JSON serialization
- `maps/hello`, `maps/classify-demo` — Lua-only smoke test maps

## Implementation log

### Init + invoke + teardown — 2026-05-12

`langs/lua/spec.c` now exports a real `lang_spec_t`:

- `init(worker_idx)` creates a `lua_State` and opens the standard
  libraries via `luaL_openlibs`. Returns the state as the per-
  worker handle.
- `teardown(handle)` calls `lua_close`.
- `invoke(handle, file_path, fn_name, inputs, sizes, n, out_buf,
  out_cap, out_size)` loads the file with `luaL_loadfile`,
  executes the chunk (which is expected to return a module
  table), pulls the named function out of that table, pushes each
  input as a Lua string, calls via `lua_pcall`, and copies the
  return as a string into the caller's buffer. The state's stack
  is rewound to its baseline depth at every entry/exit so
  cross-call leaks don't accumulate. Errors at every step
  surface as nonzero return and a stderr message.

Five end-to-end tests in `tests/306-lua-spec-test.c` exercise the
init/teardown round-trip, a `greet(name, salutation)` call with
both args, the same call with one arg (the Lua side defaults
salutation to "Hello"), and the two failure modes (missing
function, missing file). The test loads the spec through the
real spec registry via dlopen, so this is the actual artifact
the pool runner will consume.

(The rich-return-types follow-on noted here originally — tables /
closures — landed under issue 312's slice 4.5: tables and primitives
both serialize via the project JSON encoder.)

### Per-worker module cache — 2026-05-12

`lua_init` now seeds an empty Lua table at
`LUA_REGISTRYINDEX[LUA_CACHE_KEY]`. `lua_invoke` looks up the
box's `file_path` in that table; on a hit it skips the
`luaL_loadfile + lua_pcall` round-trip and uses the cached
module table directly; on a miss it loads, runs the chunk,
verifies the module table shape, and stashes the result in the
cache before proceeding.

The cache is per-worker (each worker has its own `lua_State`),
which means no locking. It's also per-file-path string, so two
boxes that share a source file share a module table on each
worker — calling `M.greet` from one box doesn't reload the
chunk when another box later calls `M.farewell` on the same
file.

Cache invalidation is intentionally absent for now: once a file
is loaded, its module table sticks for the run. If the box
source changes mid-run, the worker won't notice. Fine for the
read-once-then-execute usage; revisit when there's a use case
for hot-reload.
