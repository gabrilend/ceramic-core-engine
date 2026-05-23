# 319d — `create_box` and `connect` runtime built-ins, Lua bindings

## Status
open — planning stub. Full design in parent issue 319.

## Parent issue
Sub-issue of 319. Implements the headline feature for Lua only
(C and Bash bindings deferred to 319e to keep this slice
testable on its own).

## Intended behavior

- `soramech.create_box(spec_table) -> box_id_string` — instantiates
  a new box at runtime. `spec_table` shape matches box JSON schema.
  Returns the box's id string. Hard-crashes on failure (Q3).
- `soramech.connect(connection_table, ...)` — variadic, each arg
  is a connection-entry shape from the box JSON schema's
  `connections[]`. Hard-crashes on failure.

## Depends on

- 319a (input cap removed so new boxes can have arbitrary width)
- 319b (slot store growth so new boxes' slots can be allocated)
- 319c (id generation + compile cache)

## Suggested implementation

To be expanded when picked up.
