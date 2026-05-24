-- 318 fixture producer: returns a table containing a Lua function
-- plus a regular number. The fan-out to the write box forces the
-- producer's output to JSON (the write box has no language, so the
-- producer's output_native is 0). The function lands in the JSON
-- as a $lang_opaque sentinel pointing into this Lua state's
-- per-worker registry; the Lua-language consumer can reconstruct
-- and invoke it.

local M = {}

-- {{{ M.producer
function M.producer(n)
    local val = tonumber(n) or 0
    return {
        fn  = function(x) return x * 2 end,
        val = val,
    }
end
-- }}}

return M
