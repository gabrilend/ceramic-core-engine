-- Pass-through router: the routing kind does the actual work.
local M = {}
function M.identity(x) return x end
return M
