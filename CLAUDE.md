# SoraMech — project notes

SoraMech is a visual editor plus execution engine for box-and-wire
dataflow programs ("maps"). The map directory IS the program: one
JSON file per box, wires as connection lists, and three programs
sharing that one format — the browser editor (assets/), the Lua
HTTP file-proxy server (src/006-server-main.lua), and the runners
(the synchronous Lua interpreter src/007-runner-main.lua and the
phase-3 C thread-pool runner `soramech-pool`, built from
src/008 through src/020).

## Orientation

- notes/vision — why the project is shaped this way; read first.
- docs/000-table-of-contents.md — the documentation index.
- Every source file has a companion `<name>.info.md`. Prefer
  reading that over the source unless debugging that specific file.
- issues/ holds open blueprints; issues/completed/ is the buildable
  history; issues/phase-N-progress.md are the canonical live
  status pages.

## Build and test

- `make` — build the pool runner and the per-language spec plugins
- `make test` — build, then C unit tests plus integration fixtures
- `scripts/run-unit-tests.sh` — re-run already-built test binaries
- `./run [port]` — start the editor server and open the browser
- `./demo.sh [phase]` — run a phase demo

## House rules (project-specific)

- LuaJIT-compatible Lua only; C for the runtime; no Python.
- Errors over fallbacks: fail loudly, never silently substitute a
  default. Fallbacks are warnings, and warnings are errors.
- Ephemeral output goes through the two-tier RAM scheme:
  tmp/ → /tmp/soramech (exec scratch) and tmp/shared-memory/ →
  /dev/shm/soramech (logs, builds, artifacts).
  scripts/ensure-tmp.sh builds the scheme; scripts invoke it
  before writing logs. Test fixture boxes hard-code
  /dev/shm/soramech/tests/ because box JSON has no variable
  expansion.
- Scripts run from anywhere: hard-coded ${DIR} at the top,
  overridable by argument; every path is relative to ${DIR}.
- Indexed filenames (NNN-name) take the next number from the
  hidden .file-index-counter at the project root; bump it when
  adding a file.
- Issue files are append-only blueprints. When one completes:
  move it to issues/completed/, update the phase progress page,
  and make one commit for that issue's changes only.
