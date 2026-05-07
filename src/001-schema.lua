-- Schema definitions and validators for SoraMech map files.
-- Validates box files, meta.json, and drivers.json against the spec
-- in docs/001-architecture.md. Used by both the runner and the server
-- before any file is written to disk.

local DIR = "/mnt/mtwo/programs/sora/soramech"

local M = {}

-- {{{ valid_kinds
local valid_kinds = { call = true, branch = true, data = true }
-- }}}

-- {{{ valid_ops
local valid_ops = {
    eq = true, lt = true, gt = true,
    lte = true, gte = true, contains = true, matches = true
}
-- }}}

-- {{{ err
local function err(errors, msg)
    errors[#errors + 1] = msg
end
-- }}}

-- {{{ validate_port
local function validate_port(port, i, errors)
    -- "else" port has no predicate; all others require one
    if port.name == nil then
        err(errors, "port[" .. i .. "] missing 'name'")
        return
    end
    if port.name == "else" then return end
    if port.predicate == nil then
        err(errors, "port '" .. port.name .. "' missing predicate (only 'else' may omit it)")
        return
    end
    local p = port.predicate
    if p.op == nil then
        err(errors, "port '" .. port.name .. "' predicate missing 'op'")
    elseif not valid_ops[p.op] then
        err(errors, "port '" .. port.name .. "' predicate has unknown op '" .. p.op .. "'")
    end
    if p.value == nil then
        err(errors, "port '" .. port.name .. "' predicate missing 'value'")
    end
end
-- }}}

-- {{{ validate_connection
local function validate_connection(c, i, errors)
    -- all four fields must be present; both endpoint files store the same record
    if c.from_box == nil then
        err(errors, "connection[" .. i .. "] missing 'from_box'")
    end
    if c.from_output == nil then
        err(errors, "connection[" .. i .. "] missing 'from_output'")
    end
    if c.to_box == nil then
        err(errors, "connection[" .. i .. "] missing 'to_box'")
    end
    if c.to_input == nil then
        err(errors, "connection[" .. i .. "] missing 'to_input'")
    end
end
-- }}}

-- {{{ validate_port_connection
local function validate_port_connection(c, i, errors)
    -- branch box connections use from_port instead of from_output;
    -- from_box and to_box/to_input are still required for the reciprocal check
    if c.from_box == nil then
        err(errors, "branch connection[" .. i .. "] missing 'from_box'")
    end
    if c.from_port == nil then
        err(errors, "branch connection[" .. i .. "] missing 'from_port'")
    end
    if c.to_box == nil then
        err(errors, "branch connection[" .. i .. "] missing 'to_box'")
    end
    if c.to_input == nil then
        err(errors, "branch connection[" .. i .. "] missing 'to_input'")
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
        err(errors, "missing or invalid 'kind' (must be call, branch, or data)")
    end

    if box.kind == "call" then
        if box.ref == nil or type(box.ref) ~= "string" then
            err(errors, "call box missing 'ref'")
        end
        -- fn is optional for binary callers (shell scripts, binaries)
        if box.inputs ~= nil and type(box.inputs) ~= "table" then
            err(errors, "'inputs' must be an array")
        end
        if box.outputs ~= nil and type(box.outputs) ~= "table" then
            err(errors, "'outputs' must be an array")
        end
        if box.connections ~= nil then
            for i, c in ipairs(box.connections) do
                validate_connection(c, i, errors)
            end
        end

    elseif box.kind == "branch" then
        if box.inputs == nil or type(box.inputs) ~= "table" then
            err(errors, "branch box missing 'inputs'")
        end
        if box.ports == nil or type(box.ports) ~= "table" or #box.ports == 0 then
            err(errors, "branch box missing 'ports' array")
        else
            local has_else = false
            for i, p in ipairs(box.ports) do
                validate_port(p, i, errors)
                if p.name == "else" then has_else = true end
            end
            if not has_else then
                err(errors, "branch box missing required 'else' port")
            end
        end
        if box.connections ~= nil then
            for i, c in ipairs(box.connections) do
                validate_port_connection(c, i, errors)
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
