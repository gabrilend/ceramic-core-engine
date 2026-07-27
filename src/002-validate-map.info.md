# 002-validate-map.lua — Public API

> **This validator is stranded and the rules below are the retired
> dialect.** It was written against the pre-233 schema and never
> converted: it matches wires on `from_output`, branches on a
> `kind` of `"branch"`, and requires `drivers.json` — none of
> which the project still has. Nothing invokes it (not the
> Makefile, not the test scripts, not `run`), which is why the
> drift went unnoticed. Measured against the maps the runtime
> actually runs, it accepts none of the fixtures under
> `tests/maps/`, and on a current-dialect map it dies with a Lua
> traceback rather than a report.
>
> Issue 103 (runner graph loader and validator) is reopened to fix
> it. Until then, the authority on what a valid map looks like is
> `docs/002-map-model.md`, and the check that actually runs is the
> C loader's — `src/010-graph-loader.c`.

CLI validator for a SoraMech map directory. Not a module — run directly.

## Usage

    luajit src/002-validate-map.lua <map-dir>

Exits 0 and prints "OK" if the map is valid.
Exits 1 and prints errors to stderr if not.

## What it checks (retired dialect — see the warning above)

- meta.json: presence, schema validity, entry_box_id points to a
  real box. *`entry_box_id` is being removed entirely — issue 206.*
- drivers.json: presence, schema validity. *A Lua-side artifact
  only; the C pool runner never reads it, resolving languages
  through `langs/` instead. Requiring it is why every current
  fixture fails.*
- boxes/: every .json file parses and passes schema validation
- connections: every connection's to_box refers to an existing box
  id; the target box has a reciprocal connection entry referencing
  the source. *Wires are matched on `from_output`, a field the
  schema no longer defines.*
- branch boxes: "else" port exists; unwired "else" emits a warning.
  *There is no `branch` box kind. Issue 233 dissolved it into the
  `routing` declaration; the equivalent today is comparator
  routing.*
