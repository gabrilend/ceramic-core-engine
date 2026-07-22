-- Regression pair for bug 323: `make` returns a structured value to
-- a same-language consumer; `use` proves it arrived as a real table
-- (not zero bytes, not a raw JSON string). If the fast path drops or
-- mis-delivers the value, `use` names the type it actually received,
-- so a regression fails the fixture's substring check loudly.
local M = {}

-- {{{ M.make
-- The field is deliberately NOT named `n`: an integer `n` field is
-- the issue-317 array-length convention and would make the encoder
-- emit a JSON array of nulls instead of an object. Real box authors
-- can hit that edge too — see the encoder notes in langs/lua/spec.c.
function M.make(tag)
    return { half = 21, tag = tag }
end
-- }}}

-- {{{ M.use
function M.use(t)
    if type(t) ~= "table" then
        return "not-a-table:" .. type(t)
    end
    return "sum=" .. tostring(t.half * 2) .. ":" .. tostring(t.tag)
end
-- }}}

return M
