# 207 — Source file browser and port auto-population

## Status

complete

## Implementation notes

Server: added `handle_list_src` (GET /maps/:name/src) and `handle_get_src`
(GET /maps/:name/src/:file) in src/005-http-server.lua.

API: added `list_src_files()` and `get_src_file(filename)` in 003-api.js.

Parser (007-filebrowser.js): Lua matches `function M.name(args)` for inputs;
scans function body for `return` statements and uses bare identifier names as
output port names (falls back to "result" for single complex expressions).
Bash matches `name() {` and scans for `local var="${N}"` to name positional
inputs; outputs always ["output"].

Inspector (004-inspector.js): ref field is now a read-only display plus a
Browse button that replaces the inspector with the file browser. On function
click, ref/fn/inputs/outputs are set on the box and saved. Inputs and outputs
are shown as read-only port-display lists, not editable fields. Branch box
inputs remain manually editable (no source function to parse).

## Blockers

- 201 (boxes must render for this to be testable)

## Current behavior

The inspector shows editable text inputs for ref, fn, inputs, and outputs.
Users must type port names by hand with no relationship to the actual function
being called. Mismatches between the box config and the function signature
cause silent runtime failures.

## Intended behavior

For call boxes:
- "ref" field shows current value plus a Browse button
- Clicking Browse replaces the inspector panel with a file list from the
  map's src/ directory
- Selecting a file shows all public functions parsed from that file, each
  with its inputs (from the function signature) and outputs (from return
  statements), displayed as read-only text
- Clicking a function sets ref and fn on the box, derives inputs and outputs
  from the parsed signature, saves the box, and returns to the normal
  inspector view
- Inputs and outputs in the inspector are read-only — not editable fields

For branch boxes: no file browser; ports remain manually configured.

## Suggested implementation steps

1. Server: add GET /maps/:name/src (list files in src/) and
   GET /maps/:name/src/:file (return file content as text/plain)
2. API: add list_src_files() and get_src_file(filename)
3. New file 007-filebrowser.js: parse_functions(content, filename),
   render(container, on_select) that shows file list → function list
4. Inspector: replace ref text input with display + Browse button;
   remove editable inputs/outputs; show derived ports as read-only list
5. When function selected: set box.ref, box.fn, box.inputs, box.outputs
   (with type: "any" for all), autosave

## Parser conventions

Lua (.lua): match `function M.name(args)` for inputs; scan function body
for `return` statements and use variable names as output names. If the
return expression is not a bare identifier, name it "result".

Bash (.sh): match `name() {`; scan body for `local var="${N}"` to name
positional inputs; output is always one value named "output".

Other extensions: show file in list but display "no parser" when selected.

## Related documents

- docs/001-architecture.md — box file format (ref, fn, inputs, outputs)
- assets/js/004-inspector.js — inspector to modify
- src/005-http-server.lua — server to extend
