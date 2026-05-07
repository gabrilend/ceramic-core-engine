# 111 — Bash driver output format and driver-test silent skip

## Status

complete

## Implementation notes

Entry box fix: `lua-box.json` inputs cleared; `math-funcs.lua` `add()` now
reads `a` and `b` from `data/state.json` via `SORAMECH_MAP_DIR` (no wired
inputs). `data/state.json` pre-seeded with `a=3, b=5`.

Bash output fix: `utils.sh` stringify now outputs `printf '["%s"]\n'` — a
JSON array with one plain-string element. The executor's `json.decode(raw)`
fails gracefully on a plain string (not valid JSON), so `outputs["text"]`
receives the raw string directly. Same pattern the C box already used.

Regression test: `tests/004-driver-test-runner.lua` runs the map end-to-end
and asserts all three boxes ran with the correct output chain:
  lua-box sum=8 → bash-box text="stringified: 8" → c-box result="STRINGIFIED: 8"

## Current behavior

Two related problems found while building the phase-1 demo:

1. **driver-test silent skip**: `maps/driver-test/` meta.json sets `entry_box_id`
   to `lua-box`, which declares two inputs (`a`, `b`). The executor starts with
   the entry box in the queue but immediately skips it because `inputs_satisfied`
   returns false. No boxes run. The runner reports "OK" and writes
   `{"ok":true,"boxes":[]}` to last-run.json. The map appears to pass but does
   nothing.

2. **bash function output format**: The existing `stringify` function in
   `maps/driver-test/src/utils.sh` outputs `"42"` (a bare JSON string). The
   executor expects a JSON array `["..."]` where each element is itself a
   JSON-encoded value. Passing a bare JSON string fails the type check
   (`type(result_arr) ~= "table"`) — but this was never hit because no boxes ran.
   The bash driver contract (function must output a JSON array) was not enforced
   or tested.

## Intended behavior

1. driver-test entry box should have no inputs (reads inputs from a data file via
   `SORAMECH_MAP_DIR`), so it fires immediately. All three driver boxes
   (lua → bash → C) should run and produce verifiable output.

2. Bash functions must output a JSON array matching the same format as the lua
   driver: `["<json-encoded-result>"]` for a single return value. The bash driver
   comment and driver spec in docs/003-driver-system.md should state this
   explicitly. A test in tests/ should verify a bash box end-to-end.

## Suggested implementation steps

1. Redesign driver-test: change lua-box to read its inputs from a data file
   (no wired inputs), so it is the true entry with no dependencies.
2. Update `utils.sh` stringify to output `["\"<result>\""]` format.
3. Run driver-test end-to-end and confirm all three boxes appear in last-run.json.
4. Update docs/003-driver-system.md to document the required JSON array format
   for all driver outputs.
5. Add a test (tests/004-bash-driver-test.lua or a bash script) that invokes
   drivers/bash.sh directly and asserts the output format.

## Related documents

- docs/003-driver-system.md — driver output format spec
- maps/driver-test/ — the broken map
- issues/102 — original driver interface design

## Notes

The classify-demo (issue 110) was built using a correct bash function that
outputs `["..."]` from the start, so it works. driver-test is the only broken
case. Fix driver-test after classify-demo demo is documented.
