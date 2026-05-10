# 309 — Build system & Makefile orchestration

## Status
open

## Current behavior
Phase 2 has no compiled binary. The runner is a Lua script
(`src/007-runner-main.lua`) invoked via `luajit`. Drivers are shell
scripts that compile per-box C source on demand. There is no
top-level Makefile, no vendoring of C dependencies, no link step
beyond per-driver compile.

Phase 3 introduces a real C binary (the pool runner) plus a set of
language spec `.so` files. They need to compile in the right order
with the right flags, and the build needs to be reproducible from a
clean checkout.

## Concept

A top-level `Makefile` at the project root orchestrates the entire
phase 3 build. It builds:

1. The pool runner binary: `soramech-pool` (statically links the
   SoraMech-owned task pool, the vendored JSON parser, the slot
   store, the graph loader, the dispatch layer, the spec registry).
2. Each language spec: `langs/<name>/spec.so`, built by a small
   per-spec Makefile.
3. (Optional) Unit test binaries.

Each component has its own Makefile if it has non-trivial build
rules. The top-level Makefile delegates to them via `$(MAKE) -C
<dir>`. A clean checkout becomes a working build with `make`.

## Directory layout

```
soramech/
├── Makefile                       ← top-level orchestration
├── soramech-pool                  ← built binary (gitignored)
├── src/
│   ├── 008-pool-runner.c          ← main entry, pool lifecycle (issue 301)
│   ├── slot-store.c / .h          ← per-task slot allocator (issue 302)
│   ├── graph-loader.c / .h        ← C loader, replaces 003-loader.lua (issue 305)
│   ├── dispatch.c / .h            ← dispatch action (issue 304)
│   └── spec-registry.c / .h       ← spec dlopen/registry (issue 303)
├── libs/
│   ├── task-pool/                 ← SoraMech-built thread pool
│   │   ├── pool.h
│   │   └── pool.c
│   └── json/                      ← vendored JSON parser (cJSON, jsmn, etc.)
│       ├── cjson.h
│       └── cjson.c
├── langs/
│   ├── lua/
│   │   ├── Makefile               ← builds spec.so
│   │   ├── spec.c
│   │   └── spec.so                ← built (gitignored)
│   ├── c/
│   │   ├── Makefile
│   │   ├── spec.c
│   │   ├── soramech-c.h           ← user-facing helper header
│   │   └── spec.so
│   └── bash/
│       ├── Makefile
│       ├── spec.c
│       ├── bash-server.sh
│       └── spec.so
└── build/                         ← intermediate object files (gitignored)
```

`build/` holds intermediate `.o` files; binaries land alongside their
sources or at the project root. `.so` and `.o` files are gitignored;
`make clean` removes them.

## Top-level targets

```make
.PHONY: all runner specs clean test

all: runner specs

runner: soramech-pool

soramech-pool: $(POOL_OBJECTS)
	$(CC) -O2 -Wall -pthread -o $@ $^ -ldl

specs:
	$(MAKE) -C langs/lua
	$(MAKE) -C langs/c
	$(MAKE) -C langs/bash

clean:
	rm -f soramech-pool
	rm -rf build/
	$(MAKE) -C langs/lua clean
	$(MAKE) -C langs/c clean
	$(MAKE) -C langs/bash clean

test: all
	./scripts/run-tests.sh
```

`POOL_OBJECTS` resolves to the `.o` files for every `src/*.c`,
`libs/task-pool/*.c`, and `libs/json/*.c`. A pattern rule compiles
each source into `build/`.

## Per-language spec Makefiles

Each `langs/<name>/Makefile` follows the same shape. Lua's:

```make
LUA_INCLUDE ?= /usr/include/luajit-2.1
LUA_LIB    ?= /usr/lib/x86_64-linux-gnu

CFLAGS  = -shared -fPIC -O2 -Wall -I$(LUA_INCLUDE)
LDFLAGS = -L$(LUA_LIB) -lluajit-5.1

spec.so: spec.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f spec.so
```

C's spec links `-ldl`. Bash's links nothing beyond libc. The pattern
is uniform.

LuaJIT's include and lib paths are set via environment variables so
the build works on systems where they live elsewhere (macOS via
homebrew, Alpine, etc.). Defaults match Debian/Ubuntu.

## Build modes

```
make                  ← release: -O2 -Wall
make DEBUG=1          ← debug:   -O0 -g -DDEBUG -Wall -Wextra
make STRICT=1         ← strict:  release + -Werror -Wextra -Wpedantic
```

`DEBUG=1` adds `-g` for symbols and `-DDEBUG` for opt-in debug
prints in the C sources. `STRICT=1` fails the build on any warning;
useful in CI.

## Vendoring

Vendored libraries live under `libs/` and are checked into the repo
as plain source — no submodules, no fetches at build time. To
update a vendored library, copy the new sources in and commit. The
vendored directory carries a small `VENDOR.md` noting the upstream
source and the version that was copied.

Initial vendored libraries:
- `libs/task-pool/` — SoraMech-built thread pool (issue 301)
- `libs/json/` — JSON parser (issue 305 picks cJSON or similar)

Future additions land alongside these.

## Spec discovery at runtime

The pool runner finds language specs by scanning the `langs/`
directory at startup, opening each `langs/<name>/spec.so` via
`dlopen`, and reading the `soramech_lang_spec` symbol via `dlsym`
(per issue 303). The path to `langs/` is resolved relative to the
runner binary's location: the runner reads `/proc/self/exe`
(Linux), follows the symlink, and looks for `langs/` next to it.

This means the binary is location-flexible: the user can drop the
project tree anywhere, and `./soramech-pool` finds its specs
without a config file.

