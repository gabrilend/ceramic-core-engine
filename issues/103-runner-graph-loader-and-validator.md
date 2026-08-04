# 103 — Runner: graph loader and validator

## Status

reopened 2026-07-25 — the loader half shipped and is current. The
standalone validator half shipped, then rotted: it is the last file
in the project still speaking the pre-unified-routing vocabulary,
and nothing invokes it, so nothing noticed. Reopened because the
validator is the tool a map author reaches for first and it cannot
read the maps the runtime actually runs.

## Blockers

- 101 (map format must be defined before loading can be implemented)
- 102 (driver list loaded here; drivers must exist to validate references)

## Current behavior

**The loader is current and healthy.** `src/003-loader.lua` loads a
map directory into the graph table, cross-checks that both ends of
every wire store the same record, and permits cycles that pass
through a routing box. It speaks the unified-routing vocabulary
(`from_branch`). The C runtime has its own independent loader
(`src/010-graph-loader.c`) which also speaks it.

**The standalone validator is stranded.** `src/002-validate-map.lua`
was written against the original schema and was not converted when
issue 233 unified routing — 233's status list names the six places
that were converted, and the validator is not among them. Concretely:

- It matches wires on `from_output`, a field the schema no longer
  defines. It is the only file left in `src/`, `assets/`, `libs/`,
  or `langs/` that mentions `from_output` at all.
- It still branches on a `kind` of `"branch"`, a box kind that 233
  dissolved into the routing declaration. Its companion doc
  (`002-validate-map.info.md`) still documents that kind, so a
  reader who trusts the docs learns the retired dialect.
- It dereferences `from_output` unconditionally when reporting a
  mismatch, so on any map written in the current dialect it dies
  with `attempt to concatenate field 'from_output' (a nil value)`
  and a Lua stack traceback instead of a report. Confirmed against
  narrative-engine's map, which is an outside project's
  current-format map — a downstream author's first contact with
  SoraMech validation is a crash.

**Nothing runs it.** Not `Makefile`, not `scripts/run-tests.sh`, not
`scripts/run-unit-tests.sh`, not `run`, not `demo.sh`. Its only
callers are two comments inside `src/001-schema.lua` that name it as
the authority for graph-level input-binding rules the schema checker
deliberately does not handle. So the project documents a validation
authority that no code path consults. It drifted for exactly as long
as it went unexercised, which is every commit since 233.

**Measured against the maps the runtime actually runs:** it accepts
**0 of the 27** map fixtures under `tests/maps/`. Every one of the 27
is rejected for having no `drivers.json`, and that requirement is
itself stale — `drivers.json` is a Lua-side artifact only. The C
pool runner never reads it; it resolves languages through the
per-language specs under `langs/`. The remaining rejections are
mostly a required per-box `label`, which the runtime does not need,
and a required `entry_box_id`, which the pool runner infers for
itself (see issue 419's entry-point ruling).

So there are two disagreeing definitions of "a valid map" — the one
the runtime enforces by refusing to run, and the one the validator
enforces by refusing to approve — and the second one has been wrong
for two months without a single test going red.

## Intended behavior

soramech-runner.lua, when given a map directory, performs a load and
validate phase before any execution begins:

  1. Reads meta.json
  2. Reads all files in boxes/ — parses each as a box table
  3. Resolves how each box's language is dispatched
  4. Validates:
     a. Every connection's to_box references an existing box id
     b. Every connection is reciprocally written in the target box's file
        (both ends agree) — disagreement is a hard error with a message
        naming both files and the conflicting field
     c. Every ref's file extension can be dispatched
     d. Routing boxes: every declared branch a wire names exists on the
        producer's routing declaration; the fallback branch exists; if
        the map has any LLM-type boxes feeding the routing decision, the
        fallback is permitted to be unwired (retry semantics); otherwise
        it must be wired or the runner refuses to start
     e. No cycles that don't pass through a routing decision (direct
        loops are an error; loops through a routing box are allowed —
        they are the retry pattern)
  5. Builds an in-memory graph: table of box tables, adjacency lists
     keyed by box id

The runner prints a structured error report and exits non-zero if any
validation fails. No fallbacks.

Three constraints the original blueprint left implicit, and whose
absence is what let the validator rot:

- **The validator's rule set and the runtime's rule set are the same
  rule set.** A map the runtime runs must validate; a map the
  validator rejects must not run. Where they can't share code they
  must at least share a test corpus.
