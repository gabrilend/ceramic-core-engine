# SoraMech — Writing boxes

Three languages ship with first-class support: Lua, C, and Bash.
Each follows the same dataflow contract — a box function takes
input values, returns one output value — but the calling shape
differs per language. This doc covers each language's quickstart.

## The contract, in plain english

A box function receives values for each declared input port and
must produce exactly one output value. The output gets pushed to
the box's outgoing wires by the runtime — your function doesn't
push or fan out anything itself; you just return.

Input port names are declared on the box's JSON. Input order
matches the JSON's `inputs[]` order. The runtime delivers values
in that order to your function.

## Lua

```lua
-- src/greeter.lua
local M = {}

-- A box function: receives one string, returns one string.
function M.greet(name)
    return "Hello, " .. name .. "!"
end

return M
```

Wired up via the box JSON:

```json
{
  "id":   "greet",
  "kind": "call",
  "lang": "lua",
  "ref":  "src/greeter.lua",
  "fn":   "greet",
  "inputs":  [ { "name": "name", "type": "string" } ],
  "routing": { "kind": "plain" }
}
```

The Lua spec keeps a `lua_State` per worker, so consecutive
fires of the same box reuse the same Lua state — module
imports and closures persist across calls within a run.

### Lua values across wires

The Lua spec serialises return values to bytes for the wire.
Strings pass through unchanged; numbers format as their JSON
representation; tables serialise to JSON; functions become a
`$lang_opaque` sentinel that consumer Lua workers can
reconstruct in their own state.

## C

```c
// src/sqr.c
#include <stdlib.h>
#include <string.h>

int square(const void **inputs, const int *sizes, int n_inputs,
           void *out_buf, int out_capacity, int *out_size)
{
    (void)n_inputs; (void)out_capacity;
    // First input is a string-encoded number.
    char tmp[64];
    int n = sizes[0] < 63 ? sizes[0] : 63;
    memcpy(tmp, inputs[0], n);
    tmp[n] = '\0';
    long x = strtol(tmp, NULL, 10);
    long r = x * x;
    int written = snprintf(out_buf, out_capacity, "%ld", r);
    if (written < 0 || written >= out_capacity) return -1;
    *out_size = written;
    return 0;
}
```

Wired up:

```json
{
  "id":   "sqr",
  "kind": "call",
  "lang": "c",
  "ref":  "src/sqr.c",
  "fn":   "square",
  "inputs":  [ { "name": "x", "type": "string" } ],
  "routing": { "kind": "plain" }
}
```

The C spec compiles `src/sqr.c` to `<map>/bin/sqr.so` at
compile-pipeline time (or to a `/tmp` artifact at first dispatch
when no compile pipeline has run), then `dlopen`s it.

### Compile hints per box

Two optional fields on the box JSON let you tune the build:

```json
{
  ...
  "cflags":    "-O3 -DDEBUG_FLAG=1",
  "link_libs": ["m", "crypto"]
}
```

`cflags` strings are split on whitespace and passed as
individual argv entries to gcc. `link_libs` produces `-l<name>`
entries.

## Bash

```bash
# src/greet.sh
greet() {
    local name="$1"
    echo "Hello, $name!"
}
```

Wired up:

```json
{
  "id":   "greet",
  "kind": "call",
  "lang": "bash",
  "ref":  "src/greet.sh",
  "fn":   "greet",
  "inputs":  [ { "name": "name", "type": "string" } ],
  "routing": { "kind": "plain" }
}
```

The Bash spec runs a persistent helper bash process per worker
and dispatches invocations over a socketpair using a small line
protocol. The function reads its inputs as positional args and
writes its output to stdout. Function source is loaded into the
helper's environment once per worker; subsequent invocations
reuse the same helper.

## Choosing a language

- **Lua** — easiest, fastest iteration. Source changes pick up
  on the next worker init. Best for planner / glue logic and
  anything where the box body is short.
- **C** — fastest steady-state execution. Pays a compile on the
  first dispatch (cached after); the compile pipeline can
  precompile every C box so the runtime doesn't pay that cost.
  Best for tight inner loops and anything you'd write in C
  anyway.
- **Bash** — for boxes that wrap external CLI tools. The
  per-call overhead is real (process IPC), so don't use it for
  hot inner work.

## Cross-language wires

A wire between two boxes of the same language carries values in
that language's native representation. A wire between two boxes
of different languages crosses via JSON: the producer's spec
serialises to JSON on the way out, the consumer's spec
deserialises on the way in. The dispatch handles the bookkeeping;
your box function never sees the boundary.

The slot store carries values dual-ring per wire — native and
JSON cells side-by-side, with a per-cell tag picking which the
consumer reads. This is what makes a Lua function value sent on
a Lua → Lua wire survive as a callable function on the
consumer's side, while the same value sent on a Lua → C wire
arrives as a JSON sentinel the C side can pass back to Lua
unchanged.

## Adding a language

If you need a fourth language (Python, JavaScript, …), the
contract is the `lang_spec_t` struct in `langs/lang-spec.h`:

```c
typedef struct {
    const char *name;
    const char *file_ext;
    int  (*init)(int worker_idx);
    int  (*invoke)(void *handle, ...);
    void (*teardown)(void *handle);
    int  (*compile)(const char *src, const char *out, const box_t *box);
    int  (*native_to_json)(const void *handle, const void *bytes, int size,
                           char *out_buf, int out_cap, int *out_size);
    int  (*json_to_native)(const void *handle, const void *bytes, int size,
                           char *out_buf, int out_cap, int *out_size);
    /* ... see the header for the full surface */
} lang_spec_t;
```

Ship a `langs/<name>/spec.so` that exports a `lang_spec_t` plus
the registration entrypoint, and the spec registry picks it up
on startup. The Lua / C / Bash specs are working references —
`langs/lua/spec.c` is the most heavily-commented and the easiest
starting point.
