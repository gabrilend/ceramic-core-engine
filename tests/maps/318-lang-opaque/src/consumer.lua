-- 318 fixture consumer: receives the producer's bundle (decoded
-- from JSON via the $lang_opaque sentinel for the function field)
-- and invokes the function on the val. The test runner asserts the
-- output reflects the call.

local M = {}

-- {{{ M.consumer
function M.consumer(bundle)
    local fn  = bundle.fn
    local val = bundle.val
    if type(fn) ~= "function" then
        return "fn-type=" .. type(fn)
    end
    local doubled = fn(val)
    return "doubled=" .. tostring(doubled)
end
-- }}}

return M
