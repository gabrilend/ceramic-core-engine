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
-- A connection's `from_branch` field identifies which output of
-- the producer box this wire leaves from. The accepted values
-- depend on the producer's `routing.kind` (issue 233):
--   plain      → nil
--   comparator → 'lt' / 'eq' / 'gt'
--   iterator   → 'out_0', 'out_1', ... 'out_<n-1>'
-- The schema here only enforces the lexical shape — that a
-- branch string is either nil, one of the three comparator
-- branches, or `out_<digits>`. Cross-referencing the producer's
-- routing kind happens at the graph level in
-- `check_input_bindings` / `validate-map.lua`.
local valid_branches = { lt = true, eq = true, gt = true }
local function is_iterator_branch_name(name)
    return type(name) == "string" and name:match("^out_%d+$") ~= nil
end
-- }}}

-- {{{ valid_routing_kinds
-- Issue 233's unified `routing` field. Plain is the default
-- shape for a call box that wants its single output fanned to
-- all wires. Comparator and iterator are the two routing kinds
-- shipped under 233. Randomizer / weighted / distributor /
-- multi-band-comparator have their own follow-on issues
-- (240–243); until those ship, the schema rejects their kind
-- values so we don't accumulate dead state.
local valid_routing_kinds = { plain = true, comparator = true, iterator = true }
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
    if c.from_branch ~= nil
       and not valid_branches[c.from_branch]
       and not is_iterator_branch_name(c.from_branch) then
        err(errors, "connection[" .. i .. "] 'from_branch' must be nil, " ..
            "'lt'/'eq'/'gt' (comparator), or 'out_<n>' (iterator)")
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
        -- Unified routing (issue 233). Every call box carries a
        -- `routing` field declaring how its single output reaches
        -- downstream wires. Routing kind also gates whether
        -- ref/fn are required: iterator is a pure routing
        -- primitive with no language function attached; plain
        -- and comparator boxes need a function to invoke.
        local r = box.routing
        local is_routing_primitive = false
        if r == nil then
            err(errors, "call box missing 'routing' field (issue 233)")
        elseif type(r) ~= "table" then
            err(errors, "'routing' must be a table")
        elseif type(r.kind) ~= "string" then
            err(errors, "'routing.kind' must be a string")
        elseif not valid_routing_kinds[r.kind] then
            err(errors, "'routing.kind' = '" .. r.kind ..
                "' is not one of 'plain'/'comparator'/'iterator' " ..
                "(other kinds belong to follow-on issues 240–243)")
        else
            if r.kind == "comparator" then
                if type(r.comparand) ~= "number" then
                    err(errors, "comparator routing requires numeric 'routing.comparand'")
                end
            elseif r.kind == "iterator" then
                is_routing_primitive = true
                if type(r.n_outputs) ~= "number"
                   or r.n_outputs < 1
                   or r.n_outputs ~= math.floor(r.n_outputs) then
                    err(errors, "iterator routing requires 'routing.n_outputs' to be a positive integer")
                end
            end
        end

        -- Legacy fields are explicitly rejected (issue 233 ships
        -- no in-loader migration). The editor or a one-off script
        -- rewrites old maps to the new shape; reading the old
        -- shape with this schema is meant to fail loudly.
        if box.comparand ~= nil then
            err(errors, "'comparand' is a legacy field — use routing = " ..
                "{ kind = 'comparator', comparand = <number> } instead (issue 233)")
        end
        if box.iterator_outputs ~= nil then
            err(errors, "'iterator_outputs' is a legacy field — use routing = " ..
                "{ kind = 'iterator', n_outputs = <integer> } instead (issue 233)")
        end

        if not is_routing_primitive then
            if box.ref == nil or type(box.ref) ~= "string" then
                err(errors, "box missing 'ref'")
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
