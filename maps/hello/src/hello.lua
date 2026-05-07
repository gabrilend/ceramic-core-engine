-- Hello function for the hello example map.
-- Takes a name string and returns a greeting string.

local M = {}

-- {{{ hello
function M.hello(name)
    return "Hello, " .. tostring(name) .. "!"
end
-- }}}

return M
