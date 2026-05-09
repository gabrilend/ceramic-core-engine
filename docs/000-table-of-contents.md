# SoraMech — Table of Contents

Read in order for a complete picture of the system.

## Notes

- [notes/vision](../notes/vision) — what this is and why it has this shape

## Documentation

- [docs/001-architecture.md](001-architecture.md) — components, data flow, file format spec
- [docs/002-roadmap.md](002-roadmap.md) — phases, goals, and sequencing
- [docs/003-driver-system.md](003-driver-system.md) — phase 2 driver scripts; phase 3 language spec overview
- [docs/004-ipc-and-threading.md](004-ipc-and-threading.md) — IPC options, threading roadmap, blocking semantics, Unix domain socket model
- [docs/005-language-specs.md](005-language-specs.md) — guide to writing a language spec (stub; filled out as 306–308 are implemented)

## Source

- src/ — server, runner, and shared utilities (populated as issues complete)

## Issues

Per-issue files live under `issues/` (open) and `issues/completed/`
(done). The progress files below are the canonical index — read them
for the live status of each phase.

- [issues/phase-1-progress.md](../issues/phase-1-progress.md) — phase 1 status (complete)
- [issues/phase-2-progress.md](../issues/phase-2-progress.md) — phase 2 status
- [issues/phase-3-progress.md](../issues/phase-3-progress.md) — phase 3 status

## Libraries

- libs/ — vendored libraries and shipped helpers (dkjson, luasocket, ollama)
- [libs/soramech-data.info.md](../libs/soramech-data.info.md) — `data.get` / `data.set` / `data.load` API reference
- [libs/text.lua.info.md](../libs/text.lua.info.md) — `text.concat` / `text.split` API reference
- [libs/files.lua.info.md](../libs/files.lua.info.md) — `files.read_text` / `files.read_data` API reference

## Tests

- tests/ — unit tests (current). Phase 3 integration tests under
  `tests/maps/` per issue 311.
