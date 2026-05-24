-- tests/maps/246-lua-shim/src/echo.lua — fixture Lua box for issue 246.
-- Receives one input ("msg") and returns it prefixed with "seen:".
-- The port has a custom_translation shim attached that reverses
-- the input AND prepends "REV-" before the box function sees it.
-- The output proves the shim ran.

local M = {}

-- {{{ M.echo
function M.echo(msg)
    return "seen:" .. msg
end
-- }}}

return M
