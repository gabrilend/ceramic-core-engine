# Phase 1 Progress

Goal: a map you can edit in the browser and execute from the terminal.

## Issues

| ID  | Title                                    | Status    |
|-----|------------------------------------------|-----------|
| 101 | project scaffold and map directory format | complete |
| 102 | language driver interface                 | complete |
| 103 | runner: graph loader and validator        | complete |
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

The phase-1 demo maps (user content under `maps/`, gitignored) were
found gutted on disk long after the phase closed; they were restored
from git history but predate the unified routing schema.

| ID  | Title                                    | Status    |
|-----|------------------------------------------|-----------|
| [111](111-restore-phase-1-demo-maps-to-current-schema.md) | restore the phase-1 demo maps to the current schema | open · files recovered; schema migration remains; 003-data-test folded into the suite, 004-driver-test blocked on this |
