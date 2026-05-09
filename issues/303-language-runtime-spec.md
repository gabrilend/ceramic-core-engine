# 303 — Language runtime spec: pluggable per-language invocation

## Status
open

## Current behavior
Each language has an ad-hoc driver script (`drivers/lua.sh`, `drivers/bash.sh`,
`drivers/c.sh`). The driver is invoked per box call: spawn a process, run
the function, read stdout, exit. Adding a new language means writing a new
driver script and modifying the executor to know about it.

There is no shared interface, no persistent runtime, and no way for a user
to add a language without touching SoraMech internals.

## Concept

The language runtime spec is the user-extensible interface that defines how
a language's functions are invoked from inside the thread pool. Every
language goes through this interface — there is no special path for any
language. Lua is a spec. C is a spec. Bash is a spec. A user adding Python
writes another spec.

The pool runner does not know how to "run a Lua function." It knows how to
call `lang->invoke(...)`. The Lua spec is the thing that knows how to run a
Lua function.

The user provides the bridging code. SoraMech does not transpile, rewrite,
or convert box source between languages. If the user wants Rust, the user
writes a Rust spec.

## The interface

Each language spec is a C library that exports a single symbol:

```c
typedef struct {
    const char *name;       // "lua", "bash", "c", "python", ...
    const char *file_ext;   // ".lua", ".sh", ".c", ".py" — for box file resolution

    // Once-per-worker. Returns an opaque handle stored in worker_ctx.
    // Called at pool startup, before any tasks run.
    void *(*init)(int worker_idx);

    // Once-per-worker. Called at pool shutdown.
    void  (*teardown)(void *handle);

    // Optional: called once per box source file at map load.
    // For languages that compile (C → .so), this is where compilation
    // happens. For languages that don't (Lua, Bash), this is NULL.
    int   (*compile)(const char *src_path, const char *out_path);

    // The invocation. Inputs are arrays of (bytes, size) pairs,
    // already read from slots by the dispatch layer. The spec writes
    // its return value into out_buf and sets *out_size.
    //
    // Returns 0 on success, nonzero on error.
    int   (*invoke)(void *handle,
                    const char *file_path,
                    const char *fn_name,
                    const void **input_data, const int *input_sizes, int n_inputs,
                    void *out_buf, int out_buf_capacity, int *out_size);
} lang_spec_t;

extern lang_spec_t soramech_lang_spec;
```

The spec library is built as a `.so` and loaded by the pool runner at
startup via `dlopen` / `dlsym`. The runner iterates `langs/*/spec.so`,
loads each, and registers it under its `name`.

## Slot access boundary

Specs do not see slot IDs. The dispatch layer (issue 304) reads input
values from slots into raw byte buffers, hands them to `invoke`, receives
the output bytes, and writes them to the output slot. This decouples
specs from slot internals — a spec only deals with values.

If a future language wants zero-copy access (e.g. memory-mapping a slot
directly into a Python bytearray), the interface can be extended. Do not
add it preemptively.

## Per-language layout

Specs live under `langs/<name>/`:

```
langs/lua/
    spec.c              ← implements lang_spec_t (handles → lua_State)
    Makefile            ← builds spec.so
    spec.so             ← built artifact

langs/bash/
    spec.c              ← bridges to bash-server.sh via Unix domain socket
    bash-server.sh      ← persistent subprocess started by spec.init
    Makefile

langs/c/
    spec.c              ← uses dlopen/dlsym; compile = gcc -shared -fPIC
    Makefile

langs/python/           ← user-added; not shipped
    spec.c
    Makefile
```

The pool runner has no compiled-in knowledge of any specific language.
Even Lua is loaded as a `.so` at startup. This keeps the boundary honest
— there is no temptation to special-case the "shipped" languages.

## Built-in specs (shipped, not privileged)

SoraMech ships specs for:
- **Lua** — handle is a `lua_State`. `invoke` calls `luaL_loadfile` (cached)
  + `lua_pcall`. Args pushed as Lua strings; return read as Lua string.
- **C** — handle is a small `dlopen` handle cache. `compile` runs
  `gcc -shared -fPIC -o <out_path> <src_path>`. `invoke` does
  `dlsym` + direct call.
