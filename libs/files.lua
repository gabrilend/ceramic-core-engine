-- File reading — shared library and ready-to-use box source.
-- require("files") from any Lua function, or copy to a map's src/ to use
-- it directly as a call box via the file browser. Both helpers emit a
-- single string on the box's output wire (per the single-output rule,
-- issue 218); errors go to stderr and the function returns nil so the
-- executor surfaces an empty string downstream.
--
-- M.read_data depends on dkjson (already vendored under libs/ for the
-- runtime's package.path).

local json = require("dkjson")
local M    = {}

-- {{{ M.read_text
-- Reads a plain text file and returns its full contents as a string.
-- Returns nil on any failure (file not found, read error). The error
-- is written to stderr so the user sees it in the runner output.
function M.read_text(path)
    local f, ferr = io.open(path, "r")
    if not f then
        io.stderr:write("read_text: cannot open " .. tostring(path) ..
                        ": " .. tostring(ferr) .. "\n")
        return nil
    end
    local content = f:read("*a")
    f:close()
    return content
end
-- }}}

-- {{{ M.read_data
-- Reads a dkjson data file (the soramech-data format with `value` /
-- `constant` fields) and returns the value at `key` as a string. If
-- `key` is nil or empty, the full decoded JSON object is re-encoded
-- and returned as a plain string — useful for piping a raw data file
-- into a box that expects JSON text.
--
-- Strips metadata: when a key's record has a `.value` field, that
-- value is returned (and only that value). Bare values without a
-- wrapping record are returned as-is.
function M.read_data(path, key)
    local raw = M.read_text(path)
    if not raw then return nil end

    local obj, _, jerr = json.decode(raw)
    if not obj then
        io.stderr:write("read_data: JSON decode failed for " ..
                        tostring(path) .. ": " .. tostring(jerr) .. "\n")
        return nil
    end

    if key == nil or key == "" then
        -- Whole-file mode: re-encode the parsed object so callers
        -- receive a normalized JSON string regardless of the input
        -- formatting. Uses indented form to match how soramech-data
        -- writes its files on disk.
        return json.encode(obj, { indent = true })
    end

    -- Soramech-data nests fields under `obj.fields[key]`; older
    -- ad-hoc data files use a flat shape. Try the nested shape first,
    -- fall back to the flat shape if no match.
    local rec = (obj.fields and obj.fields[key]) or obj[key]
    if rec == nil then
        io.stderr:write("read_data: key '" .. tostring(key) ..
                        "' not found in " .. tostring(path) .. "\n")
        return nil
    end

    -- Strip metadata: if the record is itself an object with `.value`,
    -- that's the user-facing value. Otherwise the bare record is the
    -- value.
    if type(rec) == "table" and rec.value ~= nil then
        return tostring(rec.value)
    end
    return tostring(rec)
end
-- }}}

return M
