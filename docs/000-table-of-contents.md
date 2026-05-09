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

- [issues/phase-1-progress.md](../issues/phase-1-progress.md) — phase 1 status (complete)
- [issues/phase-2-progress.md](../issues/phase-2-progress.md) — phase 2 status
- [issues/phase-3-progress.md](../issues/phase-3-progress.md) — phase 3 status

### Phase 1 — Foundation (complete)

- 101 — project scaffold and map directory format
- 102 — language driver interface
- 103 — runner: graph loader and validator
- 104 — runner: synchronous executor (task boundary)
- 105 — HTTP server: file CRUD API
- 106 — web editor: canvas and box rendering
- 107 — web editor: wiring and connection UI
- 108 — branch box and predicate routing (later superseded by 210)
- 109 — data files: persistent and ephemeral storage
- 110 — phase 1 demo

### Phase 2 — Editor & graph model (current)

- 202 — fit-to-view on map load (complete)
- 203 — map picker panel (complete)
- 204 — visible interaction hints (complete)
- 206 — entry box designation (won't implement; phase 3 auto-detects)
- 207 — source file browser and port auto-population (complete)
- 208 — port literal values
- 209 — Ollama query library (complete)
- 210 — comparator wire branching (replaces 108 named ports)
- 211 — file browser library directories (complete)
- 211a — unify src/ into same directory pipeline (complete)
- 212 — editor interaction modes (complete)
- 213 — queued inputs and task model
- 214 — tmp symlink recreation on reboot (complete)
- 215 — view source button (complete)
- 216 — read-file box (`libs/files.lua`)
- 223 — syntax highlighting via user-written lexers
- 217 — concat box and dynamic inputs
- 218 — enforce single-output driver contract (complete)
- 219 — map compiler
- 220 — root run script and entry point cleanup (complete)
- 221 — iterator box: round-robin output routing
- 222 — compile button and assets directory (complete)

### Phase 3 — Thread pool runtime (designed, not yet implemented)

- 301 — thread pool lifecycle and per-worker initialization
- 302 — per-task slot store with wire-held references
- 303 — language runtime spec (pluggable per-language invocation)
- 304 — task dispatch layer (C, replaces synchronous executor)
- 305 — C graph loader (replaces `003-loader.lua`)
- 306 — Lua language spec implementation
- 307 — C language spec implementation
- 308 — Bash language spec implementation
- 309 — build system & Makefile orchestration
- 310 — priority queue: wired in, no-op behaviorally
- 311 — integration tests & run output (`last-run.jsonl`)

### Phase 4 — SoraMind integration (planned)

TBD after phase 3 completes.

### Phase 5 — Map-to-map calls (planned)

TBD.

### Phase 6 — Remote runner (planned)

TBD.

## Libraries

- libs/ — vendored libraries and shipped helpers (dkjson, luasocket, ollama)
- [libs/soramech-data.info.md](../libs/soramech-data.info.md) — `data.get` / `data.set` / `data.load` API reference

## Tests

- tests/ — unit tests (current). Phase 3 integration tests under
  `tests/maps/` per issue 311.
