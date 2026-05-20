# 232 — Test coverage map

## Status
ongoing (test inventory; updated each time tests are added or
removed)

## Why this exists

This issue is the project-wide answer to "what's tested, what
isn't, and what runs the tests." It folds together two earlier
threads:

- **316** (now folded in here) — a cheap, build-free unit-test
  runner script. Its scope lives under "Test runners → Planned"
  below.
- The parser-test work that opened this issue (Node-side tests
  for `langs/<name>/parser.js`).

A related but separate issue is **311** — phase-3 integration
tests and the JSONL run-output format. 311's scope is the JSONL
writer + the `tests/maps/<name>/` fixtures driven through
`soramech-pool`; the coverage of those fixtures shows up in the
"End-to-end fixtures" section below, but the writer/format work
stays in 311.

Without a coverage view, gaps accumulate silently: tests get
written for the area someone happens to be touching, and the rest
drifts uncovered. The checklist below is that view, updated every
time tests are added or removed.

Every time a new test file lands or a test gap is closed, the
checklist below is updated. When a new module is added, a row goes
into the appropriate section showing it has no coverage yet.

## How to read the table

- `[x]` — has a dedicated test file (path in the row).
- `[ ]` — no dedicated test; may be exercised indirectly through
  integration fixtures, but not directly verified.
- `[~]` — partially covered; only some surface area is tested.
- `[—]` — intentionally untested (vendored library, thin entrypoint, etc.).

## Editor side — JavaScript

The editor is a browser app served by `src/005-http-server.lua`.
No JS test runner was wired up before the work that opened this
issue; the parser tests introduced one (Node's built-in
`node:test`). Other JS modules can follow the same pattern when
they get tests.

