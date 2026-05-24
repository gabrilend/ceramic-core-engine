-- 319-cross-lang-create fixture — the Lua box that the C trigger
-- creates at runtime. The worker's Lua spec handle didn't exist
-- at pool startup (the static graph has no Lua boxes), so this
-- function only runs because dispatch's lazy spec init populated
-- the handle on demand.

local M = {}

-- {{{ M.echo
function M.echo(x)
    return "lua-echo-from-runtime-init:" .. x
end
-- }}}

return M
