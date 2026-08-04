# Phase 1 Progress

Goal: a map you can edit in the browser and execute from the terminal.

## Issues

| ID  | Title                                    | Status    |
|-----|------------------------------------------|-----------|
| 101 | project scaffold and map directory format | complete |
| 102 | language driver interface                 | complete |
| [103](103-runner-graph-loader-and-validator.md) | runner: graph loader and validator | **reopened** · loader current; the standalone map validator never converted to unified routing and nothing invokes it |
| 104 | runner: synchronous executor              | complete |
| 105 | HTTP server: file CRUD API                | complete |
| 106 | web editor: canvas and box rendering      | complete |
| 107 | web editor: wiring and connection UI      | complete |
| 108 | branch box and predicate routing          | complete |
| 109 | data files: persistent and ephemeral      | complete |
| 110 | phase 1 demo                              | complete |

## Phase goal checklist

- [x] Create a box in the browser and save it to disk
- [x] Wire two boxes together in the browser
- [x] Run a 3-box map from the CLI and see outputs written to disk
- [x] Write a custom driver and have the runner use it
- [x] Branch box routes correctly based on a predicate
- [x] Data file read and write from within a box function

## Post-phase maintenance

Two phase-1 artifacts turned out to have rotted quietly after the
phase closed, both for the same reason: nothing in the test suite
reaches them. A survey on 2026-07-25 established the extent.

| ID  | Title                                    | Status    |
|-----|------------------------------------------|-----------|
| [103](103-runner-graph-loader-and-validator.md) | runner: graph loader and validator | reopened · the loader is current; the standalone validator still speaks the pre-unified-routing dialect, accepts 0 of the 27 tracked map fixtures, and crashes on a current-format map. No build step invokes it |
| 113 | restore the phase-1 demo maps to the current schema | open · four maps, not three: one to migrate, one to rebuild from its test, two to rule on. The earlier recovery was partial and the "hello serves the smoke test" note was a name collision with a tracked fixture. **Renumbered from 111 on 2026-08-03** — phase 1 had two issues at that number |

Both point at the same fix and it is neither a migration nor a
rewrite: **the maps under `maps/` and the validator must end up
inside the same sweep that `make test` already runs over
`tests/maps/`.** Everything the suite touches stayed current
through two months of format changes. Everything it doesn't touch
drifted. That is the whole difference between the healthy and the
rotted halves of phase 1.

A survey on 2026-08-02 asked how far that pattern reaches beyond
phase 1, and the answer is: through the whole record. Fourteen
places where two parts of the project disagree — a completed
issue against the code it produced, a document against the
runtime it describes, a house rule against the corpus meant to
obey it, and one issue number used twice.

| ID  | Title                                    | Status    |
|-----|------------------------------------------|-----------|
| 112 | record consistency: the contradiction sweep and the checker that prevents the next one | open · fourteen findings, none resolved · six mechanically detectable, eight need a ruling |

It lives in phase 1 because the promise it defends is
foundational: that re-completing the issues in `completed/`
rebuilds the project. Six of the fourteen findings become a
script that `make test` runs, which is the same medicine 103 and
the demo-map restoration prescribe — put the thing inside the
sweep, and it stops rotting.

Its row above deliberately carries no path link, per the house
rule against linking between issue files by path. The links
elsewhere in this document are finding 7 of that survey.
