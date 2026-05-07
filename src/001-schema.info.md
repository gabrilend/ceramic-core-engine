# 001-schema.lua — Public API

Schema validators for all SoraMech map file types.
Returns error arrays (empty = valid). Used by runner and server before any write.

## M.validate_box(box: table) -> errors: string[]

Validates a parsed box file table. Checks id, label, kind, and kind-specific
fields. For "call" boxes: ref, optional fn, inputs/outputs arrays, connections.
For "branch" boxes: inputs, ports array (each port needs predicate except "else"),
"else" port must be present.

## M.validate_meta(meta: table) -> errors: string[]

Validates a parsed meta.json table. Requires: name (string), entry_box_id (string).
description is optional.

## M.validate_drivers(drivers: table) -> errors: string[]

Validates a parsed drivers.json table. Each key must be a dot-prefixed extension
string (e.g. ".lua"), each value must be a string path to a driver script.
