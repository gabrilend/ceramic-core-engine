# 002-validate-map.lua — Public API

CLI validator for a SoraMech map directory. Not a module — run directly.

## Usage

    luajit src/002-validate-map.lua <map-dir>

Exits 0 and prints "OK" if the map is valid.
Exits 1 and prints errors to stderr if not.

## What it checks

- meta.json: presence, schema validity, entry_box_id points to a real box
- drivers.json: presence, schema validity
- boxes/: every .json file parses and passes schema validation
- connections: every connection's to_box refers to an existing box id;
  the target box has a reciprocal connection entry referencing the source
- branch boxes: "else" port exists; unwired "else" emits a warning
  (runner enforces retry_vary requirement at execute time)
