-- Tiny fixture function used by the loader test.
-- Not executed in this iteration — the loader doesn't run boxes;
-- this file exists so the box's `ref` field resolves to something
-- real on disk when a later iteration starts checking that.

local M = {}

-- {{{ M.greet
function M.greet(name, salutation)
    return (salutation or "Hello") .. ", " .. name .. "!"
end
-- }}}

return M
