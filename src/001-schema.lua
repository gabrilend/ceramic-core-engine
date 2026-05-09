-- Schema definitions and validators for SoraMech map files.
-- Validates box files, meta.json, and drivers.json against the spec
-- in docs/001-architecture.md. Used by both the runner and the server
-- before any file is written to disk.

local DIR = "/mnt/mtwo/programs/sora/soramech"

local M = {}

-- {{{ valid_kinds
local valid_kinds = { call = true, data = true }
-- }}}

-- {{{ valid_branches
-- null/nil means no comparator (single wire); the three strings are comparator outputs.
local valid_branches = { lt = true, eq = true, gt = true }
-- }}}

-- {{{ err
local function err(errors, msg)
    errors[#errors + 1] = msg
end
-- }}}

-- {{{ validate_connection
local function validate_connection(c, i, errors)
    -- from_branch is nil (no comparator) or one of "lt"/"eq"/"gt"
    if c.from_box == nil then
        err(errors, "connection[" .. i .. "] missing 'from_box'")
    end
    if c.from_branch ~= nil and not valid_branches[c.from_branch] then
        err(errors, "connection[" .. i .. "] 'from_branch' must be nil, 'lt', 'eq', or 'gt'")
    end
    if c.to_box == nil then
        err(errors, "connection[" .. i .. "] missing 'to_box'")
    end
    if c.to_input == nil then
        err(errors, "connection[" .. i .. "] missing 'to_input'")
    end
end
-- }}}

-- {{{ validate_box
function M.validate_box(box)
    local errors = {}
    if type(box) ~= "table" then
        return { "box must be a table" }
    end

    if box.id == nil or type(box.id) ~= "string" then
        err(errors, "missing or non-string 'id'")
    end
    if box.label == nil or type(box.label) ~= "string" then
        err(errors, "missing or non-string 'label'")
    end
    if box.kind == nil or not valid_kinds[box.kind] then
        err(errors, "missing or invalid 'kind' (must be 'call' or 'data')")
    end

    if box.kind == "call" or box.kind == "data" then
        -- iterator boxes (issue 221) are a pure routing primitive: input
        -- copies straight to a chosen output, no language function is
        -- invoked. Such a box has no ref/fn — only `iterator_outputs`,
        -- an ordered list of output slot names. Otherwise ref is required.
        local is_iterator = box.iterator_outputs ~= nil
        if not is_iterator then
            if box.ref == nil or type(box.ref) ~= "string" then
                err(errors, "box missing 'ref'")
            end
        end
        if box.iterator_outputs ~= nil then
            if type(box.iterator_outputs) ~= "table" then
                err(errors, "'iterator_outputs' must be an array")
            else
                for i, name in ipairs(box.iterator_outputs) do
                    if type(name) ~= "string" then
                        err(errors, "'iterator_outputs[" .. i .. "]' must be a string")
                    end
                end
            end
        end
        -- fn is optional for binary callers (shell scripts, binaries)
        if box.inputs ~= nil and type(box.inputs) ~= "table" then
            err(errors, "'inputs' must be an array")
        end
        -- variadic_inputs: optional list of base names; each name must
        -- correspond to a series of inputs named "<base>_<index>" in
        -- box.inputs (the editor maintains this; we only check the
        -- shape here). See issue 217 part B.
        if box.variadic_inputs ~= nil then
            if type(box.variadic_inputs) ~= "table" then
                err(errors, "'variadic_inputs' must be an array")
            else
                for i, name in ipairs(box.variadic_inputs) do
                    if type(name) ~= "string" then
                        err(errors, "'variadic_inputs[" .. i .. "]' must be a string")
                    end
                end
            end
        end
        -- comparand: optional; if present it must be a string parseable as number at runtime
        if box.comparand ~= nil and type(box.comparand) ~= "string" then
            err(errors, "'comparand' must be a string")
        end
        -- has_output: optional; absence means "true" (default behavior).
        -- false marks a sink box whose function has no return values
        -- (issue 226); the runtime treats such boxes as terminal and the
        -- editor hides the output port and inspector output section.
        if box.has_output ~= nil and type(box.has_output) ~= "boolean" then
            err(errors, "'has_output' must be a boolean")
        end
        if box.connections ~= nil then
            for i, c in ipairs(box.connections) do
                validate_connection(c, i, errors)
            end
        end
    end

    if box.ui ~= nil then
        if type(box.ui) ~= "table" then
            err(errors, "'ui' must be a table")
        elseif box.ui.x == nil or box.ui.y == nil then
            err(errors, "'ui' must have 'x' and 'y' fields")
        end
    end

    return errors
end
-- }}}

-- {{{ validate_meta
function M.validate_meta(meta)
    local errors = {}
    if type(meta) ~= "table" then
        return { "meta.json must be a table" }
    end
    if meta.name == nil or type(meta.name) ~= "string" then
        err(errors, "meta.json missing 'name'")
    end
    if meta.entry_box_id == nil or type(meta.entry_box_id) ~= "string" then
        err(errors, "meta.json missing 'entry_box_id'")
    end
    return errors
end
-- }}}

-- {{{ validate_drivers
function M.validate_drivers(drivers)
    local errors = {}
    if type(drivers) ~= "table" then
        return { "drivers.json must be a table" }
    end
    for ext, path in pairs(drivers) do
        if type(ext) ~= "string" or ext:sub(1, 1) ~= "." then
            err(errors, "driver key '" .. tostring(ext) .. "' must be a dot-prefixed extension")
        end
        if type(path) ~= "string" then
            err(errors, "driver value for '" .. ext .. "' must be a string path")
        end
    end
    return errors
end
-- }}}

return M
