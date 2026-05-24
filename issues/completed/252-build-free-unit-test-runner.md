# 252 — Cheap, build-free unit-test runner

## Status
complete — `scripts/run-unit-tests.sh` ships with the
pretty-default + `--quiet` + `--verbose` + name-prefix filter
modes the design called for. `make quicktest` is the alias;
`make quicktest ARGS=009` forwards the filter. The runner
honestly identifies pre-existing failures (a longstanding
`307-c-spec-test` failure surfaces immediately when the
runner first runs against the built tree).

## Current behavior

`make test` builds the C unit-test binaries, runs them, then
invokes `scripts/run-tests.sh` for the integration fixtures.
That's the right shape for "ship it" verification, but it's
awkward as a development loop:

- The build phase prints `→ src/foo.c` for every file, dozens of
  lines before the first test runs on a clean tree.
- Tests run inside a Make recipe; one binary failing aborts the
  recipe, so later binaries that would have run aren't reported.
- Unit-test and integration-test output interleave in one stream.

## Intended behavior

A cheap, build-free runner script — `scripts/run-unit-tests.sh`
— that runs the already-built C unit-test binaries without
re-invoking Make and without touching the integration fixtures.

Goals:

1. **Cheap.** No make recursion, no spec rebuilds, no integration
   tests. Under a second on a current build.
2. **Continues past failures.** Every binary runs; summary at
   the bottom names what went wrong.
3. **Pretty by default, mineable on request.** Per-binary
   one-line status by default; `--quiet` prints only totals;
   `--verbose` dumps each binary's full output.
4. **Filterable.** `scripts/run-unit-tests.sh 009` runs only
   binaries whose names start with `009`.

## Concept

Glob `build/tests/<name>-test`, sort by filename (which sorts by
the project's leading numeric index), run each in turn, capture
stdout/stderr, parse the binary's trailing `N passed, M failed`
line for the verdict.

Output shape:

```
  009-slot-store-test                          16 / 16  ok
  010-graph-loader-test                        13 / 13  ok
  015-large-value-heap-test                     8 /  8  ok
  ...
  113 / 113 tests passed (14 binaries)
```

A failing binary prints its captured output below the summary
line. Exit code is zero iff every binary passes; non-zero with
the count of failing binaries otherwise.

Convention compliance per CLAUDE.md: hard-coded `${DIR}` at top
with arg override, all paths under `${DIR}`, each shell command
on its own line, lives at `scripts/run-unit-tests.sh`.

## Suggested implementation steps

1. Write `scripts/run-unit-tests.sh` with the layout above.
2. Verify it picks up every existing `build/tests/*-test` binary
   and produces the expected totals.
3. Verify `scripts/run-unit-tests.sh 015` filters to one binary.
4. Verify `--quiet` and `--verbose` produce useful output.
5. Provoke a failure (temporarily modify one test) and confirm
   the failure is reported, exit code non-zero, captured output
   visible.
6. Add a `make quicktest` alias or document the script in the
   Makefile's `help` target.

## Open questions

- **Stale-binary check.** Should the script warn when a test's
  source is newer than its built binary? Cheap (`stat` both
  sides) but the default should be "just run what's built" —
  offer it as `--check-stale`.
- **Parse format coupling.** Parsing each binary's trailing
  `N passed, M failed` line couples the runner to the test
  binaries' output format. Acceptable today (every test uses
  the same convention) but worth documenting so test authors
  don't accidentally diverge.

## Related documents

- `docs/006-test-coverage-map.md` — the test inventory this
  runner exercises.
- The historical `issues/completed/316-simple-test-runner.md`
  was the original home of this design; the work folded into
  the old 232 issue file and now ends up here as its own
  ticket so the runner script can be tracked independently.
