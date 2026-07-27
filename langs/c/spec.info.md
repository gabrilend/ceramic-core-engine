# langs/c/spec.c — public surface

C language spec. Built as `spec.so`; `dlopen`'d by the pool runner
at startup. Fully implemented (issue 307).

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name` = `"c"`, `file_ext` = `".c"`
  - `init` / `teardown` — per-worker handle cache lifecycle
  - `compile` — the only shipped spec with a real build step
  - `invoke` — call the box's function through its `.so`
  - `native_to_json` / `json_to_native` / `translate` — wire bridges
  - `translate_targets` = `{"lua", "bash"}`
  - sentinel emit / reconstruct masks — `$ref` only
  - no `invoke_wrote_native`, which the dispatch reads as "I write
    what I'm asked" — true for C, whose values are already bytes

## Compile and cache

A C box is source on disk, so this spec compiles it. `compile`
builds the box's `.c` into a shared object, lazily: it compares
mtimes and rebuilds only when the source is newer than the cached
`.so`. Each worker keeps its own `dlopen` handle cache, so the
first fire on a worker pays the open and later fires are a
function-pointer call.

The compiled artifact is what `scripts/soramech-compile.sh` packs
into a portable map bundle. The runner still lazy-compiles from
`src/` if a prebuilt `.so` is missing, so a bundle without them
degrades to slower-first-fire rather than failing.

## The box function signature

```c
int box_fn(const void **inputs, const int *sizes, int n_inputs,
           void *out_buf, int out_capacity, int *out_size);
```

Inputs arrive in the order the box JSON declares its ports. The
function writes its output into the caller's buffer and sets
`*out_size`; returning nonzero fails the fire. Nothing is
allocated on the box's behalf and nothing is freed for it — the
buffer belongs to the dispatch.

## Thread safety is the author's problem

C boxes share the whole address space. Every worker runs the same
compiled function, and every box is multi-spawn, so a `static` or
global variable is genuinely shared and genuinely races. Use
`stdatomic.h` for counters and keep everything else on the stack.
See `docs/005-writing-boxes.md`.

## Values across wires

C's outputs are already bytes, so a same-language wire carries
them untouched. Crossing to Lua or Bash goes through JSON, with
strings wrapped and primitives passed through.

## Build

- `make` here, or `make specs` from the project root. Links `-ldl`.

## Related

- Issue 307 — the implementation.
- Issue 325 — `translate_targets`.
- `langs/lang-spec.h` — the contract.
- `docs/005-writing-boxes.md` — the C box author's quickstart.