- **The validator runs in the build.** It is pointed at every fixture
  under `tests/maps/` on every `make test`. An unexercised checker is
  a document wearing a program's clothes.
- **Vocabulary is single-sourced.** Wire field names, box kinds, and
  routing kind names are read from the schema module rather than
  spelled out a second time in the validator, so a redesign of the
  surface cannot leave one reader behind.

## Suggested implementation steps

The loader steps are done (see implementation notes). What remains is
the validator, and the order matters — wire it into the build *first*,
so every subsequent step is driven by a failing test rather than by
reading code.

1. **Point the build at it.** Add a step to `scripts/run-tests.sh`
   that runs the validator over every directory under `tests/maps/`.
   It will report 27 failures. That number is the work list, and it
   only ever goes down.
2. **Settle what a map must carry**, because the two rule sets
   disagree on three fields and each needs a ruling, not a patch:
   - `drivers.json` — Lua-side only; the pool runner resolves
     languages through `langs/` specs and never opens it. Either the
     validator stops requiring it, or map authors keep writing a file
     that only the interpreter and the editor read. Recommend: stop
     requiring it, require it only when validating for the
     interpreter path.
   - per-box `label` — the editor wants one; the runtime doesn't
     care; fixtures don't have one. Recommend: warn, don't reject.
   - `entry_box_id` — resolved by issue 419's ruling that a map has
     no single entry. Stop requiring it.
3. **Convert the vocabulary.** Replace `from_output` matching with
   `from_branch`, and replace the `kind == "branch"` fallback-port
   rules with the equivalent check against the producer's routing
   declaration. Read the field names and kind names from
   `src/001-schema.lua` rather than re-spelling them, so the next
   surface redesign can't strand this file again.
4. **Fix the crash before the rules.** The mismatch-report path
   concatenates the wire's source field unconditionally; a nil there
   must produce a report, never a traceback. A validator that dies on
   invalid input is worse than no validator, because the author can't
   tell a broken map from a broken tool.
5. **Correct the companion doc** (`002-validate-map.info.md`) — it
   still documents the retired branch box kind and the `drivers.json`
   requirement.
6. **Add the crash as a fixture.** A map whose wire names a branch
   that doesn't exist, asserted to produce a report and exit 1.
7. Then the two remaining consumers: fold the four maps under `maps/`
   into the same sweep once issue 113 (restore the phase-1 demo
   maps) has migrated them, and decide
   whether the two comments in `src/001-schema.lua` that name this
   file as the graph-level authority are still true.

## Implementation notes

`src/003-loader.lua` implements `load_map(map_dir)` and `validate_graph(graph)`. It handles JSON loading of all box files, reciprocal connection cross-checking (both ends must agree — hard error if not), and cycle detection that permits loops through branch boxes. The graph structure is a Lua table with boxes keyed by id, an entry id, and a driver table keyed by file extension.

The standalone validator `src/002-validate-map.lua` shipped alongside
it and is the part still outstanding — see current behavior.

## Related documents

- docs/002-map-model.md — box kinds, routing kinds, wire semantics
  (was cited here as docs/001-architecture.md, a path that no longer
  exists; the documentation was renumbered)
- issues/101 — map format and example map
- issues/104 — executor calls into the graph loaded here
- issues/completed/233 — the routing redesign this file was not
  converted by; its status list is the record of what *was* converted
- issue 113 (restore the phase-1 demo maps) — the four maps under
  `maps/`, blocked on the same dialect
- issues/419 — the entry-point ruling that removes one of the three
  disagreements above

## Notes

The reciprocal connection check (both box files must agree) is the most
important invariant to enforce. If this check is ever skipped or weakened,
the server's partial writes (e.g., browser tab closed mid-edit) will
produce silently corrupt maps. Always hard-error here.

Cycle detection through branch boxes: the retry pattern (else wired back
to an upstream box) forms a cycle. This is valid. The detector must not
reject it. Marking branch boxes as "cycle-break" nodes in the DFS is
sufficient.

The lesson this file is the evidence for: the five readers of the map
format that converted during the routing redesign converted because
something broke when they didn't — the editor stopped drawing, the
runner stopped loading, a test went red. This one had nothing to
break, so it kept its old dialect while the format moved out from
under it. Correctness that nothing checks is a claim, not a property.
That is why step 1 is "wire it into the build" and not "convert the
vocabulary" — converting it by hand today without a test would leave
it in exactly the same position for the next redesign.
