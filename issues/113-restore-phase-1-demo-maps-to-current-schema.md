# 113 — restore the phase-1 demo maps to the current schema

## Status

Renumbered from 111 on 2026-08-03. Phase 1 had two issues
numbered 111 — this one and the completed bash-driver-output
issue — so the newer of the pair moved, per the ruling recorded
in issue 112 (record consistency sweep). Every citation moved
with it; no reference to the old number survives, so nothing is
left behind at 111 but the original bash-driver issue.

open · the recovery from git history (commit 890c31b, 2026-07-21)
was **partial** — see current behavior. There are four maps under
`maps/`, not the three this issue was written about, and none of
the four currently loads. Two need migrating, two need rebuilding
from scratch.

## Current behavior

The maps under `maps/` are user content, gitignored since early
phase 3, so nothing in the test suite has ever touched them. At
some point after the gitignore they were gutted on disk. A survey
on 2026-07-25 found all four in distinct states of broken:

| map | on disk | verdict |
|-----|---------|---------|
| `classify-demo` | boxes, meta, drivers, src, data | old dialect — five call boxes carry no `routing` field, the router box uses the removed `branch` kind |
| `hello` | six editor-created boxes, meta, drivers | wire points at a box `write-result` that exists only in `classify-demo`; `meta.json` names entry box `greet`, which is not one of its six box files |
| `driver-test` | **a `drivers/` directory and nothing else** | no boxes, no meta, no drivers.json — the recovery did not restore it |
| `branch-test` | **a `drivers/` directory and nothing else** | same; this map is not mentioned anywhere in this issue's original text |

Two corrections to what this issue previously recorded:

- `hello` was described here as current-schema and as serving the
  server smoke test. It is neither. The smoke test
  (`scripts/test-server.sh`) copies a **different** map —
  `tests/maps/hello`, a tracked fixture — into a scratch directory
  and generates its `drivers.json` at copy time. That fixture is
  healthy and its 13/13 pass says nothing about `maps/hello`. Two
  maps with the same name, one tracked and one not, and the tracked
  one is the one the tests use.
- `driver-test` was described as restored. It was not; only its
  `drivers/` directory came back.

So the effects are:

- `./demo.sh 1` (phase-1 demo) fails at load, loudly and clearly.
- `tests/004-driver-test-runner.lua` (the phase-1 driver-interface
  test) has no map to run against at all — not an old-schema map, an
  absent one. It stays out of the suite, unlike its sibling
  `003-data-test.lua`, which passes and now runs inside
  `scripts/run-tests.sh`.
- The phase-1 demo folder under `issues/completed/demos/` holds only
  a README pointing at `classify-demo`, so the phase-1 demo is
  broken and `demo.sh` cannot tell anyone.

Worth recording as the reason this went unnoticed for months: these
four maps are the only maps in the project that no test sweep
reaches. The 27 fixtures under `tests/maps/` are tracked and
exercised; these are gitignored and exercised by a demo script a
human has to choose to run. Whatever migration happens here, the
four should end up inside the same validation sweep as the fixtures
(see issue 103, step 1) or they will rot again the next time the
format moves.

## Intended behavior

All four maps under `maps/` load and validate under the current
schema. The phase-1 demo runs green through the interpreter
(`./demo.sh 1`), continuing to exercise the foundation
capabilities it was built to show: read → transform → route →
write across Lua and Bash drivers. The driver test folds into the
suite alongside the data test. When the synchronous runner is
retired (phase-3 checklist), the demo moves to the pool runner
with the same map.

Each of the four gets a decided fate, because "broken" means
something different for each:

- `classify-demo` — migrated. It is the phase-1 demo and has real
  content worth keeping.
- `driver-test` — rebuilt. Nothing survives to migrate.
- `hello` — decided, not repaired blindly. A tracked fixture of the
  same name already serves the server tests, so the question is
  whether the untracked one has a job at all. If it doesn't, delete
  it and remove the name collision.
- `branch-test` — decided the same way. It predates the routing
  redesign that removed the branch box kind, so a map named for that
  kind may have no reason to exist; whatever it was testing is
  covered by the routing fixtures under `tests/maps/`
  (`comparator`, `iter-route`, `distributor-route`, and the rest).

## Suggested implementation steps

1. Re-express each `classify-demo` box against the current schema
   per `docs/002-map-model.md`: `routing` field on call boxes, the
   old branch router re-expressed as comparator routing
   (`from_branch` wires), read/write box kinds where the old map
   simulated them with call boxes.
2. Decide the fate of the stray editor-created box
   (`box-mpciwd7h.json`) — it postdates the gutting and belongs to
   no wiring; delete it or wire it in deliberately.
3. Rebuild `driver-test` from what `tests/004-driver-test-runner.lua`
   expects of it — the test is the surviving specification of the
   map, so read the test first and build to it. Then fold that test
   into `scripts/run-tests.sh` the way `003-data-test.lua` was.
4. Rule on `hello` and `branch-test` (keep-and-repair, or delete).
   If `hello` stays, rename one of the two so the tracked fixture and
   the user map can't be confused again.
5. Run `./demo.sh 1` and the suite; both green.
6. Bring these four inside the validation sweep that issue 103 step 1
   adds over `tests/maps/`, so a gitignored map is still a checked
   map. This is the step that keeps the issue from recurring; without
   it the other five are a one-time cleanup.
7. If a third old-dialect map ever surfaces after this, stop and
   build a schema-migration tool instead of hand-migrating again (the
   "create the tool, not the artifact" rule — twice by hand is
   the limit). Note that this issue has now spent two hand-migrations
   on `classify-demo` alone if you count the recovery attempt, so the
   tool may already be the cheaper path.

## Related tools / files

- `maps/classify-demo/`, `maps/driver-test/` — the restored maps
- `docs/002-map-model.md` — the schema to migrate to
- `demo.sh` — phase-1 demo entry (runner path already repaired to
  `src/007-runner-main.lua`)
- `tests/003-data-test.lua` / `tests/004-driver-test-runner.lua` —
  the legacy pair; 003 folded in, 004 blocked on this issue
- issues/completed/233 — the unified routing schema the maps predate
