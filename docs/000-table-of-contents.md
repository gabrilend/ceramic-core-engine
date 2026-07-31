# SoraMech — Table of Contents

## Start here

If you're new to SoraMech, read these in order:

- [docs/001-overview.md](001-overview.md) — what SoraMech is, when to reach for it, and a quickstart that walks you through your first map
- [docs/002-map-model.md](002-map-model.md) — the box kinds, routing kinds, wire semantics, and how encapsulated sub-maps compose
- [docs/003-editor.md](003-editor.md) — using the browser editor to build maps visually
- [docs/004-runtime.md](004-runtime.md) — how runs work, what the JSONL transcript records, the compile pipeline, the reference-counted-artifact system
- [docs/005-writing-boxes.md](005-writing-boxes.md) — per-language quickstarts for Lua, C, and Bash box authors
- [docs/006-test-coverage-map.md](006-test-coverage-map.md) — what's tested, what isn't, and what runs the tests

## Working on the runtime itself

- [docs/007-architecture.md](007-architecture.md) — the inside view: the three programs and why they share only a directory, the C runner's module stack bottom to top, the path one value takes from JSON file to downstream byte, the language-plugin boundary, and the known structural debt. This is the doc that older issue files cite as `docs/001-architecture.md`, a path that no longer exists.

## Notes

- [notes/vision](../notes/vision) — the project's original design intent: why the map directory is the program, why the engine is language-agnostic, and the three-programs-one-format split. **Written in the pre-233 dialect** — it describes `branch` and `data` box kinds, `outputs[]` tuples, `from_output` wires, and a Lua runner, none of which the project still has. Read it for intent, not for schema; [docs/002-map-model.md](002-map-model.md) is the current format.

The apartment-building metaphor — the runtime is the steel and concrete, box authors design the rooms — lives in [docs/001-overview.md](001-overview.md), not in the vision.

## Libraries shipped with the runtime

- [libs/soramech-data.info.md](../libs/soramech-data.info.md) — `data.get` / `data.set` / `data.load` API reference
- [libs/text.lua.info.md](../libs/text.lua.info.md) — `text.concat` / `text.split` / `text.replace` / ... API reference
- [libs/files.lua.info.md](../libs/files.lua.info.md) — `files.read_text` / `files.read_data` API reference
- [libs/ollama.lua.info.md](../libs/ollama.lua.info.md) — **deprecated** Ollama client; retirement tracked in the llama.cpp replacement issue (254)

## Project tracking

Per-issue files live under `issues/` (open) and `issues/completed/` (done). The phase-progress index pages are the canonical live status:

- [issues/phase-1-progress.md](../issues/phase-1-progress.md) — phase 1 (foundation): complete
- [issues/phase-2-progress.md](../issues/phase-2-progress.md) — phase 2 (editor + graph model)
- [issues/phase-3-progress.md](../issues/phase-3-progress.md) — phase 3 (C pool runner)
- [issues/phase-4-progress.md](../issues/phase-4-progress.md) — phase 4 (runtime graph mutation): in design
- [issues/phase-5-progress.md](../issues/phase-5-progress.md) — phase 5 (the hardware target: C boxes compiled to HDL, maps placed onto FPGAs): in design
