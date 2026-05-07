-- Writes the result text to data/state.json in the map directory.
-- Uses dkjson directly (soramech-data is implemented in issue 109).

local json = require("dkjson")
local M    = {}

-- {{{ write_result
function M.write_result(text)
    -- SORAMECH_MAP_DIR is injected by the lua driver shim
    local map_dir = SORAMECH_MAP_DIR or "."
    local path = map_dir .. "/data/state.json"

    local f_read = io.open(path, "r")
    if not f_read then error("write_result: cannot open " .. path) end
    local raw = f_read:read("*a")
    f_read:close()

    local obj, _, jerr = json.decode(raw)
    if not obj then error("write_result: JSON error in state.json: " .. tostring(jerr)) end

    obj.fields.result.value = text

    local tmp_path = path .. ".tmp"
    local f_write = io.open(tmp_path, "w")
    if not f_write then error("write_result: cannot write " .. tmp_path) end
    f_write:write(json.encode(obj, { indent = true }))
    f_write:close()
    os.rename(tmp_path, path)
end
-- }}}

return M
