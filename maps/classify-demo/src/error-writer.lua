-- Writes an error note to data/error.json when the else branch fires.
local data = require("soramech-data")
local M    = {}

-- {{{ M.write_error
function M.write_error(text)
    local dir = SORAMECH_MAP_DIR .. "/data"
    local ok, err = data.set(dir, "error.json", "message", tostring(text))
    if not ok then error("write-error: " .. tostring(err)) end
end
-- }}}

return M
