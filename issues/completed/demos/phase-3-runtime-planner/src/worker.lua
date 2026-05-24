-- Phase 3 demo — runtime workers.
--
-- Three small transformation functions. The planner creates one
-- runtime box per function via soramech.create_box{fn=...}. Each
-- worker's task fires once with the same input the planner
-- received; the output goes nowhere (no downstream wires are
-- created from the worker), which is fine — the unwired-output
-- rule discards it. The demo reads each worker's output from the
-- JSONL transcript's task_output events under SORAMECH_LOG_VALUES.

local M = {}

-- {{{ M.uppercase
function M.uppercase(text)
    return "uppercase: " .. string.upper(text)
end
-- }}}

-- {{{ M.reverse
function M.reverse(text)
    return "reverse: " .. string.reverse(text)
end
-- }}}

-- {{{ M.double_length
-- Length of the input, doubled — a small computation that's
-- transparently different from the other two transformations so
-- the demo's output reads as three clearly distinct workers.
function M.double_length(text)
    return "double_length: " .. tostring(#text * 2)
end
-- }}}

return M
