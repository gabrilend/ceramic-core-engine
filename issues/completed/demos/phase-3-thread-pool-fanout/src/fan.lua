-- src/fan.lua — iterator fan-out passthrough.
--
-- The fan box runs three times under iterator routing
-- (n_outputs=3 in its box JSON). Each run forwards the same
-- input value to a different language-specific tag worker via
-- the iterator's `out_0` / `out_1` / `out_2` branches. The
-- counter that decides which branch fires lives in dispatch;
-- the box itself just returns the value verbatim.

local M = {}

-- {{{ M.passthrough
function M.passthrough(v)
    return v
end
-- }}}

return M
