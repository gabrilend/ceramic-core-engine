# 222 — Compile button and assets directory

## Status
complete (button placeholder; backend wired by issue 219)

## Implementation notes

The button exists in the toolbar as `⚙ compile`, next to the `⬇ png`
export. Click handler `App.compile_map` logs `compile clicked` and
shows a status message noting the backend is not wired up yet —
that work belongs to issue 219.

The compiled-directory layout is captured in a comment on
`compile_map` so the structure is in one place when 219 fills it in:

```
compiled/pool-runner    — the C pool runner binary
compiled/src/           — copy of every source file the map uses
compiled/bin/           — per-box compiled .so files (C boxes)
compiled/langs/         — language spec .so files needed
compiled/manifest.json  — every box, its language, its artifact path
```

Open question about disabling the button until an entry box is set
is moot — issue 206 was closed (won't implement) because phase 3
auto-detects entry boxes. The button is always enabled.

## Open questions remaining
None — the second open question (toolbar placement) is resolved by
putting the button in the right-side group with `⬇ png`, both being
"output" actions.

## Current behavior
The editor has no mechanism to trigger compilation of a map. Maps are only
runnable via the synchronous interpreter path (`src/007-runner-main.lua`).
There is no compiled artifact, no assets directory, and no binary output.

## Intended behavior

### UI: compile button
A "Compile" button appears in the editor toolbar. Clicking it does nothing
yet — it is a placeholder for the compilation pipeline (issue 219). The
button is present so the UI layout is established before the backend is wired
in.

The button label is "Compile". It has no loading state, no progress
indicator, and no output for now. It simply exists in the toolbar next to
the existing controls.

### Compiled artifacts: assets directory
When compilation is eventually wired in, pressing the button triggers a
build that produces a self-contained output directory alongside the map:

```
maps/<map-name>/compiled/
    pool-runner          ← the compiled C entry point (008-pool-runner.c)
    src/                 ← copy of all source files used by the map
    bin/                 ← compiled per-box binaries (C boxes → .so or binary)
    manifest.json        ← list of every box, its language, and its binary path
```

The compiled directory is the map's deployment artifact. Running
`maps/<map-name>/compiled/pool-runner` executes the map using the thread
pool executor, with no dependency on the editor or the Lua interpreter path.

The `src/` copy inside `compiled/` is what the pool runner references at
runtime — box files are not read from the map's live src/ during execution.
This isolates deployed maps from edits in progress.

### Language-agnostic design note
The compile step does not treat any language as special. Each box's language
determines which invocation wrapper is selected at compile time. The same
build path handles a Lua box, a C box, and a Bash box without branching on
the language at the top level. The per-language wrapper spec (issue 303) is
what parameterizes the build for each box.

## Open questions
- Should the compile button be disabled until the map has at least one entry
  box designated? Or always enabled (and let the backend reject invalid maps)?
- Where in the toolbar does it live — left side with map controls, or right
  side with a "run" / "export" group?

## Suggested implementation sequence
1. Add a "Compile" button element to the editor toolbar in `assets/index.html`.
2. Wire a click handler in `assets/js/005-app.js` that logs "compile clicked"
   (placeholder). No server call yet.
3. Leave the `compiled/` directory layout as a comment in the handler — the
   structure is established here for when issue 219 fills it in.

## Relevant files
- `assets/index.html` — editor toolbar
- `assets/js/005-app.js` — click handler
- `issues/219-map-compiler.md` — compilation pipeline that this button triggers
- `issues/303-language-runtime-spec.md` — per-language invocation wrapper spec
  (to be written; parameterizes what goes into `compiled/bin/`)
