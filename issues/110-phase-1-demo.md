# 110 — Phase 1 demo

## Status

open

## Blockers

- 101 through 109 (all phase 1 issues must be complete)

## Current behavior

No demo exists. There is no way to show the system working end-to-end.

## Intended behavior

A demo map lives at maps/classify-demo/ that demonstrates every phase 1
capability in a single coherent run:

  Box 1 (call, lua): reads a name from a data file, returns a greeting
  Box 2 (call, bash): appends a timestamp to the greeting string
  Box 3 (branch): routes on string content — "hello" port or "else" port
  Box 4 (call, lua, reached via "hello"): writes result to a data file
  Box 5 (call, lua, reached via "else"): writes an error note to tmp/

The map reads from data/input.json at start and writes to data/output.json
at end. tmp/last-run.json is written by the runner on completion.

The demo is run with:
  luajit soramech-runner.lua maps/classify-demo/

Expected output: data/output.json contains the result, tmp/last-run.json
shows all box outputs and "ok" status, no errors.

A second part of the demo opens index.html, connects to the server, and
shows the classify-demo map on the canvas with all wires drawn correctly.
A screenshot or screen recording documents this.

## Suggested implementation steps

1. Write maps/classify-demo/ — all box files, data files, drivers.json,
   meta.json, and the source functions in maps/classify-demo/src/.
2. Write a src/classify-demo/hello.lua with hello(name) -> greeting.
3. Write a src/classify-demo/append-time.sh with a bash function that
   appends a timestamp.
4. Write a src/classify-demo/write-result.lua with write_result(text)
   that calls data.set to write to data/output.json.
5. Run the map, confirm output. Fix any issues found; create skeleton
   issue files for any new bugs discovered.
6. Start the server pointed at the maps/ directory. Open index.html.
   Confirm the classify-demo map renders with correct boxes and wires.
7. Document results in issues/phase-1-progress.md. Mark phase 1 complete
   if all checklist items pass.

## Demo lives in

  issues/completed/demos/phase-1-classify-demo/

After the demo is recorded or documented, move it there. The run script
lives in the project root as demo.sh:
  #!/usr/bin/env bash
  # Runs the phase 1 demo
  luajit soramech-runner.lua maps/classify-demo/

## Related documents

- issues/phase-1-progress.md — phase checklist updated here
- docs/002-roadmap.md — phase 1 goal definition

## Notes

The demo is part of the deliverable, not just a development artifact. It
should be kept working as later phases add functionality. If a later
change breaks the classify-demo map, that is a regression.

Any bugs found during the demo run should get skeleton issue files before
being fixed. Do not fix bugs discovered during demo without a written
issue first.
