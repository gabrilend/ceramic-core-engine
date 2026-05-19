-- Calc fixture functions for the end-to-end dispatch test.
-- Arguments arrive as strings; parse, compute, return a string.

local M = {}

-- {{{ M.add
function M.add(a, b)
    return tostring(tonumber(a) + tonumber(b))
end
-- }}}

-- {{{ M.mul
function M.mul(a, b)
    return tostring(tonumber(a) * tonumber(b))
end
-- }}}

return M
