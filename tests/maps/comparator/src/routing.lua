local M = {}

-- {{{ M.identity
function M.identity(v) return v end
-- }}}

-- {{{ M.tag
function M.tag(prefix, v) return prefix .. ":" .. v end
-- }}}

return M
