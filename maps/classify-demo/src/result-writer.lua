-- Writes the routed greeting to data/output.json.
local data = require("soramech-data")
local M    = {}

-- {{{ M.write_result
function M.write_result(text)
    local dir = SORAMECH_MAP_DIR .. "/data"
    local ok, err = data.set(dir, "output.json", "result", tostring(text))
    if not ok then error("write-result: " .. tostring(err)) end
end
-- }}}

return M
