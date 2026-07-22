# 111 — restore the phase-1 demo maps to the current schema

## Status

open · the map files themselves were recovered from git history
(commit 890c31b) on 2026-07-21; what remains is migrating their
contents to the schema the loader now enforces.

## Current behavior

The maps under `maps/` are user content, gitignored since early
phase 3 — and at some point after that, the working copies of the
phase-1 era maps were gutted on disk: `classify-demo` was reduced
to one stray editor-created box, `driver-test` and `hello` lost
their meta and data files. The files are now restored from
history, and `hello` (whose boxes were still current) again serves
the server smoke test (`scripts/test-server.sh`, 13/13).

But `classify-demo` and `driver-test` predate the unified routing
schema (issue 233): their call boxes carry no `routing` field, and
the router box uses the removed `branch` kind. Today's loader
rejects them with per-box errors, so:

- `./demo.sh 1` (phase-1 demo) fails at load, loudly and clearly.
- `tests/004-driver-test-runner.lua` (the phase-1 driver-interface
  test) fails on the missing/old-schema `driver-test` map, and is
  therefore not folded into the suite — unlike its sibling
  `003-data-test.lua`, which passes and now runs inside
  `scripts/run-tests.sh`.

## Intended behavior

`classify-demo` and `driver-test` are valid maps under the current
schema. The phase-1 demo runs green through the interpreter
(`./demo.sh 1`), continuing to exercise the foundation
capabilities it was built to show: read → transform → route →
write across Lua and Bash drivers. The driver test folds into the
suite alongside the data test. When the synchronous runner is
retired (phase-3 checklist), the demo moves to the pool runner
with the same map.

## Suggested implementation steps

1. Re-express each `classify-demo` box against the current schema
   per `docs/002-map-model.md`: `routing` field on call boxes, the
   old branch router re-expressed as comparator routing
   (`from_branch` wires), read/write box kinds where the old map
   simulated them with call boxes.
2. Decide the fate of the stray editor-created box
   (`box-mpciwd7h.json`) — it postdates the gutting and belongs to
   no wiring; delete it or wire it in deliberately.
3. Same migration for `driver-test`, then fold
   `004-driver-test-runner.lua` into `scripts/run-tests.sh` the
   way `003-data-test.lua` was.
4. Run `./demo.sh 1` and the suite; both green.
5. If a third old-schema map ever surfaces, stop and build a
   schema-migration tool instead of hand-migrating again (the
   "create the tool, not the artifact" rule — twice by hand is
   the limit).

## Related tools / files

- `maps/classify-demo/`, `maps/driver-test/` — the restored maps
- `docs/002-map-model.md` — the schema to migrate to
- `demo.sh` — phase-1 demo entry (runner path already repaired to
  `src/007-runner-main.lua`)
- `tests/003-data-test.lua` / `tests/004-driver-test-runner.lua` —
  the legacy pair; 003 folded in, 004 blocked on this issue
- issues/completed/233 — the unified routing schema the maps predate
