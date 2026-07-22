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

-- {{{ M.describe
-- Used by the dual-ring dispatch test (issue 312) to distinguish
-- native-tagged from JSON-tagged input cells. The Lua spec turns
-- native bytes into a Lua string — unless they look structured
-- (leading '{' or '['), which since the bug-323 fix parse into the
-- real value, because tables now cross same-language wires as JSON.
-- JSON-tagged bytes always parse (a table for an object). This
-- function reports which shape arrived, so the dispatch test can
-- assert both the per-cell ring tag and the 323 sniff behave.
function M.describe(x)
    if type(x) == "table" then return "table:" .. tostring(x.a)
    else                       return "string:" .. tostring(x)
    end
end
-- }}}

return M
