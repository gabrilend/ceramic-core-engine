-- src/tag.lua — Lua worker that prefixes its input with a
-- language tag and the worker's reverse-engineered value
-- length, so the output file makes the work visible.

local M = {}

-- {{{ M.tag_lua
function M.tag_lua(v)
    return "lua:    " .. v .. "  (" .. #v .. " bytes)"
end
-- }}}

return M
