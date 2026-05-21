# SoraMech — Architecture

## The structural shell

SoraMech is a structural shell. The thread pool, the slot store, the
dispatch layer, the allocator — these are the steel and concrete of
an apartment building. They hold the roof up. They do not decorate
the rooms.

Box authors design the rooms. Library authors furnish them. The
shell provides the framing, the load-bearing walls, the structural
guarantees of what can hang where. The shell does not know what
colour the curtains are.

> "We are creating a structural shell, like an apartment building
> that is just steel and concrete at first, and the user can build
> the walls and windows and design the doors and movement paths as
> they please. The library authors are doing the actual decorating,
> with tables and chairs and pictures of the beach. We are
> structural, we keep the roof from caving in. To that end, we must
> be rigid. We must be firm. We must be flexible." — 2026-05-20

Two consequences shape every design decision below.

### One allocator function per kind of operation

For each distinct kind of allocation the runtime needs, exactly one
function performs it. The same function is called from every box's
hot path; only its parameters vary. The user's code, the spec's
code, the dispatch layer's code — all reach the same entry point.

This is the rigidity. The shell does not present "convenient
shortcuts" for special cases. Every value lifecycle goes through
the same allocator. Every task struct comes from the same slab.
Every wire push goes through the same `slot_push`. No alternatives,
no escape hatches.

The exceptions are setup and teardown: the graph loader allocates
every box's slot table once at startup before the pool launches;
`pool_destroy` releases everything in one shot at the end. Outside
those two moments, the shell is uniform.

### No single-threaded process outside the pool

Every piece of runtime work — every allocation, every reference
count update, every dispatch action, every cleanup — happens
inside a pool task. There is no background garbage collector, no
maintenance daemon, no separate scheduler. The pool is the
universal scheduler; the pool task is the universal unit of work.

Consequences:

- The allocator's deep sweep, when it runs, runs inside the
  failing-to-allocate task. There is no "sweep thread" polling for
  free memory. There is no quiescence-triggered sweep either — we
  cannot assume there will ever be a quiescent moment in a graph
  with self-feeding iterators.
- Reference count decrements happen at the end of the task that
  was holding the reference. Cleanup is the task's last act, not
  a separate worker's job.
- Box-level retirement (when all of a box's possible inputs are
  exhausted) is checked inside the task whose completion may have
  triggered the retirement condition.

The pool's parallelism is the only parallelism the runtime
provides. A side thread would be a different shape — a violation
of the uniform "one allocator, one scheduler" rule.

## Three programs, one data format

SoraMech is three independent programs that share a common on-disk
format: the map directory. None of the three knows about the others
at runtime.

```
  Browser (assets/index.html)
       |  HTTP (file CRUD)
       v
  src/006-server-main.lua       [map directory]
                                maps/<name>/
                                  meta.json
                                  boxes/
                                  data/
                                  src/
                                  compiled/   (after compile button)
                                  tmp/ -> /tmp/<name>/
       ^
       | reads & writes
  Runner (one of):
    src/007-runner-main.lua     ← phase 2, synchronous Lua interpreter
    soramech-pool (binary)      ← phase 3, C thread pool runner
```

### src/006-server-main.lua (the editor's HTTP backend)

Thin file CRUD proxy. Receives JSON requests from the browser and
translates them into reads and writes on the map directory. No logic
beyond path validation and basic consistency checks (e.g. reject a
delete if the box is still referenced by another box's connection
list).

Started with: `luajit src/006-server-main.lua [port]` (default 7700).
The `run` script at the project root starts the server and opens the
editor.

The editor is editor-only — no run button, no run results display.
The runner is invoked separately, by command line. `last-run.jsonl`
is the runner's output, consumed by external tooling, not by the
editor.

### Phase 2 runner — `src/007-runner-main.lua`

Synchronous Lua interpreter. Loads a map directory, validates the
graph, walks boxes in dependency order, invokes language drivers per
box (`drivers/lua.sh`, `drivers/c.sh`, `drivers/bash.sh`). Each box
invocation is wrapped in a `task_fn(inputs) -> outputs` boundary that
matches the phase 3 thread pool's action signature.

