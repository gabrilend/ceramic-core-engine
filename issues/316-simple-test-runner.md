# 316 — Simple test runner script

## Status
open

## Current behavior

`make test` builds and runs every unit-test binary plus the
integration runner. The output is helpful when you want to verify
that everything still passes at the end of a working session, but
it's noisy when you're iterating on one specific change:

- The build phase prints `→ src/foo.c`, `→ src/bar.c`, ... for
  every file that compiles or relinks. On a clean tree that's
  dozens of lines before the first test runs.
- Tests are run from inside a Make recipe (a `for` loop). If one
  binary fails, the recipe aborts and you don't see the results
  for binaries that would have run after it.
- The integration runner (`scripts/run-tests.sh`) is interleaved
  with unit tests, mixing two scales of test output in one stream.

The current behavior is fine for "ship it" verification. It's
suboptimal as a development loop.

## Intended behavior

A separate script (`scripts/run-unit-tests.sh`) that runs the
already-built unit-test binaries one by one and reports their
results in a uniform format. No build step. No make. Just:
locate the binaries, run them in turn, capture stdout/stderr,
print a one-line per-binary summary, and at the end print a
totals line and a list of any failures.

Goals:

1. **Cheap to invoke.** No make recursion, no spec rebuilds,
   no integration tests. If the build is current, the script
   runs in well under a second.
2. **Continues past failures.** Every test binary runs, even if
   an earlier one fails. The summary at the bottom names what
   went wrong.
3. **Pretty output by default; mineable output on request.** A
   `--quiet` mode that prints just the totals line is enough for
   CI pre-merge gates. A `--verbose` mode dumps each binary's
   full output. The default is per-binary one-line status.
4. **Filterable.** A positional argument like
   `scripts/run-unit-tests.sh 009` runs only the binaries whose
   names start with `009`, so iterating on one slot-store change
   doesn't require re-running every test.

Out of scope: building the binaries, running integration tests
(that's `scripts/run-tests.sh`), running specs (those are
implicitly tested via the binaries that depend on them).

## Concept

The build system already produces `build/tests/<name>-test`
binaries; one per test source file under `tests/`. The script
discovers them via a glob, sorts them by filename (which sorts
by leading numeric index, which is the project's natural
read-order), and runs each one.

Each binary already prints its own header (`009-slot-store-test:`)
and per-test status lines. The script captures the output, parses
the last line (`N passed, M failed`) for the per-binary verdict,
and prints a per-binary summary like:

```
  009-slot-store-test                          16 / 16  ok
  010-graph-loader-test                        13 / 13  ok
  015-large-value-heap-test                     8 /  8  ok
  ...
```

A failing binary prints its captured output below the summary
line so the failure is visible without re-running.

The very last line is:

```
  113 / 113 tests passed (14 binaries)
```

…or, on failure:

```
  111 / 113 tests passed (14 binaries; 1 binary with failures: 009-slot-store-test)
```

Exit code: zero if every binary passes; non-zero with the count
of failing binaries otherwise.

## Convention compliance

Per the project's script rules (CLAUDE.md):

- Hard-coded `${DIR}` at the top; first argument optionally
  overrides it.
- All paths under `${DIR}`.
- Each shell command on its own line (no chained `||` /
  `&&` pipelines for control flow; assign command output to
  variables and branch on the variable).
- Lives at `scripts/run-unit-tests.sh`, runnable from any cwd.

## Suggested implementation steps

1. Write `scripts/run-unit-tests.sh` with the layout described
   above.
2. Verify it picks up every existing `build/tests/*-test`
   binary and produces the expected totals.
3. Verify the filter argument works: `scripts/run-unit-tests.sh
   015` runs just the lvh test binary.
4. Verify `--quiet` and `--verbose` modes both produce useful
   output.
5. Provoke a failure (modify one test temporarily) and confirm
   the failure is reported, exit code is non-zero, and the
   captured output is visible.
6. Document the script in `Makefile`'s `help` target, or add a
   `make quicktest` alias that calls it.

## Relevant files

- `scripts/run-unit-tests.sh` — new
- `Makefile` — optional `quicktest` target
- `scripts/run-tests.sh` — sibling script for integration tests;
  this issue covers the unit-test counterpart

## Open questions

- Should the script also verify the binaries are up to date with
  their sources? Cheap: `stat` the binary and its `.c` and
  complain if the source is newer. The user might prefer "just
  run what's built" so they can deliberately test old binaries.
  Default to no warning; offer a `--check-stale` flag.
- Filter syntax: prefix-match on the test name is simple; should
  it accept glob patterns too? Start simple. Extend if needed.
