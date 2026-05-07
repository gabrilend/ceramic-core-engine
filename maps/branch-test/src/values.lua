-- Value producer and result writer for branch-test map.
local json = require("dkjson")
local M    = {}

-- {{{ get_value
-- Reads "n" from data/state.json and returns it as a number.
function M.get_value()
    local path = (SORAMECH_MAP_DIR or ".") .. "/data/state.json"
    local f    = io.open(path, "r")
    if not f then return 42 end
    local raw  = f:read("*a")
    f:close()
    local obj = json.decode(raw)
    return (obj and obj.fields and obj.fields.n and obj.fields.n.value) or 42
end
-- }}}

-- {{{ write_result
-- Writes the routed value and which branch fired to data/state.json.
function M.write_result(n)
    local path = (SORAMECH_MAP_DIR or ".") .. "/data/state.json"
    local f    = io.open(path, "r")
    if not f then return end
    local raw  = f:read("*a")
    f:close()
    local obj  = json.decode(raw) or { constant = false, fields = {} }
    obj.fields.result = { constant = false, value = tostring(n) }
    local tmp  = path .. ".tmp"
    local fw   = io.open(tmp, "w")
    if not fw then return end
    fw:write(json.encode(obj, { indent = true }))
    fw:close()
    os.rename(tmp, path)
end
-- }}}

return M
