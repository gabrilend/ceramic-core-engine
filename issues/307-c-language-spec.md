# 307 — C language spec implementation

## Status
open

## Current behavior
C boxes today run via `drivers/c.sh`, which compiles the source on
demand (mtime-based recompile to a binary), then spawns the binary as
a subprocess and reads stdout. Process spawn cost dominates; each call
is its own freshly-launched executable. The phase 2 driver compiles
to a standalone binary, not a `.so`.

## Concept
The C language spec implements `lang_spec_t` (issue 303) for C. It is
the second reference implementation, and the first one that exercises
the `compile` callback. After compile, invocation is in-process via
`dlopen` / `dlsym` — the user's compiled function is called directly
through a function pointer, with no subprocess and no IPC.

This is the "Option 1 — LuaJIT FFI" model from
`docs/004-ipc-and-threading.md`, applied to C: same-process call,
nanosecond-overhead invocation, native-typed access. The only thing
the C spec adds over the Lua spec is the compile step.

## Per-worker state

`init` returns a small struct holding the dlopen-handle cache:

```c
typedef struct {
    void  *so_handles[MAX_BOXES];   // per-file .so handle, indexed by file id
    void  *fn_ptrs   [MAX_BOXES];   // per-(file, fn) cached function pointer
    int    n_cached;
    // (a hash table could replace these arrays if MAX_BOXES grows large)
} c_handle_t;

void *c_init(int worker_idx);
void  c_teardown(void *handle);    // dlclose every cached handle
```

Each worker thread has its own cache. The same `.so` will end up
loaded once per worker, since `dlopen` with the same path on the same
process returns a refcounted handle (the underlying code segment is
shared across all callers — the per-worker `dlopen` is just a
refcount bump after the first).

## Compile callback

The C spec's `compile` callback receives the box's metadata in
addition to the source and output paths, so it knows the function's
signature and can generate the wrapper:

```c
int c_compile(const char *src_path, const char *out_path, const box_t *box);
```

