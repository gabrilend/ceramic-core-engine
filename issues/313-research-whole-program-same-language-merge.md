# 313 — Research: whole-program same-language compilation

## Status
in progress — Lua slice landed 2026-05-21 (compile-time
concatenation in `soramech-compile.sh`, runtime detection in the
Lua spec, end-to-end test). C and Bash merges are follow-ons.

## Concept

Instead of one box = one function call going through the dispatch
layer + slot store + spec invocation, **merge all same-language
boxes at compile time into a single translation unit** for that
language. Box-to-box transitions within a language region become
ordinary function calls — the optimizer can inline across them, the
language runtime stays warm, and the slot store and dispatch layer
don't run for those transitions at all. Only cross-language wires
retain the slot machinery.

For C: every C box becomes a function in one big `.c` file, with
the build emitting one `.so`. Wire endpoints become direct calls;
the optimizer inlines based on its usual heuristics.

For Lua: every Lua function gets pre-compiled into one bytecode
chunk loaded into one shared `lua_State` per worker. Wire
endpoints become direct `lua_pcall`s into that chunk — no JSON
encode/decode, no slot push/pop, no spec re-entry per call.

For Bash: probably no benefit — Bash is already process-per-call;
fusing scripts doesn't help much. Skip.

## Relationship to 312 — what stays, what thins

312 defines the **wire format**: dual-ring slots (native ring,
JSON ring, ordering ring) per input port, per-edge classification
at graph load, JSON-only-at-borders, native-otherwise. Every
value movement between boxes goes through this machinery.

This issue's merge thins the **spec invocation overhead** — the
dlsym + bridge + lua_pcall (or equivalent) that happens once
per box invocation. For boxes in the merged region, the dispatch
holds a direct pointer to the function inside the merged module
and calls it without going through the spec interface.

The slot machinery stays alive in the merged region. Why:
**boxes still run as separate thread-pool tasks**. A producer
on worker 0 and a consumer on worker 1 still need slots to hand
off the value across threads. The thread pool's parallelism
depends on tasks being scheduled independently; collapsing
same-language hops to synchronous function calls would defeat
the pool entirely. The merge wins on the per-call indirection
cost, not on the wire format.

So a box invocation inside the merged region looks like:

1. Worker picks up the task.
2. Dispatch reads inputs from the box's slots (same as 312 —
   dual-ring native side).
3. Dispatch jumps to the merged function directly (this is
   what 313 wins).
4. Function runs, returns a value.
5. Dispatch writes output to consumer slots (same as 312).

Steps 2 and 5 are unchanged from 312. Step 3 is the savings.

## Why this is research, not a feature

Three open questions that need code-level investigation before
the design is concrete:

1. **Loop and iterator boundaries**: a same-language region with an
   iterator inside it has dynamic re-spawning. Is the iterator
   merged in (a loop in the generated code) or does it remain a
   spec-mediated boundary? If the latter, the merge buys less.
