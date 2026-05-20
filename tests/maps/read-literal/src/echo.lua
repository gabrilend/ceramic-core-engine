local M = {}

-- {{{ M.echo
-- Identity function — the value that arrives via the wire is the
-- value that leaves. Used by the read-literal fixture to confirm
-- the read box's inline value reaches the downstream slot intact.
function M.echo(x)
    return x
end
-- }}}

return M
