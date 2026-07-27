# 218 — Enforce single-output driver contract throughout

## Status
completed

## Current behavior
The driver contract was originally written to support multi-return tuples: drivers
output a JSON array where each element is one return value. The executor unwraps
`result_arr[1]` and discards everything else. Three concrete artifacts of this:

- `drivers/lua.sh` shim: collects all return values into `results`, encodes each
  as a JSON string, then wraps in another JSON array (`json.encode(encoded)`).
  A single-return function produces `["\"hello\""]` — doubly-encoded.
- `maps/classify-demo/src/stamp.sh` and `maps/driver-test/src/utils.sh`: bash
  functions that explicitly output `['value']` — following the now-dead contract.
- `maps/driver-test/src/strops.c`: C binary that outputs `["VALUE"]`.
- `src/004-executor.lua`: parses a JSON array, checks it's a table, takes `[1]`.
- Docs (`drivers/README`, `docs/003-driver-system.md`): describe the array contract.

## Intended behavior
One output wire per box. Whatever the function returns is the single output value.
The driver contract is:
  - stdout: a single JSON value (string, number, boolean, object, or null)
  - exit 0 on success, non-zero on failure

No wrapping in an array. No `result_arr[1]`. The executor decodes stdout directly
as the output value.

## Why multi-output was ruled out
Multiple return statements with the same-width tuples are indistinguishable
positionally. The mapping from return position to output port name is ambiguous.
A single return value is always unambiguous. This was decided in the design phase
(see `issues/completed/108-branch-box-and-predicate-routing.md`).

## Suggested implementation steps
1. `drivers/lua.sh` shim — replace multi-return collection with `print(json.encode(results[1]))`.
2. `src/004-executor.lua` — decode stdout as a single JSON value; remove array unwrapping.
3. `maps/classify-demo/src/stamp.sh` — change printf to output `"value"` not `["value"]`.
4. `maps/driver-test/src/utils.sh` — same fix.
5. `maps/driver-test/src/strops.c` — change printf to output `"VALUE"` not `["VALUE"]`.
6. `drivers/README` — update contract description.
7. `docs/003-driver-system.md` — update contract description.
8. `docs/007-architecture.md` — note `outputs` field in box JSON is display metadata only.

## Implementation notes

All changes made and verified via `maps/driver-test` (lua + bash + C all pass).
Observed output: lua returns `8`, bash returns `"stringified: 8"`, C returns
`"STRINGIFIED: 8"` — all single JSON values, no array wrapping.

The `from_output` field still present in existing connection records is harmless —
the executor ignores it in non-comparator mode (fires all connections where
`from_branch == nil`). Cleanup of `from_output` from box JSON files is deferred
to a future cosmetic pass; it does not affect execution.

The `outputs` array in box JSON is retained as display metadata for the inspector.
It is not read by the executor.

## Relevant files
- `drivers/lua.sh` — LUASHIM section: results collection and encoding
- `src/004-executor.lua` — `run_task` function, lines 94-112
- `maps/classify-demo/src/stamp.sh` — outputs `['value']`
- `maps/driver-test/src/utils.sh` — outputs `['value']`
- `maps/driver-test/src/strops.c` — outputs `["VALUE"]`
- `drivers/README` — driver contract docs
- `docs/003-driver-system.md` — driver system spec
- `docs/007-architecture.md` — execution model description