(This is a small extension to `lang_spec_t` in issue 303 — `compile`
gains a `box_t` pointer. Lua's compile is NULL, so unaffected.)

The callback:
1. Reads `box->inputs`, `box->returns`, `box->headers`, `box->link_libs`,
   `box->cflags` from the box metadata.
2. Generates a wrapper `.c` file (see "Wrapper generation" below) in
   the map's `compiled/bin/` directory next to where the `.so` will
   land.
3. Forks and execs the system `cc` with:
   - `-shared -fPIC -O2 -Wall` (defaults)
   - The user's `cflags`, if any
   - The wrapper `.c` and the user's `src_path` (if `ref` is set)
   - `-o out_path`
   - `-l<lib>` for each entry in `box->link_libs`
4. Waits for the compiler, returns 0 on success, 1 on failure.

If the resulting `.so` is newer than the user's source and the
wrapper, compile is skipped (mtime check, same as phase 2).

The compiler is `cc` — the system default. `gcc` on most Linux,
`clang` on macOS. No SoraMech-specific compiler dependency.

### cflags in box JSON
The `cflags` field on the box JSON is a string of additional compiler
flags (`-O0 -g`, `-mavx2`, etc.). It is set by the user in the
inspector — a "Compile flags" text input shown when a C-language box
is selected. The editor saves it as part of the box's JSON; reads
preserve the field; saves write it back. Nothing overwrites it.

## Box file convention: any C function, no boilerplate

**The rule, stated as a rule:** a box author writes code as if
SoraMech does not exist. The function they write must be a
function their non-SoraMech compiler can compile, their
non-SoraMech editor can lint, their non-SoraMech test harness
can call. No SoraMech-prefixed macros. No SoraMech-shaped
return types. No `#include "soramech.h"`. Nothing in the
author's source betrays that the function will ever cross
SoraMech's wire.

SoraMech bends to the language. The language never bends to
SoraMech. This is non-negotiable across every spec; it's the
reason for the spec interface in the first place.

Users do not write a SoraMech-specific C signature. They reference
any C function — whether one they wrote themselves, or one from a
library (zlib, OpenCV, libcurl, anything they have headers for) —
and the C spec generates a thin wrapper that bridges SoraMech's
bytes-in / bytes-out ABI to the function's natural typed C
signature.

### What the user provides
Either:
- A `.c` file containing the function, referenced by `ref` in the
  box JSON; or
- A library function from a header, no `.c` file of their own — the
  `ref` field is omitted, the function name and headers are
  declared in the box JSON, and the library is named in
  `link_libs`.

### Box JSON for a user-written function

```json
{
  "id": "classify",
  "kind": "call",
  "lang": "c",
  "ref": "classify.c",
  "fn": "classify",
  "inputs": [
    {"name": "text",   "type": "string"},
    {"name": "weight", "type": "int"}
  ],
  "returns": "int"
}
```

The user's `.c` file is naturally written:

```c
// maps/<name>/src/classify.c
int classify(const char *text, int weight) {
    if (text[0] >= '0' && text[0] <= '9') return weight * 1;
    return weight * 2;
}
```

No header to include, no macro, no `int classify(SM_BOX_ARGS)`.
Just a function with a typed signature.

### Box JSON for a library function (no user source)

```json
{
  "id": "compress",
  "kind": "call",
  "lang": "c",
  "fn": "compress2",
  "headers": ["<zlib.h>"],
  "link_libs": ["z"],
  "inputs": [
    {"name": "src",   "type": "bytes"},
    {"name": "level", "type": "int"}
  ],
  "returns": "bytes"
}
```

The wrapper includes `<zlib.h>`, calls `compress2`, and the linker
pulls in `libz` via `-lz`. No user-written `.c` file.

### Wrapper generation

The compile callback emits a wrapper `.c` for each C box. The
wrapper has a fixed entry-point name (`sm_box_entry`) that the spec
always `dlsym`s. It declares the user's function (extern), unpacks
SoraMech's input bytes into typed C arguments per the box's `inputs`
declaration, calls the user's function, and writes the return value
into `sm_out_buf` per the box's `returns` declaration.

For `classify` above, the generated wrapper is roughly:

```c
// generated: classify_wrap.c
extern int classify(const char *, int);

int sm_box_entry(const void **inputs, const int *sizes, int n,
                 void *out_buf, int out_cap, int *out_size) {
    const char *text   = (const char *)inputs[0];
    int         weight = atoi((const char *)inputs[1]);
    int         result = classify(text, weight);
    *out_size = snprintf(out_buf, out_cap, "%d", result);
    return 0;
}
```

The wrapper is mechanical — same template for every box, varying
only in the input-marshalling lines and the return-encoding line.
The compile step writes it to `compiled/bin/<box-id>_wrap.c` and
compiles `<box-id>_wrap.c` plus the user's source (if any) into a
single `.so`.

This means the C spec implements typed function calls without
libffi (which would do the marshalling at runtime via the C ABI
metadata). The same effect is achieved at compile time with
straightforward generated code, no runtime dependency.

## Input marshalling (in the generated wrapper)

The wrapper converts SoraMech's `(bytes, size)` input pairs into the
typed C arguments the user's function expects, based on the
declared input types in the box JSON:

| Declared type | Wrapper action                                                  |
|---------------|-----------------------------------------------------------------|
| `string`      | `(const char *)inputs[i]`                                       |
| `int`         | `atoi((const char *)inputs[i])`                                 |
| `long`        | `strtoll(...)`                                                  |
| `double`      | `strtod(...)`                                                   |
| `bool`        | `strcmp("true", inputs[i]) == 0`                                |
| `bytes`       | `(const void *)inputs[i]`, paired with `sizes[i]` for the size  |
| `json`        | `cJSON_ParseWithLength(inputs[i], sizes[i])` → `cJSON *`        |

The `json` case uses the same vendored cJSON the graph loader uses
(issue 305) — one JSON library shared across the runner. When a box
input is declared `"type": "json"`, the wrapper passes a `cJSON *`
to the user's function. The user navigates the tree with cJSON's
API (`cJSON_GetObjectItem`, `cJSON_GetArrayItem`, etc.) and the
wrapper frees the parsed tree after the function returns.

If a parse fails (a `json` input that is not valid JSON, an `int`
input that does not parse as an integer), the wrapper aborts the
program with the box id and the input position (per issue 303).

## Output encoding (in the generated wrapper)

The wrapper takes the user function's return value and serializes
it into `sm_out_buf`, based on the declared `returns` type:

| Declared `returns` | Wrapper action                                              |
|--------------------|-------------------------------------------------------------|
| `string`           | the user returns `const char *`; `strcpy` + set `out_size` |
| `int`              | `snprintf("%d", ret)`                                       |
| `long`             | `snprintf("%lld", ret)`                                     |
| `double`           | `snprintf("%.17g", ret)`                                    |
| `bool`             | write `"true"` or `"false"`                                 |
| `bytes`            | the user returns `(void *, int)` via an out-parameter pair (see below) |
| `json`             | the user returns `cJSON *`; `cJSON_PrintUnformatted` → bytes |
| `void`             | `*out_size = 0` (zero-byte output)                          |

For `bytes` returns, the user function's signature has the form
`int my_fn(... const char *out_buf, int out_cap, int *out_size)` —
extra parameters that the wrapper passes through from
`sm_out_buf` / `sm_out_capacity` / `sm_out_size`. This handles
library functions like `compress` that write their output via an
out-pointer rather than a return value. The box JSON declares
`"returns": "bytes"` and the wrapper threads the buffer through.

If the encoded value exceeds `sm_out_capacity`, the wrapper aborts
(per issue 303). For boxes whose output capacity is not known at
compile time, see "Variable-size outputs" below — the wrapper
routes through the large-value heap.

## Variable-size outputs

For boxes whose output capacity is not known at compile time (issue
302's large-value heap): the box JSON declares `output_capacity = 0`
and the wrapper allocates from the large-value heap on each call.
For `bytes` and `json` returns, the wrapper transparently picks the
heap path when the box is variable-size — the user's function is
unchanged either way. The handle (pointer + size into the
large-value heap) is what ends up in `sm_out_buf`.

For string returns, similarly: if the returned string exceeds the
declared fixed `output_capacity`, the wrapper falls back to the
large-value heap. This decision is made at compile time based on the
box JSON, not at runtime.

## Thread safety of user code

The same `.so` is loaded by every worker thread that may call into
it. **Static variables and globals declared in the user's C source
are shared mutable state across all workers** unless explicitly
made `const`.

A note on `static` in C: it does not mean immutable. `static` at
file scope means "this symbol has internal linkage, only visible
inside this `.c` file." `static` inside a function means "this
variable persists across calls." Either way, the variable itself is
mutable — `static int counter = 0; counter++;` is perfectly legal
and does what it says, just with internal linkage. Immutable in C
is `const`.

So:
- `static int counter = 0;` at file scope — racy, shared across workers.
- `static int x = 0;` inside a function body — racy, shared across workers calling that function.
- `const int max = 100;` — safe, immutable.
- `static const int max = 100;` — safe, immutable.

Box functions must be reentrant — no mutable shared state without
explicit locking. The C spec does not enforce this; the rule lives
in `docs/005-language-specs.md` (to be written) for box authors to
read.

### Library functions that need global init

Many real C libraries require a one-time setup call before any of
their functions work — `curl_global_init` for libcurl,
`OPENSSL_init_crypto` for OpenSSL, etc. A SoraMech box that calls
into such a library cannot just be wired in cold; the library's
state has to exist before the call.

Two patterns handle this without changing the spec interface:

1. **Init box wired as an entry point.** The user creates a small C
   box whose function is the library's init call (e.g.
   `curl_global_init`). They mark it as an entry box and wire it
   such that any box using libcurl is downstream of it. The init
   runs once at startup before any consumer fires. Process-global
   library state — which is what most C libraries actually use — is
   set up by the time anyone needs it. Pairs with a teardown box
   wired as a terminal node if needed.

2. **Per-worker init via spec extension** (future). For libraries
   that need per-worker setup (each thread gets its own handle),
   the C spec gains an optional per-box `worker_init_fn` field in
   the box JSON. The spec calls that function once per worker
   thread during pool startup, before any box invoke. Not in scope
   yet — pattern (1) covers the common case.

The init-box approach is the recommended starting point. The
spec-extension path is the escape hatch when (1) does not fit.

A future fallback exists: `dlmopen`, a Linux variant of `dlopen`
that loads each library into its own isolated "namespace," giving
each worker its own copy of the library's symbols (statics
included). Two threads using two different namespaces would not
share static state. Less portable than `dlopen` (Linux-only), so
not the default — flagged here as the escape hatch if static-
variable races become a real problem.

