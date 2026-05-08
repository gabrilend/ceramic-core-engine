# SoraMech — Table of Contents

Read in order for a complete picture of the system.

## Notes

- [notes/vision](../notes/vision) — what this is and why it has this shape

## Documentation

- [docs/001-architecture.md](001-architecture.md) — components, data flow, file format spec
- [docs/002-roadmap.md](002-roadmap.md) — phases, goals, and sequencing
- [docs/003-driver-system.md](003-driver-system.md) — language driver interface spec
- [docs/004-ipc-and-threading.md](004-ipc-and-threading.md) — IPC options, threading roadmap, Unix domain socket model

## Source

- src/ — server, runner, and shared utilities (populated as issues complete)

## Issues

- [issues/phase-1-progress.md](../issues/phase-1-progress.md) — phase 1 status

### Phase 1 — Foundation

- 101 — project scaffold and map directory format
- 102 — language driver interface
- 103 — runner: graph loader and validator
- 104 — runner: synchronous executor (task boundary)
- 105 — HTTP server: file CRUD API
- 106 — web editor: canvas and box rendering
- 107 — web editor: wiring and connection UI
- 108 — branch box and predicate routing
- 109 — data files: persistent and ephemeral storage
- 110 — phase 1 demo

### Phase 2 — Concurrent execution

- TBD after phase 1 completes

### Phase 3 — SoraMind integration

- TBD

## Libraries

- libs/ — vendored libraries (dkjson, luasocket)
- [libs/soramech-data.info.md](../libs/soramech-data.info.md) — data.get / data.set / data.load API reference

## Tests

- tests/003-data-test.lua — data library unit tests (run with `luajit tests/003-data-test.lua`)
