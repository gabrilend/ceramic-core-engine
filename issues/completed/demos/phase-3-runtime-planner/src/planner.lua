-- Phase 3 demo — runtime planner.
--
-- Receives a seed string; spawns three worker boxes mid-run via
-- soramech.create_box, each running a different transformation
-- function from src/worker.lua; wires its own output into each
-- worker's `text` input port so the seed value fans out to all
-- three runtime-built consumers.
--
-- After plan() returns its value, the dispatch's fan-out sees the
-- three freshly-added connections on the planner's own
-- connections array and pushes the return value into each
-- worker's input slot — the new boxes fire under their own Lua
-- spec on whichever pool worker picks them up. The whole
-- planner-creates-workers-then-feeds-workers sequence is one
-- task on the dispatch's perspective; the workers are three
-- additional tasks that get spawned during the planner's fan-out.

local M = {}

-- {{{ M.plan
function M.plan(seed_text)
    local transformations = { "uppercase", "reverse", "double_length" }

    -- Create one runtime box per transformation. The auto-generated
    -- ids come back as strings; we capture them so the connect call
    -- knows where to send each wire. Every create_box emits a
    -- box_create event into the JSONL transcript, every connect
    -- emits a wire_add event (issue 311 — runtime mutation events
    -- always emit, no verbosity gate needed because they are rare
    -- and load-bearing for the audit log).
    for _, fn_name in ipairs(transformations) do
        local worker_id = soramech.create_box{
            kind = "call",
            lang = "lua",
            ref  = "src/worker.lua",
            fn   = fn_name,
            inputs = {
                { name = "text", type = "string" }
            }
        }
        soramech.connect{
            from_box    = "planner",
            from_branch = nil,
            to_box      = worker_id,
            to_input    = "text"
        }
    end

    -- Return the seed verbatim — the dispatch fans this string to
    -- each freshly-wired worker. Each worker's task then runs its
    -- transformation on the same input.
    return seed_text
end
-- }}}

return M