If the user wants to override (custom spec dirs for development),
`SORAMECH_LANGS_DIR=/path/to/langs ./soramech-pool` overrides the
auto-detected location.

## Compile and package a map for standalone execution

Folded in from issue 219 (closed). The build system also handles the
"package a map for deployment" step that the editor's Compile button
(issue 222) and a CLI tool both invoke.

The compile step turns an editable map directory into a self-contained
deployable directory:

```
maps/<name>/compiled/
    pool-runner          ← copy (or symlink) of soramech-pool
    src/                 ← every source file the map uses
    bin/                 ← per-box compiled .so files (C boxes)
    langs/               ← copy of language spec .so files used
    manifest.json        ← every box, its language, artifact path, build version
```

Running `./compiled/pool-runner` executes the map without reference to
the editor, the live `src/` outside `compiled/`, or any external
SoraMech installation.

### Steps the compile step performs

1. **Walk the graph.** Reuse the C loader (issue 305) to enumerate
   boxes, languages, and source files.
2. **Copy source.** Every file referenced by `box.ref` plus
   transitively-required libs lands in `compiled/src/`. The map's
   local `src/` is the only path `package.path` searches at runtime
   (issue 306), so all dependencies must be present.
3. **Compile per-box artifacts.** For each language present, invoke
   that spec's `compile` callback (issue 303). The C spec emits a
   wrapper from the box's signature into `compiled/bin/` (issue 307).
   Lua and Bash have no compile step; their files are copied verbatim.
4. **Copy spec libraries.** `langs/<name>/spec.so` for every language
   used in the map is copied to `compiled/langs/<name>/spec.so`. The
   pool runner's spec discovery (above) finds them relative to the
   binary location.
5. **Copy or symlink the runner binary.** `--portable` copies; the
   default symlinks to save disk on the dev machine.
6. **Write the manifest.** `compiled/manifest.json` lists every box,
   its language, its compiled artifact path (or source file for
   interpreted languages), and the SoraMech build version.

### Incremental compile

mtime comparison per artifact: if the source is newer than the
compiled output, recompile that one; otherwise skip. The C spec's
`compile` callback (issue 307) implements this per-box; the package
step inherits it.

### Editor invocation (issue 222)

The Compile button calls `POST /maps/<name>/compile`. The server
shells out to a `soramech-compile` CLI (or invokes the same code
in-process). Output streams back to the editor for display. The
editor itself never runs the compiled output; the user invokes
`./compiled/pool-runner` from a terminal.

### CLI invocation

Standalone `soramech-compile <map-dir>` runs the same pipeline
without the server. Used in CI and headless deployment.

### Errors

Any failure aborts the compile and surfaces a precise message:
which box, which file, what failed. Partial output is left in
`compiled/` for inspection but the manifest is not written, so the
deployment is recognizably broken until a clean compile succeeds.

### Open questions (compile step)

- **Multi-architecture**: a directory built on x86_64 won't run on
  arm64. Out of scope — users compile on the target. Could later
  add cross-compile if the SoraMind cluster goes mixed-arch.
- **Source-copy granularity**: copy the file referenced by `box.ref`
  plus a static-analysis sweep for `require` / `source` /
  `#include`, or just copy the entire `libs/` tree. Lean toward
  static analysis with a fallback whole-tree copy if analysis fails.

## Open questions

- macOS: `.so` becomes `.dylib`, and there's no `/proc/self/exe`.
  Use `_NSGetExecutablePath`. Cross-platform spec discovery is a
  small platform-#ifdef. Fold into the spec registry implementation.
- Out-of-tree build (a separate `build/` directory holding all
  artifacts): nice-to-have, not blocking. Current layout uses
  in-tree `.o` files in `build/` already — full out-of-tree means
  the `.so`s live in `build/langs/lua/spec.so` instead of next to
  the source. Defer.
- Static vs dynamic linking of vendored libs: vendored sources
  compile straight into the runner binary. No separate `.so` for
  the task pool or JSON parser. Right call given how small they are.

## Suggested implementation sequence

1. Write the top-level Makefile with empty `runner`, `specs`,
   `clean` targets. Verify `make` succeeds against an empty source
   tree.
2. Build the SoraMech thread pool into `libs/task-pool/` (3d-rts
   referenced as design inspiration). Add a
   `VENDOR.md`.
3. Vendor cJSON (or chosen JSON parser) into `libs/json/`. Add a
   `VENDOR.md`.
4. Add the pool-runner build rule. Compiles `src/*.c` plus vendored
   libs into `soramech-pool`. Initially the runner is just `int
   main() { return 0; }` to verify the link.
5. Add `langs/lua/Makefile`. Build `spec.so` from a stub `spec.c`.
6. Add `langs/c/Makefile`. Same shape.
7. Add `langs/bash/Makefile`. Same shape.
8. Add `make DEBUG=1` and `make STRICT=1` modes.
9. Wire `make test` into a placeholder script (real tests come from
   issue 311).

## Relevant files

- `Makefile` — top-level (to be created)
- `langs/lua/Makefile`, `langs/c/Makefile`, `langs/bash/Makefile`
  — per-spec (to be created)
- `libs/task-pool/`, `libs/json/` — vendored sources (to be added)
- `issues/301-pool-lifecycle-and-worker-init.md` — pool runner this
  builds
- `issues/303-language-runtime-spec.md` — spec discovery uses
  `dlopen`/`dlsym`
- `issues/306-lua-language-spec.md`, `307-c-language-spec.md`,
  `308-bash-language-spec.md` — per-spec build details
- `issues/311-integration-tests-and-run-output.md` — `make test`
  consumers
- `issues/222-compile-button-and-assets-directory.md` (completed) —
  the editor button this compile step services
- `issues/completed/219-map-compiler.md` — original issue, folded
  into this one
