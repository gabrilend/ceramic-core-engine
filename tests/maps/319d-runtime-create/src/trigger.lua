-- 319d fixture — the trigger box uses soramech.create_box +
-- soramech.connect to spawn a downstream "echo_dyn" box at runtime
-- and wire its own output to it. The runtime then sees the new
-- connection on the trigger's connections[] array and pushes the
-- trigger's return value into the freshly-created downstream slot.

local M = {}

-- {{{ M.trigger
function M.trigger(input)
    -- Build the box spec as a Lua table that mirrors the on-disk
    -- box JSON schema.
    local new_id = soramech.create_box{
        kind = "call",
        lang = "lua",
        ref  = "src/echo_dyn.lua",
        fn   = "echo",
        inputs = {
            {name = "x", type = "string"}
        }
    }
    -- Wire this box's output to the new box's "x" port.
    soramech.connect{
        from_box    = "trigger",
        from_branch = nil,
        to_box      = new_id,
        to_input    = "x"
    }
    -- Return a marker that includes the new box's id so the test
    -- runner can verify create_box returned something sensible.
    return "trigger-fired:" .. new_id .. ":" .. input
end
-- }}}

return M