This is the development target while the editor is being built out.
Phase 2 retires wholesale when phase 3 lands; the editor and the
map-directory format are unchanged across the cutover.

### Phase 3 runner — `soramech-pool` (`src/008-pool-runner.c`)

The phase 3 runner is a single C binary. It owns:

- A **SoraMech-built thread pool** under `libs/task-pool/` (3d-rts at
  `/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h`
  is the design reference, not vendored).
- A **C graph loader** (`src/graph-loader.c`) — replaces
  `src/003-loader.lua`. The runner has no embedded Lua state at the
  top level; Lua only appears inside a Lua language spec's per-worker
  handle.
- A **per-input-port slot store** (`src/slot-store.c`) — ring-buffer
  cells in process heap, durable for the run, plus a large-value heap
  for variable-size payloads.
- A **dispatch layer** (`src/dispatch.c`) — the single worker-side
  action: read inputs → invoke spec → pick branch → push outputs.
- A **language spec registry** (`src/spec-registry.c`) — `dlopen`s
  every `langs/<name>/spec.so` at startup and registers it by name
  and file extension.
- A **run-output writer** — a dedicated thread that drains a
  multi-producer-single-consumer ring of events and appends one JSON
  line per event to `tmp/last-run.jsonl`.

One pool per map run. Created after the graph loads and validates;
destroyed after the active-task counter reaches zero. Worker count
defaults to logical CPUs (capped at `MAX_WORKERS=16`), overridable
via `SORAMECH_WORKERS=N`.

See "Execution model — phase 3" below for the action loop and slot
mechanics.

### assets/index.html (the editor)

Static HTML + vanilla JS. Infinite-scroll canvas. Box diagram editor.
Talks to `src/006-server-main.lua` for all file operations. No run
button. The Compile button (added in phase 2 as UI only) wires into
the phase 3 build/package pipeline once it lands.

## Map directory format

```
maps/<name>/
  meta.json          — { name, description, src_dirs?, lang? }
  boxes/
    <id>.json        — one file per box (see Box file format below)
  data/
    <name>.json      — soramech-data tables; section-level "constant" flag
  src/
    *.lua, *.c, *.sh — box function source files
  compiled/          — produced by the compile pipeline (issue 309)
    pool-runner      — copy or symlink of soramech-pool
    src/             — every source file the map uses
    bin/             — compiled .so files (C boxes via the C spec's compile callback)
    langs/<name>/    — copy of each language spec .so the map uses
    manifest.json    — every box, its language, its artifact path, build version
  tmp/               — symlink to /tmp/<name>/
    last-run.jsonl   — phase 3 run log (JSON Lines)
    logs/            — additional run logs
```

A compiled map runs standalone: `./compiled/pool-runner` looks up its
specs in `compiled/langs/`, reads `manifest.json`, and executes
without any reference to the live editor source tree.

Phase 2's `drivers.json` file is gone. Phase 2 resolves drivers by
file extension via the `drivers/` directory; phase 3 resolves
languages by file extension via the spec registry (each spec declares
its `file_ext`). No map-local driver config file is needed in either
phase.

