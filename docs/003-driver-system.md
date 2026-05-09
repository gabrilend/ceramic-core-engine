# SoraMech — Language Driver System

## Two systems, one filename

Phase 2 used **driver scripts** — shell wrappers under `drivers/`
that the synchronous runner invoked one process per box call.

Phase 3 uses **language specs** — `.so` libraries under `langs/`
loaded by the C pool runner via `dlopen`, with persistent per-worker
runtimes.

Same idea, different mechanism. Phase 3 replaces phase 2 wholesale
when it lands. This document covers both, since the project lives in
phase 2 today and the phase 3 design is fully specified.

## Phase 2 — Driver scripts (current)

A driver is a shell script. It knows how to invoke a function in one
language. The synchronous runner looks up the file extension, finds
the driver script, and calls it.

### Driver contract

The runner invokes a driver as:

```
<driver-script> <file-path> <fn-name> <arg-count> [<arg> ...]
```

Arguments are always strings. Structured values (tables, arrays) are
passed as JSON strings and decoded by the driver.

The driver must:
- Exit 0 on success, nonzero on failure
- Print one JSON value to stdout — string, number, boolean, object,
  or null. One output wire per box.
- Print error messages to stderr on failure

Multi-output tuple returns are not supported. Each box has exactly
one output wire (issue 218).

### Built-in driver scripts

Shipped in the project's `drivers/` directory:

- `drivers/lua.sh` — invokes a Lua function via `luajit`
- `drivers/c.sh` — compiles `.c` (mtime cache) and runs the binary
- `drivers/bash.sh` — sources the `.sh` and calls the function

User-added drivers go in the same directory and are picked up by
extension.

### What's wrong with this model

Process spawn cost dominates each call (1–10 ms per language).
Stdout is a fragile channel — debug prints inside box code corrupt
the wire value. Each invocation is its own fresh interpreter; there
is no persistent runtime between calls. These are the limitations
phase 3 fixes.

## Phase 3 — Language specs (planned)

Each language is a small C library implementing the spec interface
defined in issue 303. The pool runner loads every
`langs/<name>/spec.so` at startup; the spec interface is uniform
across languages, with no privileged language.

### Spec interface

```c
typedef struct {
    const char *name;
    const char *file_ext;

    void *(*init)(int worker_idx);
    void  (*teardown)(void *handle);
    int   (*compile)(const char *src_path, const char *out_path,
                     const box_t *box);
    int   (*invoke)(void *handle,
                    const char *file_path, const char *fn_name,
                    const void **input_data, const int *input_sizes,
                    int n_inputs,
                    void *out_buf, int out_buf_capacity,
                    int *out_size);
} lang_spec_t;
```

`init` runs once per worker thread at pool startup. `teardown` runs
at shutdown. `compile` runs at map load time for languages that need
it (C). `invoke` runs once per box invocation.

The spec receives input bytes and writes output bytes — never
interacts with slot IDs. The dispatch layer (issue 304) reads
inputs from slots into byte buffers, hands them to `invoke`, takes
back bytes, writes them to the output slot.

### Reference specs

- **Lua** (issue 306) — handle is a `lua_State` per worker. `invoke`
  loads the box file (cached in the registry), pushes typed
  arguments, calls via `lua_pcall`, serializes the return value.
- **C** (issue 307) — handle is a `dlopen`-cache per worker.
  `compile` generates a wrapper from the box's declared inputs and
  return type, then runs `cc` to produce the `.so`. `invoke` does
  `dlsym` and a direct function-pointer call.
- **Bash** (issue 308) — handle is a Unix domain socket connected
  to a persistent `bash-server.sh` subprocess. `invoke` sends a
  framed request, reads a framed response.

The Lua and C specs are in-process (Option 1 in
`docs/004-ipc-and-threading.md`). The Bash spec is out-of-process
(Option 3) — proof that the spec interface accommodates languages
that cannot be embedded.

### Adding a new language

A user adds Python, Rust, or anything else by writing a new spec:

1. Create `langs/<name>/spec.c` implementing the four callbacks.
2. Create `langs/<name>/Makefile` that builds `spec.so`.
3. Add the language to `meta.json src_dirs` if necessary so the
   editor's file browser can see source files.

The pool runner picks up the new spec at the next startup. No
SoraMech code change required.

For full implementation guidance, see the future
`docs/005-language-specs.md` (referenced from issue 303). For the
spec contract itself, issue 303.

## Migration: phase 2 → phase 3

When phase 3 lands:

| Phase 2                              | Phase 3                              |
|--------------------------------------|--------------------------------------|
| `drivers/lua.sh`                     | `langs/lua/spec.so`                  |
| `drivers/c.sh`                       | `langs/c/spec.so`                    |
| `drivers/bash.sh`                    | `langs/bash/spec.so`                 |
| `drivers.json` (per-map)             | (none — spec registry is global)     |
| stdout as channel                    | direct function call / Unix socket   |
| One process per call                 | Persistent runtimes                  |
| Single output wire (JSON-encoded)    | Single output slot (typed bytes)     |

Box JSON does not change shape — `lang`, `ref`, `fn`, `inputs`
remain. Phase 3 reads the same files phase 2 reads. The runtime
mechanism beneath the box is what changes.

## Relevant issues

- 102 (completed) — phase 2 driver interface
- 218 (completed) — single-output driver contract
- 303 — phase 3 language spec contract
- 306 — Lua language spec
- 307 — C language spec
- 308 — Bash language spec
