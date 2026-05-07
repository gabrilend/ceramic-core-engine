# soramech-data.lua

Data file access library for SoraMech map boxes. Works with any directory
of JSON data files — `data/` for persistent storage, `tmp/` for ephemeral.

All JSON files use the wrapper format:
```json
{
  "constant": false,
  "fields": {
    "key": { "constant": false, "value": <any> }
  }
}
```

---

## M.load(dir, filename) → table, err

Loads and decodes the entire file. Returns the raw decoded table (with
`constant` and `fields` wrappers intact). Useful when you need to inspect
the structure or iterate all fields.

| param    | type   | description                                  |
|----------|--------|----------------------------------------------|
| dir      | string | directory path (e.g. `map_dir .. "/data"`)   |
| filename | string | file name, e.g. `"state.json"` (no slashes)  |

Returns: `table, nil` on success; `nil, err_string` on failure.

---

## M.get(dir, filename, field_path) → value, err

Reads a single field by dot-separated path. Unwraps the `{constant, value}`
wrapper automatically — you get the plain value back.

| param      | type   | description                              |
|------------|--------|------------------------------------------|
| dir        | string | directory path                           |
| filename   | string | file name                                |
| field_path | string | dot-separated key, e.g. `"config.model"` |

Returns: `value, nil` on success; `nil, err_string` if file missing, JSON
invalid, or field not found.

---

## M.set(dir, filename, field_path, value) → ok, err

Writes a single field atomically. Refuses writes to any field (or entire
file) whose `constant` flag is true.

| param      | type   | description                                     |
|------------|--------|-------------------------------------------------|
| dir        | string | directory path                                  |
| filename   | string | file name                                       |
| field_path | string | dot-separated key                               |
| value      | any    | new value (must be JSON-encodable)              |

Returns: `true, nil` on success; `nil, err_string` if file unreadable,
constant violation, or write failure.

**Error conditions:**
- File does not exist → error
- JSON parse failure → error
- `constant: true` on file or targeted field → error (value unchanged)
- Intermediate key in path does not exist → error
- Rename fails after write → error

---

## Notes

- `field_path` supports only string keys separated by `.` — no array indexing.
- `filename` has path separators stripped; only files in `dir` are reachable.
- Writes go to `<path>.tmp` first, then `os.rename` to the real path —
  atomic on POSIX, so readers always see a complete file.
