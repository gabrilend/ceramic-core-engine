# 313 — Research: whole-program same-language compilation

## Status
research / low priority / blocked until rest of phase 3 is complete

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
