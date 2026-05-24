-- Pass-through router function. The routing kind (randomizer /
-- weighted / distributor / comparator) does the branch picking;
-- this just returns its input unchanged so the dispatch fans
-- the same value to whichever branch wins.

local M = {}

-- {{{ M.identity
function M.identity(x) return x end
-- }}}

return M
