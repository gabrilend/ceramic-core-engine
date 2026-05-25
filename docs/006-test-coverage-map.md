# SoraMech — Test coverage map

This document is the project-wide answer to "what's tested, what
isn't, and what runs the tests." Every time a test file lands or a
gap is closed, the relevant checklist row gets updated; when a new
module ships, a row goes in showing it has no coverage yet. Without
a single coverage view, gaps accumulate silently — tests get
written for whatever someone happens to be touching, and the rest
drifts uncovered.

## Legend

- `[x]` — has a dedicated test file (path in the row).
- `[ ]` — no dedicated test; may be exercised indirectly through
  integration fixtures, but not directly verified.
- `[~]` — partially covered; only some surface area is tested.
- `[—]` — intentionally untested (vendored library, thin
  entrypoint, etc.).

## Editor side — JavaScript

The editor is a browser app served by `src/005-http-server.lua`.
The Node-side test runner (Node's built-in `node:test`) was wired
up alongside the parser tests; other JS modules can follow the same
pattern when they get tests.

| Module | Coverage | Test file |
|---|---|---|
| `langs/lua/parser.js` | `[x]` | `tests/232-lua-parser-test.mjs` |
| `langs/bash/parser.js` | `[x]` | `tests/233-bash-parser-test.mjs` |
| `langs/c/parser.js` | `[—]` | not written yet; the move-parsers-into-langs work made it optional |
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

**Highest-value next targets:** the variadic-slot helpers in
`004-inspector.js` (pure functions, no DOM dependency) and the
lexer registries in each language directory.

## Lua libraries — `libs/*.lua`

| Module | Coverage | Test file |
|---|---|---|
| `libs/soramech-data.lua` | `[x]` | `tests/003-data-test.lua` |
| `libs/text.lua` | `[ ]` | — (concat, split, upper, lower, trim, replace, replace_first, contains, starts_with, ends_with, length, substring) |
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
| `src/016-unified-allocator.c` | `[x]` | `tests/016-unified-allocator-test.c` |
| `src/017-box-id.c` | `[x]` | `tests/017-box-id-test.c` |
| `src/018-runtime-builtins.c` | `[~]` | exercised through `tests/012-dispatch-test.c` and the runtime-create integration fixtures |
| `src/020-sentinels.c` | `[x]` | `tests/020-sentinels-test.c` |
| `libs/json/json.c` | `[x]` | `tests/314-json-test.c` |

## Language specs — `langs/<name>/spec.c`

| Spec | Coverage | Test file |
|---|---|---|
| `langs/lua/spec.c` | `[x]` | `tests/306-lua-spec-test.c`, `tests/317-lua-bridge-test.c` |
| `langs/c/spec.c` | `[x]` | `tests/307-c-spec-test.c` |
| `langs/bash/spec.c` | `[x]` | `tests/308-bash-spec-test.c`, `tests/317-text-bridge-test.c` |

## End-to-end fixtures — `tests/maps/`

Integration coverage that runs via `scripts/run-tests.sh`. Each
fixture is a real map driven through `soramech-pool`; the script
asserts expected output substrings appear in stderr.

| Fixture | What it exercises |
|---|---|
| `calc` | single Lua box |
| `hello` | data → Lua, string output |
| `comparator` | comparator branching (lt/eq/gt) |
| `iter-route` | iterator + routing |
| `pipeline` | five-box multilang chain (also covers the compile pipeline) |
| `read-literal` | inline-value source on a read box |
| `319a-many-inputs` | dispatch input cap removed; box with 20 inputs |
| `246-c-shim` | per-port custom translation shim (C side) |
| `246-lua-shim` | per-port custom translation shim (Lua side) |
| `318-lang-opaque` | $lang_opaque sentinel emit + dual-ring consumer |
| `248-encap-input-only` | encapsulated map: input-side splice |
| `248-encap-output-only` | encapsulated map: output-side splice |
| `248-encap-recursive` | encapsulated map: two-level nested encapsulation |
| `randomizer-route` | routing kind: randomizer (hash-of-counter pick) |
| `weighted-route` | routing kind: weighted (cumulative-band lookup) |
| `distributor-route` | routing kind: distributor (argmin over downstream fill) |
| `multi-band-comparator-route` | routing kind: multi-band comparator with thresholds |
| `pipeline (compile)` | the compile pipeline runs the artifact from `/tmp` |

The reference-counted-artifact (`315-refs-*`) checks plus the
JSON-Lines run-output writer also fold into the same suite via
`scripts/run-tests.sh`.

## Test runners

- **`scripts/run-tests.sh`** — integration fixtures + the JS parser
  tests + the 315 reference-helper unit suite + the
  compile-pipeline portable-run check. The single entry point for
  "run everything the build doesn't run." Folds Node-test results
  into the same pass/fail counter as the integration checks.
- **`make test`** — builds the C unit-test binaries, runs them,
  then invokes `scripts/run-tests.sh`. Slow on a clean tree because
  it does a full rebuild + spec build before any test runs.

- **`scripts/run-unit-tests.sh`** (issue 252) — cheap, build-free
  runner for the C unit-test binaries. Runs whatever's already
  built under `build/tests/`, prints one row per binary plus a
  total. `--quiet` collapses to just the total; `--verbose`
  dumps each binary's full output; a positional argument filters
  to binaries whose names start with that prefix. `make
  quicktest` is the alias; `make quicktest ARGS=009` filters.

## Suggested next test additions (ranked)

1. **`libs/text.lua`** — pure-Lua tests next to
   `tests/003-data-test.lua`. The expanded string surface left a
   wide untested area and consumers are about to multiply.
2. **`src/001-schema.lua`** — accept/reject tests for box JSON.
   Cheap, and the schema gates everything the executor sees.
3. **Inspector variadic helpers** (`assets/js/004-inspector.js`) —
   `parse_variadic_name`, `is_variadic_slot`, `variadic_slots_for`,
   `last_variadic_index`, `make_variadic`, `unmake_variadic`,
   `add_variadic_slot`, `remove_variadic_slot`. All pure, all
   feeding into the variadic-port work.
4. **Lexer round-trips** for the three shipped languages — feed a
   known source through `tokenize`, check the token stream.
5. **`langs/c/parser.js`** — write it and its tests together when
   a C parser commits to a shape.

## Related documents

- `docs/001-architecture.md` — components and data flow.
- `docs/004-ipc-and-threading.md` — pool runner threading model.
- `docs/005-language-specs.md` — language-spec contract.
- `issues/phase-2-progress.md`, `issues/phase-3-progress.md` —
  per-phase issue index, including the issues whose
  acceptance-test fixtures appear in the End-to-end section above.
