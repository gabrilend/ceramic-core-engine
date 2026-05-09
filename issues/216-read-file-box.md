# 216 — Read-file box: output contents of a text or data file

## Status
open

## Current behavior
No standard box exists for loading text from disk into the wire graph. A user who
wants to supply file contents as input to a downstream box must write a custom
Lua function from scratch.

## Intended behavior
A reusable source file (in `libs/` or a map's `src/`) that exposes two functions:
- `M.read_text(path)` — reads a plain text file; returns its full contents as a string.
- `M.read_data(path, key)` — reads a dkjson data file; returns the value at `key` as
  a string (strips metadata fields like `constant`). If `key` is nil/empty, returns
  the full JSON-decoded table encoded back to a plain string.

Both functions output a single port (`text`) carrying the file contents. Error cases
(file not found, bad JSON) write to stderr and return nil — the executor surfaces nil
as an empty string on downstream inputs.

The design follows the established pattern: a regular Lua file that the user picks
via the file browser and wires into their map. No executor changes needed.

## Suggested implementation steps
1. Create `libs/files.lua` with `M.read_text` and `M.read_data`.
2. Add `libs/files.lua.info.md` listing the two public functions, their inputs, and
   their single `text` output.

## Relevant files
- `libs/ollama.lua` — reference pattern for a library box source file
- `libs/dkjson.lua` — used by `read_data` for JSON parsing
- `src/004-executor.lua` — how call box outputs are threaded (no changes needed)
