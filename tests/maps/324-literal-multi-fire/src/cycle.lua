-- Cycle fixture for bug 324: a literal-fed box inside an iterator
-- cycle must keep firing. `step` counts laps; reaching the
-- comparator's threshold (3) requires the const port to survive
-- three pops — impossible before the fix, when the startup literal
-- drained after one revolution and the box silently never re-fired.
local M = {}

-- {{{ M.identity
function M.identity(v)
    return tostring(v)
end
-- }}}

-- {{{ M.step
function M.step(tick, const)
    if const ~= "elara" then
        -- A wrong or missing constant routes a sentinel count to the
        -- gt branch, so the failure names itself in the output
        -- ("D:99") instead of hiding inside a plausible number.
        return "99"
    end
    return tostring(tonumber(tick) + 1)
end
-- }}}

-- {{{ M.tag
function M.tag(tag, v)
    return tag .. ":" .. tostring(v)
end
-- }}}

return M
