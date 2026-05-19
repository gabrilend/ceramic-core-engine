# 306 — Lua language spec implementation (reference spec)

## Status
open

## Current behavior
Lua boxes today run via `drivers/lua.sh`, which spawns a fresh `luajit`
process per box invocation, feeds it stdin, and reads stdout. Process
spawn cost dominates; each call is its own fresh interpreter. There is
no persistent Lua state between calls.

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
    set_package_path(L);   // libs/, map src/, meta.json src_dirs
    return L;
}

void lua_teardown(void *handle) {
    lua_close((lua_State *)handle);
}
```

`set_package_path` writes `package.path` and `package.cpath` to
include exactly one directory: the map's own `src/`. Everything the
map needs — vendored libs, helper modules, anything — is copied into
`src/` by the compile step (issue 222). The compiled map is then a
self-contained directory: ship the directory to someone else and it
runs without further setup.

This is a tightening of the phase 2 path, which also included
SoraMech's central `libs/` and any `meta.json src_dirs`. The new rule
is: one place to look for functions, the map's own `src/`, full stop.
Compile is the step that resolves dependencies into that location.

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

Inputs arrive at `invoke` as `(void *bytes, int size)` pairs. The
spec is responsible for converting each input's bytes into the Lua
value type the user's function expects. The user's function sees
arguments as native Lua values — never a raw byte buffer that the
function has to parse itself.

Each input has a declared type in the box JSON
(`{"name": "count", "type": "number"}`). The graph loader (issue 305)
records this type on the `input_decl_t` and passes the type alongside
the bytes when calling `invoke`. The Lua spec dispatches on type:

| Declared type | Lua spec action                                             |
|---------------|-------------------------------------------------------------|
| `string`      | `lua_pushlstring(L, bytes, size)`                           |
| `number`      | parse with `strtod`, `lua_pushnumber`                       |
| `integer`     | parse with `strtoll`, `lua_pushinteger`                     |
| `boolean`     | strcmp against `"true"` / `"false"`, `lua_pushboolean`      |
| `json`        | call `dkjson.decode` on the bytes, push the resulting table |
| `bytes`       | `lua_pushlstring` (raw, may contain nulls)                  |
| `any`         | treat as `string` for now                                   |

The Lua function then takes typed arguments:

```lua
function M.add(a, b)            -- both already numbers
    return a + b
end

function M.classify(payload)    -- payload is already a table
    return payload.kind
end
```

The user does not write `tonumber` or `dkjson.decode` calls inside
their box function. The spec did that work before the function was
called.

If a parse fails (`json` input that isn't valid JSON, `number` input
that isn't a number), the spec returns nonzero and the dispatch layer
aborts (per issue 303).

## Return marshalling

The Lua function returns one or more values. The spec converts the
return into bytes for the output slot:

| Return shape           | Spec action                                                   |
|------------------------|---------------------------------------------------------------|
| Single string          | `lua_tolstring` → copy into `out_buf`                         |
| Single number          | `snprintf("%.17g", ...)` into `out_buf`                       |
| Single boolean         | write `"true"` or `"false"`                                   |
| Single table           | `dkjson.encode` into `out_buf`                                |
| Single nil             | `*out_size = 0` (zero-byte output)                            |
| Multiple values        | wrap into a table `{v1, v2, ...}` and `dkjson.encode`         |

`nil` is a valid return. The spec writes zero bytes and sets
`*out_size = 0`. Downstream consumers receive a zero-byte slot value;
how they interpret it depends on their declared input type (typed
`number` consumer fails to parse; typed `string` consumer gets an
empty string; typed `json` consumer parses zero bytes as `null` via
dkjson). We cannot demand that user functions never return nil, so we
handle it.

Multiple return values are wrapped into a table and JSON-encoded.
Phase 2 silently dropped extra returns; phase 3 keeps them — the
serialized form is `[v1, v2, ...]`.

Output buffer is sized to the box's declared `output_capacity`
(issue 305's `box_t`). If the serialized value exceeds that, abort
(per the hard-crash policy in issue 303). For variable-size outputs
the box declares `output_capacity = 0` and uses the large-value heap
(issue 302); the spec writes into that heap and stores the handle in
`out_buf`.

dkjson is the table serializer in both directions (input parsing
above and output serialization here). It is already vendored.

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

Deferred:
- **Rich return types.** The current invoke serializes the return
  with `lua_tolstring`, which handles strings/numbers/booleans/
  nil via tostring semantics. Tables, closures, and other
  structured values land with the fast-path work in issue 312.

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