The `data/` directory is for `soramech-data` storage (see "Data
files" below). It is **not** where file-source boxes read from — those
read arbitrary paths, see Box kinds.

## Box file format

Every box is a single JSON file under `boxes/<id>.json`. Every box
has exactly **one output wire** (issue 218). There is no `outputs`
array. Whatever the function returns travels down the single output
wire as one value; multi-value returns are encoded as JSON (or a
language-native struct on a same-language wire — see "Language spec
system" below) and decoded downstream.

### Box kinds

| `kind`  | Runs a function? | Notes                                                                |
|---------|------------------|----------------------------------------------------------------------|
| `call`  | yes              | Plain call, comparator, iterator, etc. — see `routing` below.        |
| `read`  | no               | Value source. Inline literal `value` OR file at `path`.              |
| `write` | no               | File sink. Writes the `value` input; emits boolean `"true"` downstream. |

`read` and `write` are dispatch-layer primitives — the runtime
reads/writes the file directly, no language spec is invoked. A
`read` box with an inline `value` literal skips file IO entirely
and emits the literal bytes. A `write` box atomically commits
its `value` input to disk and pushes `"true"` to any downstream
wires; if no wire reads its output, the boolean is discarded per
the unwired-output rule.

```json
{ "id": "from_file", "kind": "read", "path": "config.json",
  "ui": { "x": 100, "y": 100 } }

{ "id": "from_literal", "kind": "read",
  "value": "hello, world!",
  "ui": { "x": 100, "y": 200 } }

{ "id": "save", "kind": "write",
  "inputs": [
    { "name": "path",  "type": "string" },
    { "name": "value", "type": "string" }
  ],
  "ui": { "x": 400, "y": 100 } }
```

**Unwired-output discard rule.** If an output value is produced
but no wire carries it downstream, the value is discarded. A
`write` box with no wire on its done-boolean output still
commits the file — the boolean just goes nowhere. The rule
applies to every kind; `write` is the most visible case.

`data` has `path` (not `ref`), no `fn`, no inputs; emits the file's
bytes-as-UTF-8 string on its one output wire. `file_write` takes
`path` and `text` as inputs; the `path` input can be wired (dynamic)
or filled by a literal (static). Binary files are out of scope for
both — non-UTF-8 reads fail with a clear error.

### Call boxes and the `routing` field

A `call` box runs a function (`ref` + `fn`) and produces one output
value. The `routing` field decides where that value goes. Every
call box carries `routing` explicitly — there is no implicit default:

```json
{ "id": "plain-call", "kind": "call",
  "ref": "src/strings.lua", "fn": "trim",
  "inputs": [{ "name": "text", "type": "string" }],
  "routing": { "kind": "plain" },
  "connections": [
    { "from_box": "plain-call", "from_branch": null,
      "to_box": "next", "to_input": "text" }
  ],
  "ui": { "x": 120, "y": 340 } }
```

The function **always runs**, regardless of routing kind. Routing
decides which downstream branch receives the function's output; the
function itself doesn't know.

#### Routing kinds

| `routing.kind`  | Output ports          | Branch picker                                                | Status     |
|-----------------|-----------------------|--------------------------------------------------------------|------------|
| `plain`         | single port           | fan to all outgoing wires                                    | shipped    |
| `comparator`    | `lt` / `eq` / `gt`    | `compare(output, comparand)` → lt/eq/gt                      | shipped    |
| `iterator`      | `out_0` … `out_N-1`   | `slot_read_inc(counter_slot, N)`                             | shipped    |
| `randomizer`    | `out_0` … `out_N-1`   | `hash(slot_read_inc(counter_slot, MAX)) % N`                 | follow-on  |
| `weighted`      | `out_0` … `out_N-1`   | cumulative-band lookup against a counter scaled to PRECISION | follow-on  |
| `distributor`   | `out_0` … `out_N-1`   | argmin over downstream slot fill levels                      | follow-on  |

```json
"routing": { "kind": "plain" }
"routing": { "kind": "comparator", "comparand": "0" }
"routing": { "kind": "iterator",   "n_outputs": 3 }
"routing": { "kind": "weighted",   "weights": [0.8, 0.2] }
"routing": { "kind": "distributor", "n_outputs": 3 }
```

**Branch-level fan-out**: for `comparator` / `iterator` / etc., each
branch port acts like a small plain output of its own. If the
comparator's `lt` port has three wires going to three consumers, then
on every invocation where the routing decision picks `lt`, all three
of those wires fire. The routing decision picks which branch;
fan-out picks which consumers on that branch receive the value.

Plain output port can fan freely too — multiple connections from
`from_branch: null` send copies to every wired consumer. Pulling
multiple wires from a single output is not a routing decision.

### Input ports

```json
"inputs": [
  { "name": "text",        "type": "string" },
  { "name": "config_path", "type": "string", "optional": true,
    "value": "config.json" }
]
```

Every input port is in one of three states at compile time:

1. A wire is connected to it.
2. A literal `value` is set on it.
3. It carries `optional: true`.

**Compile fails** when any port is in none of those states. The
error is precise: which box, which port. Enforced at the Compile
button (phase 2 UI / phase 3 build) and again in the C graph loader
at runtime — belt and suspenders against hand-edited maps.

`optional: true` does **not** mean "pass null" — it means the
argument doesn't have to be passed. The dispatch layer assembles
`input_data[]` / `input_sizes[]` from the slots that actually have
values; an absent optional port shortens `n_inputs` by one and the
language's native short-arg convention handles the rest. Each
language picks up the convention natively (Lua: missing args are
`nil`; Bash: positional unset; C: wrapper reads `n_inputs` and
forwards what's present).

Null as an **explicit** value (typed `null` into the literal field)
is a different thing: it arrives through the function call as a real
argument, mapped to the language's native null (Lua `nil`, Bash empty
string `""`, C `NULL`).

Variadic inputs (`variadic_inputs`) carry base names whose ports
expand to `<base>_0`, `<base>_1`, … in the editor (issue 217).

### Connections

Connections are written to **both** endpoint box files. The loader
validates at load time that both ends agree — disagreement is a
hard error.

```json
{ "from_box": "...",
  "from_branch": null | "lt" | "eq" | "gt" | "out_0" | ...,
  "to_box":   "...",
  "to_input": "..." }
```

`from_branch` is `null` for `routing: plain` and for boxes without a
routing field; otherwise it names the branch port the wire leaves
from.

## Language spec system (phase 3)

Each language is a **spec** — a `.so` library at
`langs/<name>/spec.so` that exports a `lang_spec_t` symbol. The pool
runner loads every `langs/*/spec.so` at startup via `dlopen` and
registers it by `name` (for box lookup) and `file_ext` (for source
resolution). The spec contract is uniform across languages; no
language is privileged. The runner itself does not link to liblua,
libc++, or any language-specific runtime — only specs do.

### `lang_spec_t`

```c
typedef struct {
    const char *name;       // "lua", "bash", "c", "python", ...
    const char *file_ext;   // ".lua", ".sh", ".c", ".py"

    // Once-per-worker. Pool startup calls init() for every spec on
    // every worker before unblocking the init barrier; teardown()
    // runs at shutdown.
    void *(*init)    (int worker_idx);
    void  (*teardown)(void *handle);

    // Optional: at map-compile time. C uses this to gcc -shared -fPIC.
    // Lua and Bash leave this NULL.
    int   (*compile) (const char *src_path, const char *out_path);

    // Universal invoke path: JSON bytes in, JSON bytes out.
    // Used across every cross-language wire.
    int   (*invoke_json)  (void *handle,
                           const char *file_path, const char *fn_name,
                           const void **input_data, const int *input_sizes, int n_inputs,
                           void *out_buf, int out_buf_capacity, int *out_size);

    // Fast invoke path: language-native bytes in, language-native bytes out.
    // Used when producer and consumer share a language (see "Same-language
    // wire fast path" below).
    int   (*invoke_native)(void *handle, /* same signature */);

    // Bridges used when a value crosses a language boundary.
    int   (*native_to_json)(const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size);
    int   (*json_to_native)(const void *src, int src_size,
                            void *dst, int dst_capacity, int *dst_size);
} lang_spec_t;

extern lang_spec_t soramech_lang_spec;
```

A nonzero return from `invoke_*` is a fatal error. The dispatch
layer prints the context (box id, function name, language) and
aborts. There is no recovery, no retry, no per-task "failed" state.
Bugs do not belong in production.

### Init barrier

Workers are not safe to dispatch to until every worker has completed
`init` for every spec the map uses. Pool startup runs each worker's
`init` calls, increments a shared `workers_ready` atomic, and blocks
on a condition variable. The main thread waits for
`workers_ready == n_workers`, then broadcasts — all workers release
together and begin pulling tasks. One-shot; doesn't re-engage.

### Same-language wire fast path

Wire format defaults to JSON. At compile time, every wire is
classified `fast_path: true` (same language at both ends) or `false`
(crosses a language boundary). The compile step also records the
producer's `output_format` per box — `native` if **all** consumers
of its output share its language, `json` if any consumer is
different.

At runtime:

- `fast_path && output_format=native`: producer writes native bytes;
  same-language consumers read native bytes directly.
- `output_format=json`: producer writes JSON bytes; cross-language
  consumers read directly; same-language consumers call
  `json_to_native` on the way in.

Per-language native forms:
- **Lua** — `lua_dump` for closures, `cjson` or msgpack for tables,
  raw bytes for strings/numbers.
- **C** — `memcpy` of the typed struct (the C spec's `compile`
  callback knows the types).
- **Bash** — strings only; the fast path is an alias of the JSON
  path.

A spec that only ever talks to itself can stub out `invoke_json`
(compile error if a cross-language wire ever points at it). A spec
that participates in cross-language wires must implement all four
callbacks.

### Shipped specs

`langs/lua/spec.so`, `langs/c/spec.so`, `langs/bash/spec.so` ship as
reference implementations:

- **Lua** — `init` constructs a `lua_State`; `invoke_*` does
  `luaL_loadfile` (cached) + `lua_pcall`.
- **C** — `compile` runs `gcc -shared -fPIC -o <out> <src>` (caching
  by mtime); `invoke_*` does `dlsym` + direct call through a wrapper
  the compile step generates from the box signature.
- **Bash** — `init` spawns a persistent bash subprocess and connects
  to it over a Unix domain socket. `invoke_*` sends a framed request,
  reads a framed response. The same shape every future
  non-embeddable-runtime language uses (Python optional, Node
  optional, etc.).

A user adds a new language by writing a `langs/<name>/spec.c` and a
small Makefile. SoraMech itself does not transpile box source between
languages.

## Execution model — phase 2 (synchronous)

The runner walks the graph depth-first from the boxes that have all
their inputs satisfied. Each box call:

1. Read wired output values from predecessor boxes.
2. Shell out to the appropriate driver script (`drivers/<lang>.sh`).
3. Decode the single JSON value from driver stdout.
4. Hold the output in runner memory, keyed by box id.
5. Fire connections — enqueue boxes whose inputs are now met.

Single-threaded; one process per box call. The `task_fn` boundary
matches the phase 3 dispatch action signature, so the executor can
be swapped without changing the box execution model.

## Execution model — phase 3 (thread pool)

Tasks are ephemeral; box state is durable. Each box owns one slot
per input port plus optional state (iterator counter, language
handle). A **task** is one stack frame on a worker — `box_id` is
all it carries. Tasks come and go; boxes persist for the run.

### Slots

Every input port has exactly one slot, allocated at graph load and
freed at run end. A wire is a routing declaration; the producer
doesn't own a slot — its output is a push event into every
downstream input slot it's wired to. Fan-out is N pushes; fan-in is
N producers pushing into one slot.

Slot modes:

| Mode                    | Cells   | Reads via         | Used for                                    |
|-------------------------|---------|-------------------|---------------------------------------------|
| 1-cell peek             | 1       | `slot_peek`       | Literals; wires from runs-once producers    |
| N-cell pop              | N       | `slot_pop`        | Wires whose producer runs many times        |
| `SLOT_ATOMIC_COUNTER`   | 1 atom  | `slot_read_inc`   | Iterator / randomizer / weighted counters   |

Compile-time analysis decides per wire: a producer is "runs-once" if
no transitive ancestor is an iterator, else "runs-N-times." The
former gets a 1-cell peek slot (subsequent consumer tasks re-read the
same value); the latter gets an N-cell ring (FIFO drain, one cell
per consumer task).

Variable-size payloads (long LLM outputs, dynamically-sized arrays)
live in a separate **large-value heap**. The slot then holds a small
fixed-size handle `{ uint32_t size; uint32_t offset; }`. The slot
allocator only ever sees fixed-size slots; the two-tier scheme keeps
the hot path simple while supporting unbounded values.

### Cell tagging for parallel iterators

Each cell optionally carries a 4-byte ordering tag alongside its
data. Iterator pushes carry their counter value as the tag; slots
downstream of an iterator are allocated with `SLOT_TAGGED`, and
`slot_pop` on a tagged slot returns the **lowest-tag** cell — not
the head. This preserves iteration order across parallel iterator
workers (task K of iterator A may push before task K-1 if it landed
on a faster worker; the tag makes the consumer see the values in
iteration order regardless).

When a consumer has two tagged inputs from two iterators, each port
pops in tag order independently — pairing falls out as `(A_K, B_K)`
for every K.

### The dispatch action — the attempt loop

Every task in the pool is an **attempt task**. The pool's queue
holds attempt tasks; workers pick them up; each attempt either
advances the graph by one box invocation or returns without doing
anything. There is no separate "readiness check" task kind. An
attempt that finds its inputs ready *is* the work invocation; an
attempt that doesn't, returns and disappears.

```
producer task:
   compute output
   for each downstream consumer box C this producer just touched:
      write all values to C's input slots
         (snapshotted into the attempt's cell at the END)
   for each distinct C:
      slab_alloc() → attempt_task for C
      pool_spawn(attempt_task)
   slab_free(this cell)

attempt task:
   if every required input on C is satisfiable
      (slot value or data-box pull):
      snapshot values into local vars
      invoke spec, get output
      push output to downstream consumer slots
      submit one attempt per distinct downstream consumer
      decrement input $ref counts
      decrement upstream boxes' live_predecessor_count if applicable
   slab_free(this cell)
```

The producer task and the attempt task are the same shape of
allocation, same call site, same return path. One cell per
invocation in the steady-state hot path; no double-allocation
between "check that I'm ready" and "do my work."

Three properties fall out:

- **Targeted attempts.** A producer knows which consumer boxes it
  just pushed to; the attempts it submits are for those specific
  consumers, not a generic re-evaluation of every consumer in the
  graph.
- **One readiness check per producer task.** Even if a producer
  writes to multiple slots on the same consumer, only one attempt
  is submitted for that consumer. The submission happens at the
  END of the producer, after every output value has been placed —
  the attempt is guaranteed to find a coherent input set, not a
  half-written one.
- **No parking.** An attempt that finds its inputs unsatisfied
  returns. The shell does not hold tasks in waiting states. The
  next producer push to any of the consumer's input slots triggers
  another attempt with the same shape. Eventually one of those
  attempts finds the input set complete and runs the work.

> "the most recent change that would enable the box to run —
> essentially, our logic is such that the fewest amount of steps
> between each function call must be made, while enabling the
> long-tail async threads to do non-blocking tasks on their own
> time. As soon as they can, they knit themselves with their
> fellow threads, co-creating a weaving texture of computer
> programming. proving without a doubt that intelligence can have
> a dynamical host." — 2026-05-20

Routing kinds (per 233) dispatch the output-side push: `plain` fans
to every wire, `comparator` picks one of `lt`/`eq`/`gt` by numeric
comparison, `iterator` reads-and-increments a per-box atomic
counter slot mod `n_outputs` and fires only the picked branch. The
follow-on kinds (randomizer / weighted / distributor) live in
issues 240–242.

Cross-language pushes carry a `$ref` handle when the output value
is variable-size (per 312/317): the producer writes the bytes to
the large-value heap once, refcount = number of consuming slots,
and writes the `(ptr, length)` handle into each consumer slot. The
bytes themselves are never copied; the consumer's spec follows the
pointer to read.

### Startup, quiescence, and termination

Startup: the pool runner pushes literal-input values into the
corresponding slots, then spawns first tasks for boxes whose input
sets are now satisfied. Iterator counter slots start at 0.

Quiescence: the main thread waits on a condition variable that
signals when the pool's active-task counter reaches zero with no
pending spawns. That's the run-over signal. The graph validator at
load time rejects non-iterator cycles, so the run cannot deadlock —
an iterator with no upstream pushes simply contributes nothing
further, and once the counter drains, termination fires.

Hard-crash error policy: a nonzero `invoke_*` return aborts the
process. No retry, no per-task failed state, no fallback executor.

### Blocking-aware iterator pattern

Long-running operations (network calls, LLM inference) hold their
worker for the duration. Pool size is the budget. The cooperative
pattern for not holding a worker for minutes: split the call into a
"kickoff" box and a "check-done" iterator that polls. Each pass
through the iterator releases the worker between checks. Documented
fully in `docs/004-ipc-and-threading.md`.

## Run output — `tmp/last-run.jsonl`

Phase 3 writes one JSON object per event, one event per line, to
`tmp/last-run.jsonl`. The format decouples the runner from the
editor — the editor never displays run results; the file is for LLM
consumption, CLI viewers, and the integration test harness.

Event types:

```jsonl
{"event":"run_start", "ts":..., "map":"maps/<name>", "n_workers":16}
{"event":"task_submit", "ts":..., "task_id":42, "box_id":"classify"}
{"event":"task_start",  "ts":..., "task_id":42, "worker_idx":3}
{"event":"task_end",    "ts":..., "task_id":42, "duration_us":127, "output_size":12}
{"event":"slot_alloc",  "ts":..., "slot_id":5, "size":256, "n_cells":1}   // SORAMECH_LOG_SLOTS=1
{"event":"slot_free",   "ts":..., "slot_id":5}                              // SORAMECH_LOG_SLOTS=1
{"event":"run_end",     "ts":..., "duration_us":1850, "n_tasks":7}
```

Mechanism: a dedicated writer thread drains a multi-producer
single-consumer ring buffer of fixed-size event records, serializes
each to JSON, and appends to the file. Workers push records into the
ring without contending on a file mutex. The writer joins before
`pool_destroy`, including a final `run_end` summary.

Inputs/outputs per task are off by default (`SORAMECH_LOG_VALUES=1`
to enable; truncated past 4 KB). Slot allocator events are off by
default (`SORAMECH_LOG_SLOTS=1` to enable). Timestamps are Unix
epoch floats.

The file is overwritten on every run. Rotation is out of scope —
users who want history copy the file.

## Build & compile pipeline

A top-level `Makefile` builds:

1. `soramech-pool` — statically links the pool, JSON parser, slot
   store, graph loader, dispatch layer, spec registry.
2. `langs/<name>/spec.so` — per-spec, via the per-language Makefile
   (Lua links LuaJIT; C links `-ldl`; Bash links nothing extra).

Modes: `make` (release, `-O2 -Wall`), `make DEBUG=1` (`-O0 -g`),
`make STRICT=1` (release + `-Werror -Wextra -Wpedantic`).

Vendored dependencies (the task pool, the JSON parser) live under
`libs/` as plain source — no submodules, no fetches at build time.

Spec discovery at runtime: the runner reads `/proc/self/exe`,
follows the symlink, and scans for `langs/` next to its own binary.
`SORAMECH_LANGS_DIR=/path/to/langs` overrides for development.

### Compile a map for standalone execution

`./compiled/pool-runner` runs a map independently of the editor and
the surrounding source tree. The compile step (Compile button in the
editor, or `soramech-compile <map-dir>` from the CLI) produces:

```
maps/<name>/compiled/
  pool-runner          ← copy or symlink of soramech-pool
  src/                 ← every source file the map uses
  bin/                 ← per-box compiled .so files (C boxes)
  langs/<name>/spec.so ← one per language the map uses
  manifest.json        ← every box, its language, artifact path, build version
```

Steps: walk the graph (reusing the C loader's validation); copy
source files and transitive `require` / `#include` dependencies;
invoke each spec's `compile` callback for boxes whose language needs
it; copy the spec `.so` files used by the map; copy or symlink the
runner binary; write the manifest. Per-box artifacts mtime-compare
so a re-compile recompiles only what changed. Any failure aborts
the step and leaves the partial output for inspection but does not
write the manifest, so the deployment is recognizably broken until
a clean compile succeeds.

## Data files

Persistent data lives in `data/<name>.json` and is accessed via
`libs/soramech-data.lua` (`data.get` / `data.set` / `data.load`).
Format:

```json
{
  "constant": false,
  "fields": {
    "history": [],
    "config": { "constant": true, "value": { "model": "soramind-default" } }
  }
}
```

Top-level `constant: true` makes the whole file read-only;
field-level `constant` flags individual fields within an otherwise
mutable file. This is **separate** from the `data` box kind (which
reads arbitrary files at the dispatch layer) — `soramech-data` is
the per-map structured-table store; `data` boxes are unstructured
file sources.

Ephemeral state (scratch tables, run logs, `last-run.jsonl`) goes to
`tmp/`, a symlink to `/tmp/<map-name>/`. Survives the run but not a
reboot — intentionally.
