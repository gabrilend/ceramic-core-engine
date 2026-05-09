# files.lua

File-reading helpers — load text or soramech-data values from disk
into the wire graph. Pure Lua except for `read_data`, which depends
on dkjson (already vendored). Use as `require("files")` from another
Lua function, or copy into a map's `src/` and pick a function via
the file browser.

Both functions emit a single string on the box's output wire (per the
single-output rule, issue 218). On failure, the function writes to
stderr and returns `nil`; the executor surfaces nil as an empty
string downstream.

---

## M.read_text(path) → string | nil

Reads a plain text file and returns its full contents as a string.

| param | type   | description                  |
|-------|--------|------------------------------|
| path  | string | path to the text file        |

Returns: the file contents as a string, or `nil` if the file can't
be opened (the error is written to stderr).

Use cases: loading prompts, templates, configuration snippets, or
arbitrary text-shaped inputs into a downstream box.

---

## M.read_data(path, key) → string | nil

Reads a soramech-data JSON file and returns either a single field's
value or the whole file re-encoded as JSON.

| param | type   | description                                         |
|-------|--------|-----------------------------------------------------|
| path  | string | path to the JSON data file                          |
| key   | string | field name to extract; nil/empty returns whole file |

Returns: the value at `key` (as a string), or — when `key` is nil or
empty — the entire decoded object re-encoded as a JSON string. `nil`
on failure (file missing, JSON parse error, key not found), with the
error on stderr.

Strips metadata: when the field has a wrapping record with a
`.value` member (the soramech-data shape: `{ value, constant, ... }`),
only the `value` is returned. Bare values without a wrapping record
are returned as-is. Falls back to a flat top-level lookup
(`obj[key]`) when the file isn't in the soramech-data nested shape.

Edge cases:
- File missing or unreadable → nil + stderr message
- JSON decode failure → nil + stderr message (the parser error is
  included in the message)
- Whole-file mode re-encodes with indentation, matching how
  soramech-data writes its files to disk