| Module | Coverage | Test file |
|---|---|---|
| `langs/lua/parser.js` | `[x]` | `tests/232-lua-parser-test.mjs` |
| `langs/bash/parser.js` | `[x]` | `tests/233-bash-parser-test.mjs` |
| `langs/c/parser.js` | `[—]` | not written yet; issue 231 makes it optional |
| `langs/lua/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
| `langs/bash/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
| `langs/c/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
| `langs/lua/lexer.js` | `[ ]` | — |
| `langs/bash/lexer.js` | `[ ]` | — |
| `langs/c/lexer.js` | `[ ]` | — |
| `assets/js/001-canvas.js` | `[ ]` | — |
| `assets/js/002-boxes.js` | `[ ]` | — |
| `assets/js/003-api.js` | `[ ]` | — |
| `assets/js/004-inspector.js` | `[ ]` | — (variadic helpers are pure functions, prime unit-test candidates) |
| `assets/js/005-app.js` | `[ ]` | — |
| `assets/js/006-wires.js` | `[ ]` | — |
| `assets/js/007-filebrowser.js` | `[ ]` | — |
| `assets/js/008-source-view.js` | `[ ]` | — |

Highest-value next targets: the variadic-slot helpers in
`004-inspector.js` (pure functions, no DOM dependency, directly
consumed by issue 217's Part C inspector gate) and the lexer
registries in the language directories.

## Lua libraries — `libs/*.lua`

| Module | Coverage | Test file |
|---|---|---|
| `libs/soramech-data.lua` | `[x]` | `tests/003-data-test.lua` |
| `libs/text.lua` | `[ ]` | — (issue 237 expanded the surface: concat, split, upper, lower, trim, replace, replace_first, contains, starts_with, ends_with, length, substring) |
| `libs/files.lua` | `[ ]` | — |
| `libs/ollama.lua` | `[—]` | requires a live Ollama endpoint; integration-level only |
| `libs/dkjson.lua` | `[—]` | vendored third-party |

`libs/text.lua` is the most important gap — every function is a
pure string operation with obvious edge cases (empty input, nil
input, no match, plain vs. pattern), exactly the shape unit tests
catch best.

## Lua server side — `src/*.lua`

| Module | Coverage | Test file |
|---|---|---|
| `src/001-schema.lua` | `[ ]` | — (validates box JSON; deserves direct accept/reject tests) |
| `src/002-validate-map.lua` | `[ ]` | — (CLI map validator; only exercised via integration fixtures) |
| `src/003-loader.lua` | `[ ]` | — |
| `src/004-executor.lua` | `[~]` | exercised end-to-end via `tests/004-driver-test-runner.lua` and integration maps; no unit-level coverage |
| `src/005-http-server.lua` | `[ ]` | — (GET/PUT boxes, connections, files, source) |
| `src/006-server-main.lua` | `[—]` | thin entrypoint |
| `src/007-runner-main.lua` | `[—]` | thin entrypoint |

Schema and validator are the highest-value gaps — they are the
last line of defense against malformed map files and they're pure
functions over JSON-shaped tables, very testable.

## C runtime — `src/*.c`

Best-covered area of the project; phase 3 was built test-first.

| Module | Coverage | Test file |
|---|---|---|
| `src/008-pool-runner.c` | `[x]` | `tests/301-pool-test.c`, `tests/303-pool-spec-init-test.c` |
| `src/009-slot-store.c` | `[x]` | `tests/009-slot-store-test.c` |
| `src/010-graph-loader.c` | `[x]` | `tests/010-graph-loader-test.c` |
| `src/011-spec-registry.c` | `[x]` | `tests/011-spec-registry-test.c` |
| `src/012-dispatch.c` | `[x]` | `tests/012-dispatch-test.c` |
| `src/013-jsonl-events.c` | `[x]` | `tests/013-jsonl-events-test.c` |
| `src/014-event-queue.c` | `[x]` | `tests/014-event-queue-test.c` |
| `src/015-large-value-heap.c` | `[x]` | `tests/015-large-value-heap-test.c` |
| `libs/json/json.c` | `[x]` | `tests/314-json-test.c` |

## Language specs — `langs/<name>/spec.c`

| Spec | Coverage | Test file |
|---|---|---|
| `langs/lua/spec.c` | `[x]` | `tests/306-lua-spec-test.c` |
| `langs/c/spec.c` | `[x]` | `tests/307-c-spec-test.c` |
| `langs/bash/spec.c` | `[x]` | `tests/308-bash-spec-test.c` |

## End-to-end fixtures — `tests/maps/`

Integration coverage owned by issue 311. Listed here so the map
isn't blind to it.

| Fixture | What it exercises |
|---|---|
| `calc` | single Lua box |
| `hello` | data → Lua, string output |
| `comparator` | comparator branching (lt/eq/gt) |
| `iter-route` | iterator + routing |
| `pipeline` | five-box multilang chain (also covers the compile pipeline, issue 309) |

## Test runners

### Today
- `scripts/run-tests.sh` — integration fixtures + the JS parser
  tests added by the work that opened this issue. The single entry
  point for "run everything the build doesn't run." Folds Node-test
  results into the same pass/fail counter as the integration checks.
- `make test` — builds the C unit-test binaries, runs them, then
  invokes `scripts/run-tests.sh`.

### Planned — `scripts/run-unit-tests.sh` (folded in from issue 316)

A cheap, build-free runner for the C unit-test binaries. `make
test` builds and runs everything; that's fine for "ship it"
verification, awkward as a development loop:

- The build phase prints `→ src/foo.c` for every file, dozens of
  lines before the first test runs on a clean tree.
- Tests run inside a Make recipe; one binary failing aborts the
  recipe, so later binaries that would have run aren't reported.
- Unit-test and integration-test output interleave in one stream.

Goals for the new script:

1. **Cheap.** No make recursion, no spec rebuilds, no integration
   tests. Under a second on a current build.
2. **Continues past failures.** Every binary runs; summary at the
   bottom names what went wrong.
3. **Pretty by default, mineable on request.** Per-binary one-line
   status by default; `--quiet` prints only totals; `--verbose`
   dumps each binary's full output.
4. **Filterable.** `scripts/run-unit-tests.sh 009` runs only
   binaries whose names start with `009`.

Concept: glob `build/tests/<name>-test`, sort by filename (which
sorts by the project's leading numeric index), run each in turn,
capture stdout/stderr, parse the binary's trailing
`N passed, M failed` line for the verdict.

Output shape:
```
  009-slot-store-test                          16 / 16  ok
  010-graph-loader-test                        13 / 13  ok
  015-large-value-heap-test                     8 /  8  ok
  ...
  113 / 113 tests passed (14 binaries)
```

A failing binary prints its captured output below the summary
line. Exit code is zero iff every binary passes; non-zero with the
count of failing binaries otherwise.

Convention compliance per CLAUDE.md: hard-coded `${DIR}` at top
with arg override, all paths under `${DIR}`, each shell command on
its own line, lives at `scripts/run-unit-tests.sh`.

Open question (from 316): should the script warn when a binary's
source is newer than the binary? Cheap (`stat` both sides), but
the default should be "just run what's built" — offer it as
`--check-stale`.

### Implementation steps for the new runner
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

## Suggested next test additions (ranked)

1. **`libs/text.lua`** — pure-Lua tests next to
   `tests/003-data-test.lua`. Issue 237's expansion left a wide
   untested surface and consumers (issue 217 part C, issue 224)
   are about to multiply.
2. **`src/001-schema.lua`** — accept/reject tests for box JSON.
   Cheap, and the schema gates everything the executor sees.
3. **Inspector variadic helpers** (`assets/js/004-inspector.js`) —
   `parse_variadic_name`, `is_variadic_slot`, `variadic_slots_for`,
   `last_variadic_index`, `make_variadic`, `unmake_variadic`,
   `add_variadic_slot`, `remove_variadic_slot`. All pure, all
   feeding into the part-C work.
4. **Lexer round-trips** for the three shipped languages — feed a
   known source through `tokenize`, check the token stream.
5. **`langs/c/parser.js`** — write it and its tests together when
   issue 231 commits to a C parser.

## Implementation log

### 2026-05-19 — parser tests + Node runner integration

`tests/232-lua-parser-test.mjs` and `tests/233-bash-parser-test.mjs`
added under Node's built-in `node:test`. They run via
`scripts/run-tests.sh`, which gained a `parser_tests()` section
that calls `SORAMECH_DIR="$DIR" node --test "$tf"` for each file
and folds the result into the existing pass/fail summary.

`SORAMECH_DIR=...` overrides the project root when running outside
the default install path. Argv would collide with `node --test`'s
file-list parsing, so an env var is the right channel.

Verified failure detection by temporarily inverting a parser
check — the relevant test failed as expected and the failure
surfaced through the shell runner.

### 2026-05-19 — language-spec test, variadic_tail retired

The earlier pass added a `variadic_tail` field to both parsers
along with assertions in the parser tests. That model — per-
function detection of variadic capability by source regex — was
fundamentally wrong for the project's vision (every shipped
language already accepts N positional args at call time; "is this
function variadic" is a question the runtime doesn't care about).
See issue 217's rewrite for the full reasoning.

The fix in this pass:

- `variadic_tail` removed from `langs/lua/parser.js` and
  `langs/bash/parser.js`. The parsers still extract named
  parameters and outputs; the `...` token is still stripped from
  inputs so the editor shows only named ports.
- `tests/232-lua-parser-test.mjs` and
  `tests/233-bash-parser-test.mjs` had their variadic_tail
  assertions removed; they now lock the trimmed contract.
- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
  new. Each exports `LANGUAGE_SPEC` with `variadic_shape:
  'positional'`. This is the canonical home for editor-facing
  language metadata.
- `tests/234-language-spec-js-test.mjs` — new. Asserts every
  spec.js exports a valid `LANGUAGE_SPEC` with a known
  `variadic_shape`.

10 / 10 passing in the full runner.

## Relevant files

- `scripts/run-tests.sh` — the integration + JS test driver
- `tests/232-lua-parser-test.mjs`, `tests/233-bash-parser-test.mjs`
- `issues/completed/311-integration-tests-and-run-output.md` if
  closed by then, otherwise `issues/311-…` — phase-3 integration
  tests and JSONL format. Separate concern; its fixtures appear in
  the End-to-end section here for coverage tracking only.
- `issues/completed/316-simple-test-runner.md` — folded into this
  issue's "Test runners → Planned" section.
- `issues/231-move-signature-parsers-into-langs.md` — moved the
  parsers under test into `langs/<name>/parser.js`.
- `issues/217-concat-box-and-dynamic-inputs.md` — part C consumes
  the `variadic_tail` field these tests guard.
