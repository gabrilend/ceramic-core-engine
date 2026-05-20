-- Schema definitions and validators for SoraMech map files.
-- Validates box files, meta.json, and drivers.json against the spec
-- in docs/001-architecture.md. Used by both the runner and the server
-- before any file is written to disk.

local DIR = "/mnt/mtwo/programs/sora/soramech"

local M = {}

-- {{{ valid_kinds
-- `call` runs a language function. `read` and `write` are dispatch-
-- layer IO primitives (issue 229) — `read` emits an inline literal
-- or file contents, `write` commits a `value` to disk and emits a
-- boolean done signal.
local valid_kinds = { call = true, read = true, write = true }
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
        err(errors, "missing or invalid 'kind' (must be 'call', 'read', or 'write')")
    end

    if box.kind == "call" then
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
        -- Per-port `optional` flag (issue 230). Absent means false: the
        -- port is required and must be either wired or carry a literal
        -- value. The wire/literal/optional triple-check happens at the
        -- graph level (validate-map.lua and the editor's compile path),
        -- not here — this only validates per-port shape.
        if box.inputs ~= nil and type(box.inputs) == "table" then
            for i, p in ipairs(box.inputs) do
                if type(p) == "table" and p.optional ~= nil and type(p.optional) ~= "boolean" then
                    err(errors, "'inputs[" .. i .. "].optional' must be a boolean")
                end
            end
        end
        if box.connections ~= nil then
            for i, c in ipairs(box.connections) do
                validate_connection(c, i, errors)
            end
        end
    end

    -- read / write: dispatch-layer IO primitives (issue 229). Shape
    -- check is intentionally permissive — the graph loader does the
    -- "has a value or a path" semantic check.
    if box.kind == "read" or box.kind == "write" then
        if box.value ~= nil and type(box.value) ~= "string" then
            err(errors, "'value' must be a string")
        end
        if box.path ~= nil and type(box.path) ~= "string" then
            err(errors, "'path' must be a string")
        end
        if box.inputs ~= nil and type(box.inputs) ~= "table" then
            err(errors, "'inputs' must be an array")
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

-- {{{ check_input_bindings
-- Graph-level check from issue 230: every input port on every box
-- must be in one of three states by compile time —
--   1. a wire is connected to it
--   2. a literal `value` is set on it
--   3. it carries `optional: true`
-- Otherwise the box would silently receive a missing arg at runtime
-- and the bug would only surface where the function happens to
-- reference it. The check fails loud and points at the exact box +
-- port instead.
--
-- `boxes` is a table mapping box-id → box record (the shape the
-- editor cache and the phase 2 loader's graph use). Returns an
-- array of error strings; empty array means OK.
function M.check_input_bindings(boxes)
    local errors = {}

    -- precompute: for each (to_box, to_input) → true if a wire targets it.
    -- Connections live on both endpoints (issue 228), so scanning every
    -- box's connection list overcounts but doesn't miss anything.
    local wired = {}
    for _, box in pairs(boxes) do
        for _, c in ipairs(box.connections or {}) do
            if c.to_box and c.to_input then
                wired[c.to_box .. "\0" .. c.to_input] = true
            end
        end
    end

    -- stable per-box iteration for predictable error ordering
    local ids = {}
    for id, _ in pairs(boxes) do ids[#ids + 1] = id end
    table.sort(ids)

    for _, id in ipairs(ids) do
        local box = boxes[id]
        for _, p in ipairs(box.inputs or {}) do
            local has_wire    = wired[id .. "\0" .. p.name] == true
            local has_literal = p.value ~= nil and p.value ~= ""
            local is_optional = p.optional == true
            if not (has_wire or has_literal or is_optional) then
                err(errors, "box '" .. id .. "': input '" .. tostring(p.name) ..
                    "' has no wire, no literal, and is not optional")
            end
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
