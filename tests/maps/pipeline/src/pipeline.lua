local M = {}

-- {{{ M.double
-- Returns the doubled value as a Lua number. The wire encoder
-- handles JSON serialization (issue 312 slice 4.5) when the
-- consumer is in another language; an explicit tostring here
-- would JSON-encode "10" as `"10"` (a quoted string) where the
-- downstream C consumer expects the bare number 10. Letting the
-- encoder see the actual number is the right shape.
function M.double(s)
    return tonumber(s) * 2
end
-- }}}

return M
