-- 319a fixture: takes 20 inputs and reports back. The point of the
-- map isn't the computation — it's that dispatch can route 20
-- inputs into a single call without hitting the old [16] cap.

local M = {}

-- {{{ M.combine_twenty
-- Receives 20 string-numbers as positional args. Returns a string
-- of the form "count=20 sum=210" so the test runner can grep for
-- both numbers and catch either kind of regression (lost inputs,
-- truncated arg list, miscounted dispatch).
function M.combine_twenty(...)
    local args = {...}
    local sum = 0
    for i = 1, #args do
        sum = sum + tonumber(args[i])
    end
    return "count=" .. #args .. " sum=" .. sum
end
-- }}}

return M
