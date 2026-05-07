-- Writes the result text to data/state.json in the map directory.
-- Requires the soramech-data library on the package path.

local data = require("soramech-data")
local M    = {}

-- {{{ write_result
function M.write_result(map_dir, text)
    -- map_dir is injected by the driver as the first argument
    local ok, werr = data.set(map_dir, "state", "result", text)
    if not ok then
        error("write_result: " .. tostring(werr))
    end
end
-- }}}

return M
