# SoraMech — Table of Contents

## Start here

If you're new to SoraMech, read these in order:

- [docs/001-overview.md](001-overview.md) — what SoraMech is, when to reach for it, and a quickstart that walks you through your first map
- [docs/002-map-model.md](002-map-model.md) — the box kinds, routing kinds, wire semantics, and how encapsulated sub-maps compose
- [docs/003-editor.md](003-editor.md) — using the browser editor to build maps visually
- [docs/004-runtime.md](004-runtime.md) — how runs work, what the JSONL transcript records, the compile pipeline, the reference-counted-artifact system
- [docs/005-writing-boxes.md](005-writing-boxes.md) — per-language quickstarts for Lua, C, and Bash box authors
- [docs/006-test-coverage-map.md](006-test-coverage-map.md) — what's tested, what isn't, and what runs the tests

## Notes

- [notes/vision](../notes/vision) — the project's design intent and the apartment-building metaphor that frames why SoraMech is shaped the way it is

## Libraries shipped with the runtime

- [libs/soramech-data.info.md](../libs/soramech-data.info.md) — `data.get` / `data.set` / `data.load` API reference
- [libs/text.lua.info.md](../libs/text.lua.info.md) — `text.concat` / `text.split` / `text.replace` / ... API reference
- [libs/files.lua.info.md](../libs/files.lua.info.md) — `files.read_text` / `files.read_data` API reference

## Project tracking

Per-issue files live under `issues/` (open) and `issues/completed/` (done). The phase-progress index pages are the canonical live status:

- [issues/phase-1-progress.md](../issues/phase-1-progress.md) — phase 1 (foundation): complete
- [issues/phase-2-progress.md](../issues/phase-2-progress.md) — phase 2 (editor + graph model)
- [issues/phase-3-progress.md](../issues/phase-3-progress.md) — phase 3 (C pool runner)