- **Bash** — handle is a Unix domain socket connected to a persistent
  bash subprocess (`bash-server.sh`). `invoke` sends a framed request,
  reads a framed response.

These are the reference implementations. They demonstrate the spec
interface for in-process FFI (Lua, C) and for out-of-process socket
servers (Bash, and any future language whose runtime cannot be embedded).

## Compilation

The optional `compile` hook runs at map load (or at editor compile-button
time, issue 219). For C, it produces a `.so` next to the source. For
Lua and Bash, no compilation step is needed; `compile` is `NULL`.

Compilation outputs go in the map's `compiled/bin/` directory (issue
222). The dispatch layer passes the compiled artifact path to `invoke`,
not the original source path, when a compiled artifact exists.

## Iterator counter prepending

The iterator counter (issue 221) is prepended to the input array by the
dispatch layer, not by the spec. From the spec's perspective, an
iterator box just has one extra input. The spec doesn't know which
input is the counter — that's a graph-level concept.

## Errors crash the program

A nonzero return from `invoke` is a fatal error. The dispatch layer
prints the error context (box id, function name, language) to stderr
and aborts the process. There is no recovery, no retry, no per-task
"failed" state, no fallback. There are no timeouts on stalled tasks,
no wait-list garbage collection, no failure propagation through the
graph.

The reasoning: a task that cannot produce a correct return value is a
bug. Bugs do not belong in production. Building in detection and
recovery for bug states adds runtime cost and complexity to handle a
case that should not exist. If a bug surfaces in production, the
program crashes loudly; the developer fixes the bug and redeploys.

The spec is still responsible for catching language-level errors (Lua
`error()`, C signals it can intercept, Bash nonzero exit) and
translating them into a nonzero return — so that the dispatch layer
sees a clean signal instead of an undefined post-error state. Once
returned, the program ends.

## User-facing documentation

The interface is published in `langs/lang-spec.h`, which every spec
implementation includes. That header is the source of truth for the
contract.

User-facing docs that mention the language spec system:
- `README.md` — short paragraph in the features list pointing out that
  any language can be added by writing a spec, with a one-line example
  of what that means
- `docs/005-language-specs.md` (to be written) — the long-form guide
  on writing a spec: walk through the Lua spec as a small reference,
  show what `init` / `invoke` / `teardown` do, point at the C spec for
  the `compile`-using case and the Bash spec for the
  socket-server case
- `docs/000-table-of-contents.md` — index entry for the new doc

Without these the spec system is invisible — users would never know it
exists or how to extend it.

## Open questions

(none currently)

## Suggested implementation sequence

1. Write `langs/lang-spec.h` — the public interface header.
2. Write `langs/lua/spec.c` first. Lua is the easiest because it's already
   our default language and the existing driver gives a reference behavior.
3. Write `langs/c/spec.c` — adds `compile` callback, exercises the dlopen
   path.
4. Write `langs/bash/spec.c` + `langs/bash/bash-server.sh` — exercises
   the out-of-process socket model. This is the proof that languages
   without an embeddable runtime fit the same interface.
5. Pool runner registry: `dlopen` every `langs/*/spec.so` at startup,
   register by `name`. Box files are routed to specs by `file_ext`.
6. Smoke test: a map with one Lua box, one C box, one Bash box, all
   running in the pool, all going through the spec interface. No
   special-cased code paths.

## Relevant files

- `drivers/lua.sh`, `drivers/bash.sh`, `drivers/c.sh` — current ad-hoc
  drivers, replaced by `langs/<name>/spec.c`
- `issues/301-pool-lifecycle-and-worker-init.md` — pool startup invokes
  spec `init` per worker per language
- `issues/302-wire-value-slot-store.md` — dispatch layer reads slots and
  passes raw bytes to specs (specs do not touch slots)
- `issues/304-task-dispatch-layer.md` — calls `invoke` per task
- `issues/222-compile-button-and-assets-directory.md` — compile button
  runs spec `compile` for every box
- `docs/004-ipc-and-threading.md` — Option 1 (FFI) and Option 3 (Unix
  socket) describe the two execution models specs implement
