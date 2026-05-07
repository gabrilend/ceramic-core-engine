-- Reads the input name from data/input.json and returns a greeting string.
local data = require("soramech-data")
local M    = {}

-- {{{ M.hello
function M.hello()
    local dir  = SORAMECH_MAP_DIR .. "/data"
    local name, err = data.get(dir, "input.json", "name")
    if err then error("read-input: " .. err) end
    return "hello, " .. tostring(name)
end
-- }}}

return M
