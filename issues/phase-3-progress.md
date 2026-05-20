# Phase 3 Progress

Goal: replace the Lua synchronous runner with a C thread pool runner
that executes box graphs in parallel.

The phase is fully designed; implementation is now underway.
Issues 301–313 are the architectural specification. As each issue
moves into implementation, its row is marked "in progress"; on
completion it's moved to `issues/completed/`.

Implementation began **2026-05-12** with the 309 build scaffolding.

## Issues

| ID  | Title                                                       | Status   |
|-----|-------------------------------------------------------------|----------|
| 301 | thread pool lifecycle and per-worker initialization         | complete · pool + per-worker init/teardown hooks + priority queue |
| 302 | per-task slot store with wire-held references               | in progress · core + large-value heap (variable-size payloads) |
| 303 | language runtime spec (pluggable per-language invocation)   | in progress · registry + per-worker init + pool hook + lang filtering |
| 304 | task dispatch layer (C, replaces synchronous executor)      | in progress · plain + comparator + iterator + randomizer + weighted routing + cell-tagged iterator ordering |
| 305 | C graph loader (replaces `003-loader.lua`)                  | in progress · attach + lang enum + 312 fast-path classification |
| 306 | Lua language spec implementation                            | in progress · init + invoke + teardown + per-worker module cache done |
| 307 | C language spec implementation                              | in progress · compile + invoke + lazy .c→.so done, typed wrapper deferred |
| 308 | Bash language spec implementation                           | in progress · persistent socketpair + line-protocol invoke + dladdr-based server lookup |
| 309 | build system & Makefile orchestration                       | in progress · scaffolding + spec discovery + soramech-compile portable artifact |
| 310 | priority queue: wired in, no-op behaviorally                | in progress · API + ordering done |
| 311 | integration tests & run output (`last-run.jsonl`)           | in progress · writer + queue + thread + LOG_VALUES + LOG_SLOTS + run-tests.sh + compile-pipeline check |
| 312 | same-language wire fast path (skip JSON for Lua→Lua etc.)   | in progress · lang_spec_t extended + classification + dispatch resolution |
| 313 | research: whole-program same-language merge                 | research / blocked |
| 314 | small C JSON parser written for this project                | in progress · parser + writer + strict leading-zero |
| 315 | reference-counted compiled-map artifacts                    | open · design only |
| 316 | simple test-runner script                                   | folded into 232 |
| 317 | language spec JSON bridge for data boxes                    | open · design only |

## Phase goal checklist

- [x] SoraMech-owned task pool builds clean (3d-rts as design reference)
- [x] Slot store unit-tested (single-value + ring buffer)
- [x] C graph loader passes the project's existing maps
- [x] Lua spec runs `tests/maps/hello` end-to-end
- [x] C spec compiles and runs a fixed-signature box
- [x] Bash spec runs an out-of-process box via persistent
      socketpair + line-protocol
- [x] Pool runner runs a multi-language map with concurrent workers
- [x] Iterator box routes via dispatch layer (with multi-spawn
      re-fire via N-cell pop slots and auto-re-spawn)
- [x] Comparator box routes via dispatch layer (via spec output)
- [x] Variable-size outputs work via the large-value heap
- [x] `last-run.jsonl` written for every run
- [ ] All 11 integration test maps pass
- [ ] Phase 3 demo map runs and produces expected output
- [ ] Phase 2 synchronous runner retired (`src/004-executor.lua`,
      `src/007-runner-main.lua`, `src/003-loader.lua`, `drivers/*.sh`)

## Implementation order suggestion

Designed to surface concrete artifacts early so subsequent issues can
build on them.

1. **309 (build system)** — top-level Makefile, vendored deps, empty
   stubs for runner and specs. Verify everything compiles.
2. **302 (slot store)** — pure C, unit-testable, no pool dependency.
3. **305 (graph loader)** — pure C, parses existing phase 2 maps.
4. **301 (pool lifecycle)** — wire pool startup, per-worker context,
   active-task counter, quiescence.
5. **303 (lang spec interface)** — header file + stub spec to validate
   the loading mechanism.
6. **306 (Lua spec)** — easiest reference. Smoke test: `maps/hello`
   end-to-end.
7. **304 (dispatch layer)** — connects everything. First fully-running
   box.
8. **310 (priority queue)** — small change, can land any time.
9. **307 (C spec)** — adds compile callback and wrapper generation.
10. **308 (Bash spec)** — adds out-of-process execution.
11. **311 (tests + run output)** — formalizes what "phase 3 works"
    means; runs continuously from this point.

Issue 222 (compile button placeholder) lands in phase 2 but its
backend is wired in here once 307 and the per-spec compile orchestration
exist.
