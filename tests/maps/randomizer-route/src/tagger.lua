-- One function per downstream tagger. Each prepends a distinct
-- prefix to its input so the test runner can tell from stderr
-- which branch the routing box picked.

local M = {}

-- {{{ M.tag_0
function M.tag_0(text) return "got_0:" .. text end
-- }}}

-- {{{ M.tag_1
function M.tag_1(text) return "got_1:" .. text end
-- }}}

-- {{{ M.tag_2
function M.tag_2(text) return "got_2:" .. text end
-- }}}

return M
