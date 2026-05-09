# 219 — Map compiler: package a map for standalone execution

## Status
open

## Current behavior
Phase 2's synchronous runner interprets the box graph at runtime,
shelling out to a language driver per box. The runner is a Lua
script (`src/007-runner-main.lua`); to run a map, the user must have
SoraMech installed.

Phase 3 introduces the C pool runner (`soramech-pool`, issues
301–311) plus per-language specs under `langs/`. The runner is a
real binary, but it still reads the map directory at startup. The
"compile" step bridges from "an editable map directory" to "a
self-contained deployable directory" that includes the binary, all
source files, all compiled `.so`s, and a manifest.

## Intended behavior

The compile step (triggered by the editor's Compile button, issue
222, or by the CLI) walks a map and produces a `compiled/`
subdirectory:

```
maps/<name>/compiled/
    pool-runner          ← the soramech-pool binary, copied here
    src/                 ← copy of every source file the map uses
    bin/                 ← per-box compiled .so files (C boxes)
    langs/               ← copy of language spec .so files needed
    manifest.json        ← every box, its language, its compiled artifact path
```

The `compiled/` directory is the deployment artifact. Running
`./compiled/pool-runner` executes the map without any reference to
the editor, the live `src/` outside `compiled/`, or any external
SoraMech installation. Ship the directory to someone else and it
runs.

### What the compile step does

1. **Walk the graph.** Use the same loader logic the runtime uses
   (issue 305) to enumerate boxes, languages, and source files.
2. **Copy source.** Every file referenced by `box.ref` is copied
   into `compiled/src/`. The map's local `src/` is the only place
   `package.path` searches at runtime (per issue 306), so all
   dependencies must land here. This includes vendored libraries
   the user wired in (`libs/json.lua`, `libs/ollama.lua`, etc.).
3. **Compile per-box artifacts.** For each language present in the
   map, invoke that spec's `compile` callback (issue 303). The C
   spec generates a wrapper from the box's declared signature and
   produces a `.so` in `compiled/bin/` (issue 307). Lua and Bash
   specs have no compile step; their files are copied verbatim.
4. **Copy spec libraries.** The `langs/<name>/spec.so` files for
   every language used in the map are copied to
   `compiled/langs/<name>/spec.so`. The pool runner discovers
   specs relative to its binary location, so this co-location is
   what makes the deployment self-contained.
5. **Copy the runner binary.** `soramech-pool` is copied to
   `compiled/pool-runner`. (A symlink works too, for development.)
6. **Write the manifest.** `compiled/manifest.json` lists every
   box, its language, the path of its compiled artifact (or source
   file for interpreted languages), and the SoraMech build version
   the deployment was compiled against.

### What stays in the box JSON
Canvas positions, labels, wire layout — everything the editor needs
to display and edit the graph. The JSON is the editor's
representation of the graph. The compiled directory is the
deliverable. The two never need to be in sync after compile —
the compiled output is frozen at compile time.

### Editor invocation (issue 222)
The Compile button in the editor triggers the same compile step via
a new server endpoint `POST /maps/<name>/compile`. The server
shells out to a `soramech-compile` CLI (or invokes the same code
in-process — implementation detail). Output / errors stream back to
the editor for display. The editor itself does not run the
compiled output; the user invokes `./compiled/pool-runner` from a
terminal.

This supersedes the earlier "no compile button in the editor" stance
in the original issue text — issue 222 is the formal addition of
the button.

### CLI invocation
A standalone CLI tool `soramech-compile <map-dir>` runs the same
pipeline without the server. Useful for CI and headless deployment.

## Design constraints

- The editor still has no conception of *running* maps — only
  compiling them. Run is a separate concern (the user invokes the
  pool-runner binary themselves).
- Compile is incremental: mtime comparison on each artifact. If a
  source file is newer than its compiled output, re-compile that
  one. Otherwise skip. (Per the C spec, issue 307.)
- Errors at any stage abort the compile and surface a precise
  message (which box, which file, what failed).

## Open questions

- Whether the compiled directory bundles a copy of `soramech-pool`
  or a symlink. Bundling makes the directory portable across
  machines; symlinking saves disk on the dev machine. Configurable
  via a `--portable` flag.
- Multi-architecture: a compiled directory built on x86_64 won't
  run on arm64. Out of scope — users compile on the target
  architecture. Could later add a cross-compile mode if the SoraMind
  cluster is mixed-arch.

## Suggested implementation sequence

1. Walk the map graph using the C loader (issue 305) — the same
   code the runtime uses. Emit a list of (box, language, files).
2. Implement the source-copy step. Every `box.ref` file plus any
   `require`/`source`/`#include` dependencies (resolvable via
   static analysis or just-copy-the-whole-libs/-directory).
3. For C boxes: invoke the C spec's `compile` callback (issue 307)
   per box. Outputs land in `compiled/bin/`.
4. Copy `langs/<name>/spec.so` for every language present.
5. Copy or symlink `soramech-pool`.
6. Write `manifest.json`.
7. Wire `POST /maps/<name>/compile` in the server (issue 222
   button calls this).
8. Smoke test: compile `maps/hello`, run
   `./maps/hello/compiled/pool-runner`, verify result matches
   the phase 2 run.

## Relevant files

- `src/008-pool-runner.c` — the runner binary that gets copied in
- `langs/<name>/spec.so` — language specs that get copied in
- `langs/<name>/spec.c::compile` — per-language compile callback
- `issues/222-compile-button-and-assets-directory.md` — UI button
  that triggers this
- `issues/303-language-runtime-spec.md` — `compile` interface
- `issues/305-c-graph-loader.md` — graph-walk logic shared with
  the runtime
- `issues/307-c-language-spec.md` — C wrapper generation
- `docs/001-architecture.md` — describes the `compiled/` directory
  layout
- `issues/completed/102-language-driver-interface.md` — phase 2
  driver model (historical reference)