2. **Compile time**: whole-program optimization is slow. Does the
   incremental-compile model (issue 309's mtime check) still apply,
   or do edits require a full rebuild of the merged region?
3. **Debug story**: a stack trace inside the merged region needs to
   point back to a specific box. Either source-map-style mapping or
   per-function preserved names — both have costs.

The right way to answer these is to prototype: take a small all-Lua
graph (like the hello map) and see what a merged version looks
like, both for runtime cost and for build-system complexity. The
prototype should be able to fall back to the per-box path if the
merge fails for any reason.

## Why blocked until rest of phase 3 completes

The merge optimization piggybacks on the per-box model — same
specs, same compile callback, same dispatch layer for cross-language
wires. Building it before the per-box model is settled risks
designing against a target that's still moving. After phase 3 ships
and the per-box path is stable, the merge becomes a clean
"everything except spec invocation gets shorter for this region."

It's also genuinely lower priority than getting the runtime working
at all. The same-language fast path (issue 312) captures the
~2x JSON-skip win without any of this complexity. The merge wins a
further factor only if profile data shows function-call overhead
matters at all after 312 ships.

## Scope of the research output

The deliverable is a written design (not code) with:
- Concrete answers to the three questions above.
- A worked example: the merged C generated from a 5-box C-only
  graph, showing what the build artifact looks like.
- Estimated runtime cost of the per-box path vs the merged path
  on that example.
- A recommendation: ship as a feature, ship as an opt-in flag, or
  shelve indefinitely.

After the design is reviewed, a separate implementation issue
opens (or doesn't, depending on the recommendation).

## Relevant files

- `issues/312-same-language-wire-fast-path.md` — the immediate JSON-
  skip win this builds on
- `issues/307-c-language-spec.md` — `compile` callback that would
  grow whole-program-aware semantics
- `issues/306-lua-language-spec.md` — Lua-side bytecode pre-compile
- `issues/309-build-system.md` — how the merge integrates with the
  compile/package step

## Implementation log

### Lua slice — 2026-05-21

`scripts/soramech-compile.sh` grows a `merge_lua_sources` step
between source-copy and per-spec packaging. For every `.lua` file
in the map's `src/`, the merger wraps the file's body in a closure
and binds the closure's return value to `merged[basename]` of a
single output table. The result lands at `compiled/src/__merged__.lua`
alongside the original files (which stay untouched, so any caller
that hasn't been told about the merge still works).

`langs/lua/spec.c` grows `load_via_merged()`, called on the
cache-miss path of `lua_invoke`. The helper derives a candidate
`__merged__.lua` path next to the requested source file; if the
file exists, it loads the merged module once per worker (cached
under `LUA_MERGED_CACHE_KEY` in the Lua registry), then indexes
the returned table by the source file's basename to recover the
per-file module. Lookup misses (no merged file, or no matching
basename inside it) fall back to per-file lazy load — the
existing path stays as the safety net. The dispatch never knows
about the merge; the spec redirects transparently.

A unit test in `tests/306-lua-spec-test.c` exercises the merged
path on a synthetic temp directory: it writes a `__merged__.lua`
that exports a sub-module under basename `calc` and invokes a
phantom `calc.lua` file (no such file on disk). The invoke
succeeds, proving the merged path is reached. The existing
"compile pipeline (portable run)" integration test verifies the
end-to-end flow: `soramech-compile.sh` produces the merged file
when packaging the `pipeline` fixture, the compiled artifact
runs from `/tmp`, and the output matches what the source-tree
runner produces — the merge is transparent.

Deferred to follow-on slices:

- **C merge.** Concatenating C source needs care with static
  vs. non-static functions, file-scoped declarations, and header
  inclusion. Plausible approach: wrap each box's source in an
  anonymous namespace-equivalent (C doesn't have namespaces, but
  static-by-default at file scope + unique mangled exposure of
  the box's entry function would do it). Real work; not done.
- **Bash merge.** Bash already runs one persistent subprocess
  per worker (issue 308's UDS-server model); the "merge" would
  source every box's `.sh` file into the same subprocess
  environment at server startup. Smaller surface than C; sized
  somewhere in between.
- **Inline-into-dispatch optimization.** This slice loads the
  merged module per worker but still goes through the spec's
  `lua_pcall` for every box invocation. The big optimization the
  issue body talks about — turning box-to-box transitions into
  direct function calls inside the merged module — needs
  dispatch-layer work that hasn't started. The current slice is
  the foundation: load-once-per-worker. The direct-call
  optimization rides on top.

## Open questions (for the research, not for now)

- Cross-language wires inside the merged region: the merged region
  still has cross-language outputs that exit to other regions. The
  spec interface still applies at those boundaries, so the merge is
  region-internal optimization, not a global rearchitecture.
- C ABI for inlined wrappers: with whole-program optimization the
  wrapper-per-box generation might collapse to nothing. If the
  optimizer can prove a wire endpoint is always called from the
  merged region with known types, it can skip the wrapper.
- Iterators-with-side-effects: an iterator self-rescheduling its
  own task is normally dispatch-layer machinery. Inside a merged
  region, it could become a `while` loop. Whether that's safe
  depends on whether other tasks are blocked on the iterator's
  output slots — which they shouldn't be in a same-language region,
  but the analysis has to confirm.
