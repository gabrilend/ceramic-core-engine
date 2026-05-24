-- 319d fixture — the dynamically-created box's function. The
-- trigger box's create_box call references this file by path; the
-- dispatch loads it lazily on first invocation just like any
-- normal Lua call box.

local M = {}

-- {{{ M.echo
function M.echo(x)
    return "echo_dyn-received:" .. x
end
-- }}}

return M