## Build

```
langs/c/
    spec.c              ← lang_spec_t implementation
    soramech-c.h        ← user-facing header (shipped to maps via compile step)
    Makefile            ← builds spec.so
    spec.so             ← built artifact (gitignored)
```

Spec Makefile target:
```make
spec.so: spec.c
	$(CC) -shared -fPIC -O2 -Wall -o $@ $< -ldl
```

Linking against `libdl` for `dlopen` / `dlsym` is the only
dependency. No interpreter, no helper library beyond libc.

### What `dlopen` and `dlsym` do
- `dlopen("path/to/file.so", flags)` loads a shared library at
  runtime. The OS loader maps the library's code into the process's
  memory and returns a handle. Same idea as Windows'
  `LoadLibrary`.
- `dlsym(handle, "name")` looks up a symbol — a function or a
  variable — by name inside an already-loaded library, and returns
  a pointer to it. Same idea as Windows' `GetProcAddress`.

Together they are how a program loads and calls code that wasn't
linked at compile time. Plugin systems are the classic use case.
Here, every C box compiles to its own `.so`; the spec `dlopen`s the
`.so` once per worker, `dlsym`s `sm_box_entry` once per file, and
calls the resulting function pointer per invocation.

The user's C box files are compiled by the `compile` callback at map
load time, not by this Makefile. The Makefile here builds only the
spec itself.

