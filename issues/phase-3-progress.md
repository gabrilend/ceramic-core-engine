# Phase 3 Progress

Goal: replace the Lua synchronous runner with a C thread pool runner
that executes box graphs in parallel.

The phase is fully designed; implementation has not yet started.
Issues 301–311 are the architectural specification. When
implementation begins, mark each issue as in progress, then complete,
and update this file.

## Issues

| ID  | Title                                                       | Status   |
|-----|-------------------------------------------------------------|----------|
| 301 | thread pool lifecycle and per-worker initialization         | designed |
| 302 | per-task slot store with wire-held references               | designed |
| 303 | language runtime spec (pluggable per-language invocation)   | designed |
| 304 | task dispatch layer (C, replaces synchronous executor)      | designed |
| 305 | C graph loader (replaces `003-loader.lua`)                  | designed |
| 306 | Lua language spec implementation                            | designed |
| 307 | C language spec implementation                              | designed |
| 308 | Bash language spec implementation                           | designed |
| 309 | build system & Makefile orchestration                       | designed |
| 310 | priority queue: wired in, no-op behaviorally                | designed |
| 311 | integration tests & run output (`last-run.jsonl`)           | designed |
| 312 | same-language wire fast path (skip JSON for Lua→Lua etc.)   | designed |
| 313 | research: whole-program same-language merge                 | research / blocked |

## Phase goal checklist

- [ ] SoraMech-owned task pool builds clean (3d-rts as design reference)
- [ ] Slot store unit-tested (single-value + ring buffer + refcounting)
- [ ] C graph loader passes phase 2's existing maps
- [ ] Lua spec runs `maps/hello` end-to-end
- [ ] C spec compiles and runs a typed-signature box
- [ ] Bash spec runs an out-of-process box via the socket protocol
- [ ] Pool runner runs a multi-language map with concurrent workers
- [ ] Iterator box routes via dispatch layer (no spec invoked)
- [ ] Comparator box routes via dispatch layer (no spec invoked)
- [ ] Variable-size outputs work via the large-value heap
- [ ] `last-run.jsonl` written for every run
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
