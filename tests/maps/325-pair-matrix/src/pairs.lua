-- Pair-matrix fixture functions, Lua side (issue 325). `reflect`
-- reports the received value AND its Lua type — the type is the
-- decode-fidelity pin: native raw bytes arrive as a string, a
-- cross-language JSON number must arrive as a number.
local M = {}

-- {{{ M.produce
function M.produce(seed)
    return seed .. "-p"
end
-- }}}

-- {{{ M.pnum
-- Returns a real Lua number so the encoder emits a bare JSON
-- number on cross-language wires (same reasoning as the pipeline
-- fixture's double).
function M.pnum(s)
    return tonumber(s) + 1
end
-- }}}

-- {{{ M.reflect
function M.reflect(x)
    return "lua-saw:" .. tostring(x) .. ":" .. type(x)
end
-- }}}

return M
