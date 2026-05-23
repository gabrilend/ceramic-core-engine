-- Tiny fixture function used by the loader test.
-- Not executed in this iteration — the loader doesn't run boxes;
-- this file exists so the box's `ref` field resolves to something
-- real on disk when a later iteration starts checking that.

local M = {}

-- {{{ M.greet
function M.greet(name, salutation)
    return (salutation or "Hello") .. ", " .. name .. "!"
end
-- }}}

-- {{{ M.sum_pair
-- Used by the slice-4 (issue 312) JSON-input test. Receives a
-- table parsed from JSON bytes and returns the sum of its two
-- fields. If invoke had instead handed us the raw bytes string,
-- the table indexing would error — that's the failure mode the
-- test verifies against.
function M.sum_pair(t)
    return tostring(t.a + t.b)
end
-- }}}

return M
