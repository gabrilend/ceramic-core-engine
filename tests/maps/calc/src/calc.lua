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

-- {{{ M.identity
-- Used by the distributor dispatch test as a passthrough sink: the
-- distributor's argmin picker is the unit under test, the sink just
-- needs to capture whatever it received.
function M.identity(x)
    return tostring(x)
end
-- }}}

return M
