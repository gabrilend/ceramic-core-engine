-- Hello function for the hello example map.
-- Reads the name from data/state.json and returns a greeting string.
-- No inputs: name comes from the map's data file at runtime.

local json = require("dkjson")
local M    = {}

-- {{{ read_name
local function read_name(map_dir)
    -- walk up from this file's location to find the map root's data/state.json
    local state_path = map_dir .. "/data/state.json"
    local f = io.open(state_path, "r")
    if not f then return "world" end
    local raw  = f:read("*a")
    f:close()
    local obj, _, _ = json.decode(raw)
    if not obj then return "world" end
    local name_field = obj.fields and obj.fields.name
    if not name_field then return "world" end
    return name_field.value or "world"
end
-- }}}

-- {{{ hello
function M.hello()
    -- SORAMECH_MAP_DIR is injected by the lua driver shim at invocation time
    local name = read_name(SORAMECH_MAP_DIR or ".")
    return "Hello, " .. tostring(name) .. "!"
end
-- }}}

return M