## Error handling

- `dlopen` failure (missing `.so`, link errors): print path + `dlerror()`
  to stderr, return 1, dispatch layer aborts.
- `dlsym` failure (missing function): print file/fn + `dlerror()`,
  return 1, abort.
- Compile failure: `cc` exit nonzero, print compiler stderr (already
  routed there by `cc`), return 1, abort.
- User function returns nonzero: propagate that as the spec's return,
  abort.

No retry, no fallback. Per issue 303.

## Open questions

(none currently — earlier questions resolved as follows:)

- Per-box `cflags`: yes, add the field to the box JSON. The
  inspector shows a "Compile flags" text input when a C-language
  box is selected (phase 2 inspector work).
- Typed signatures: handled via compile-time wrapper generation
  (see "Box file convention" above). No libffi runtime dependency.
- Static-variable isolation via `dlmopen`: deferred. Current rule
  is "C box authors write reentrant code"; if that breaks down in
  practice, fall back to per-worker `dlmopen` namespaces (Linux-only).

## Suggested implementation sequence

1. Set up `langs/c/` directory with a Makefile that builds an empty
   `spec.so` exporting a stub `soramech_lang_spec`.
2. Implement `init` and `teardown` with the dlopen-handle cache.
3. Implement `compile` — fork/exec `cc` with the standard flags.
   Smoke test: compile a single hello-world `.c` file.
4. Implement `invoke` — dlsym, direct call, propagate return.
5. Write `soramech-c.h` with the `SM_BOX_ARGS` macro and the basic
   input/output helpers.
6. Smoke test: a C box that takes one string input, writes a string
   output, runs end-to-end through the pool.
7. Add the typed input helpers (`sm_in_int`, `sm_in_double`, etc.).
8. Add the typed output helpers.
9. Test against `maps/driver-test` (C portions).
10. Add variable-size output support (`sm_out_alloc`, `sm_out_handle`).
11. Document in `docs/005-language-specs.md` once it exists, with a
    note on thread safety.

## Relevant files

- `drivers/c.sh` — phase 2 C driver, retired by this spec
- `langs/lang-spec.h` — interface header (issue 303)
- `issues/303-language-runtime-spec.md` — spec contract this implements
- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
  `invoke`
- `issues/302-wire-value-slot-store.md` — large-value heap
- `issues/222-compile-button-and-assets-directory.md` — where compiled
  `.so`s live (`compiled/bin/`)
- `issues/306-lua-language-spec.md` — sister spec; same shape, no
  compile step
- `maps/driver-test` — multi-language test map including C boxes

## Implementation log

### Compile + invoke — 2026-05-12

`langs/c/spec.c`:
- `compile(src, out)` forks gcc with `-shared -fPIC -O2 -Wall`.
- `init` allocates a 32-entry dlopen cache; `teardown` walks it.
- `invoke` looks up the .so in the cache or dlopens it, dlsyms
  the named function, and calls it through a fixed signature:
  `int fn(const void **inputs, const int *sizes, int n,
  void *out_buf, int out_capacity, int *out_size)`.

Convention: C box functions all match that signature. The
optional typed-wrapper mode (per-box signature parsing +
generated marshaller) is deferred until a box wants types that
diverge from the byte-array shape.

`tests/maps/hello/src/echo.c` ships two fixture functions; five
tests in `tests/307-c-spec-test.c` compile and invoke them, plus
the missing-symbol and bad-source failure modes.
