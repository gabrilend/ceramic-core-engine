-- Math utility functions for driver-test map.
local data = require("soramech-data")
local M    = {}

-- {{{ M.add
-- Reads a and b from data/state.json; no wired inputs so lua-box fires immediately.
function M.add()
    local dir    = SORAMECH_MAP_DIR .. "/data"
    local a, ea  = data.get(dir, "state.json", "a")
    local b, eb  = data.get(dir, "state.json", "b")
    if ea then error("add: " .. ea) end
    if eb then error("add: " .. eb) end
    return a + b
end
-- }}}

return M
