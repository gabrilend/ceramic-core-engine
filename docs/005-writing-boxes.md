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

## Your function must be thread-safe

Every box is multi-spawn. The runtime never gates re-entry: your
box function can be running on three workers at the same moment,
and a fire can begin while a previous fire of the same box is
still in progress. There is no flag to opt out of this and no
routing kind that is exempt.

What the runtime does guarantee is exactly two things. Each fire
receives its own input values, popped from the slots before the
task was queued — no two fires share an input buffer. And each
fire writes to its own unique return slot — no two fires race on
the output. Everything between those two points is yours.

So: no unsynchronised mutable state that outlives a call.
Atomics or a lock for shared counters. Pure functions, sharing
nothing, are always safe and are the default worth reaching for.

The hazard is shaped differently per language, because the three
specs hold their state differently:

| Language | What's shared across concurrent fires | The hazard |
|---|---|---|
| **Lua** | A `lua_State` **per worker**, not per box. Two fires on different workers touch different states; two fires on one worker share one. | Not a data race — a *split brain*. A module-level counter counts once per worker, so it reads low and non-deterministically. Module imports and closures persist per worker, which is the intended benefit; mutable module state is the trap. |
| **C** | The whole address space. `static` and global variables are genuinely shared by every worker. | A real data race with real torn reads. Use `stdatomic.h` for counters; keep everything else on the stack. |
| **Bash** | Nothing in memory — each invoke is its own process. | The filesystem. Two fires appending to one path interleave. Write to distinct paths, or use an atomic rename. |

If a box genuinely cannot be made re-entrant — it drives a device
that admits one caller, say — serialise it *inside* the box with
its own lock. The runtime will not do it for you.

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
imports and closures persist across calls within a run. Per
*worker*, though, not per box: fires of one box that land on
different workers see different states, so module-level mutable
state does not add up the way it looks like it should. See the
thread-safety contract above.

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

Every cross-language pairing is **declared, not assumed** (issue
325): a spec lists the languages it can serialize values for in
its `translate_targets`, and the graph loader refuses to load a
map whose wire crosses an undeclared pair — the error names the
pair and the fix. All three shipped specs declare each other,
with JSON as the declared translation for every pair. The full
nine-pair contract is pinned by the `tests/maps/325-pair-matrix`
fixture, one consumer box per pair, named for its pair.

The dispatch routes each pushed value by the form the spec
**actually wrote**, not the form it asked for. A spec can report
its actual output form after each invoke (the optional
`invoke_wrote_native` accessor) — which is how a Lua table asked
for in native form still crosses correctly: the spec writes
JSON, reports it, and the value lands on the JSON side of the
dual ring, where the per-cell tag tells the consumer to parse.

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
    const char *const *translate_targets;      /* declared pairs (issue 325) */
    int  (*invoke_wrote_native)(void *handle); /* actual-form report (325)   */
    /* ... see the header for the full surface */
} lang_spec_t;
```

Ship a `langs/<name>/spec.so` that exports a `lang_spec_t` plus
the registration entrypoint, and the spec registry picks it up
on startup. The Lua / C / Bash specs are working references —
`langs/lua/spec.c` is the most heavily-commented and the easiest
starting point.

Two contract duties beyond the callbacks:

- **Declare your translation story.** `translate_targets` is the
  NULL-terminated list of languages your spec can serialize
  values for. Until a pair is declared, any map wiring your
  language across that pair refuses to load — loudly, naming the
  pair. Declaring a target means your serialized output is
  something that consumer's spec can decode; the shipped specs
  all declare each other, with JSON as each pair's translation.
- **Report what you actually wrote** — only if your invoke can
  ever produce a different output form than it was asked for.
  The optional `invoke_wrote_native` accessor is read by the
  dispatch right after each invoke, on the same worker thread.
  Omit it when your spec always writes the requested form (true
  for the shipped C and Bash specs; the Lua spec provides it
  because tables ride native asks as JSON).
