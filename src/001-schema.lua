-- Schema definitions and validators for SoraMech map files.
-- Validates box files, meta.json, and drivers.json against the spec
-- in docs/002-map-model.md. Used by both the runner and the server
-- before any file is written to disk.
-- (This pointed at docs/001-architecture.md, a file that no longer
-- exists; the map/box format now lives in 002-map-model.md.)

local DIR = "/mnt/mtwo/programs/sora/soramech"

local M = {}

-- {{{ valid_kinds
-- `call` runs a language function. `read` and `write` are dispatch-
-- layer IO primitives (issue 229) — `read` emits an inline literal
-- or file contents, `write` commits a `value` to disk and emits a
-- boolean done signal. `map` is the encapsulated-sub-map box from
-- issue 248: it names a sub-map directory via `ref`, declares its
-- own input and output ports, and is replaced at graph-load by
-- the inlined sub-map's boxes via the encapsulation splice.
local valid_kinds = { call = true, read = true, write = true, map = true }
-- }}}

-- {{{ valid_external_kinds (issue 248)
-- A `read` box marked externally-supplied OR a `write` box marked
-- externally-consumed carries an `external` block whose `kind`
-- picks one of three binding shapes. The C loader's
-- parse_external_binding is the canonical parser; this table is
-- the lexical gate.
local valid_external_kinds = { positional = true, numbered = true, named = true }
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
-- Issue 233's unified `routing` field. Seven kinds shipped:
--   plain         — single output port, fan to every wire
--   comparator    — numeric threshold(s), pick lt/eq/gt or named bands
--   iterator      — round-robin over n_outputs ports
--   randomizer    — hash(counter) mod n_outputs (issue 240)
--   weighted      — cumulative-band lookup over weights (issue 241)
--   distributor   — argmin over downstream slot fill (issue 242)
--   nonlinearity  — value-transforming: maps input to a smoothed
--                   score via one of three intent-named variants
--                   (decision / confidence / calibration), issue 250
local valid_routing_kinds = {
    plain        = true,
    comparator   = true,
    iterator     = true,
    randomizer   = true,
    weighted     = true,
    distributor  = true,
    nonlinearity = true,
}

-- {{{ valid_nonlinearity_ranges (issue 253)
-- The nonlinearity refactor (253) replaced the three variant
-- names with a single `range` toggle. `signed` picks tanh and
-- output [-1, 1]; `unit` picks sigmoid and output [0, 1]. The
-- gated output (v × score) makes the soft-AND composition
-- intrinsic to the box.
local valid_nonlinearity_ranges = {
    signed = true,
    unit   = true,
}
-- }}}
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
    -- from_branch accepts: nil, the three comparator branches,
    -- iterator slot names, OR any non-empty string (issue 248 —
    -- encapsulated map boxes use the from_branch slot to carry a
    -- declared output port name; the graph-level loader's
    -- output-side splice is the source of truth for whether the
    -- port actually exists on the producer).
    if c.from_branch ~= nil
       and not valid_branches[c.from_branch]
       and not is_iterator_branch_name(c.from_branch)
       and not (type(c.from_branch) == "string" and #c.from_branch > 0) then
        err(errors, "connection[" .. i .. "] 'from_branch' must be nil, " ..
            "'lt'/'eq'/'gt', 'out_<n>', or a non-empty string")
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
        err(errors, "missing or invalid 'kind' (must be 'call', 'read', 'write', or 'map')")
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
                "' is not one of 'plain' / 'comparator' / 'iterator' / " ..
                "'randomizer' / 'weighted' / 'distributor'")
        else
            if r.kind == "comparator" then
                -- Issue 243: multi-band-comparator accepts a non-empty
                -- `thresholds` array (non-decreasing; doubled values
                -- carve out equality bands) as the modern shape.
                -- Single-comparand legacy form is still accepted for
                -- backwards compat — the loader normalises it to
                -- `thresholds = [c, c]` on the C side.
                if r.thresholds ~= nil then
                    if type(r.thresholds) ~= "table" or #r.thresholds == 0 then
                        err(errors, "comparator 'routing.thresholds' must be a non-empty array")
                    else
                        local last = nil
                        for i, t in ipairs(r.thresholds) do
                            if type(t) ~= "number" then
                                err(errors, "comparator 'routing.thresholds[" .. i .. "]' must be a number")
                                break
                            elseif last ~= nil and t < last then
                                err(errors, "comparator 'routing.thresholds' must be non-decreasing " ..
                                    "(saw " .. tostring(last) .. " then " .. tostring(t) .. ")")
                                break
                            end
                            last = t
                        end
                    end
                elseif type(r.comparand) ~= "number" then
                    err(errors, "comparator routing requires numeric 'routing.comparand' " ..
                        "or non-empty 'routing.thresholds' array")
                end
            elseif r.kind == "iterator" then
                is_routing_primitive = true
                if type(r.n_outputs) ~= "number"
                   or r.n_outputs < 1
                   or r.n_outputs ~= math.floor(r.n_outputs) then
                    err(errors, "iterator routing requires 'routing.n_outputs' to be a positive integer")
                end
            elseif r.kind == "randomizer" or r.kind == "distributor" then
                -- Same shape as iterator: a positive-integer
                -- n_outputs. Randomizer (issue 240) and distributor
                -- (issue 242) share the counter-driven dispatch
                -- machinery iterator uses, just with a different
                -- picker.
                if type(r.n_outputs) ~= "number"
                   or r.n_outputs < 1
                   or r.n_outputs ~= math.floor(r.n_outputs) then
                    err(errors, r.kind .. " routing requires 'routing.n_outputs' to be a positive integer")
                end
            elseif r.kind == "weighted" then
                -- Issue 241: weights is a non-empty array of
                -- non-negative numbers. The dispatch normalises at
                -- run time (cumulative table on a fixed-precision
                -- scale), so the user doesn't have to sum to 1.0.
                if type(r.weights) ~= "table" or #r.weights == 0 then
                    err(errors, "weighted routing requires non-empty 'routing.weights' array")
                else
                    for i, w in ipairs(r.weights) do
                        if type(w) ~= "number" or w < 0 then
                            err(errors, "weighted 'routing.weights[" .. i .. "]' must be a non-negative number")
                            break
                        end
                    end
                end
            elseif r.kind == "nonlinearity" then
                -- Issue 253 — the refactor. `range` picks the
                -- output range and the S-curve (signed → tanh,
                -- unit → sigmoid). `memory` is the ring-buffer
                -- size for auto-calibration. `k` is the steepness.
                -- Output is v × score (gated), not score alone.
                if type(r.range) ~= "string"
                   or not valid_nonlinearity_ranges[r.range] then
                    err(errors, "nonlinearity 'routing.range' must be 'signed' or 'unit'")
                end
                if r.memory ~= nil then
                    if type(r.memory) ~= "number"
                       or r.memory < 1
                       or r.memory ~= math.floor(r.memory) then
                        err(errors, "nonlinearity 'routing.memory' must be a positive integer when set")
                    end
                end
                if r.k ~= nil and (type(r.k) ~= "number" or r.k <= 0) then
                    err(errors, "nonlinearity 'routing.k' (steepness) must be a positive number when set")
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
        -- Issue 248: optional `external` block. Marks a read box as
        -- externally-supplied (its value comes from the encapsulating
        -- parent's wire) or a write box as externally-consumed (its
        -- emitted value surfaces back to the parent on a wire). The
        -- block carries a binding kind plus either a name or an
        -- index, matched by the loader's encapsulation splice.
        if box.external ~= nil then
            if type(box.external) ~= "table" then
                err(errors, "'external' must be a table")
            else
                if type(box.external.kind) ~= "string"
                   or not valid_external_kinds[box.external.kind] then
                    err(errors, "'external.kind' must be 'positional', 'numbered', or 'named'")
                elseif box.external.kind == "named" then
                    if type(box.external.name) ~= "string" or #box.external.name == 0 then
                        err(errors, "'external.kind' = 'named' requires non-empty 'external.name'")
                    end
                else
                    if type(box.external.index) ~= "number"
                       or box.external.index < 0
                       or box.external.index ~= math.floor(box.external.index) then
                        err(errors, "'external.kind' = '" .. box.external.kind ..
                            "' requires non-negative integer 'external.index'")
                    end
                end
            end
        end
    end

    -- Issue 248: encapsulated sub-map box. `ref` points at a sub-map
    -- directory (relative to the parent map, or absolute). `inputs`
    -- and `outputs` declare the encap box's port set on the canvas;
    -- they're matched at graph load to the sub-map's
    -- externally-supplied / externally-consumed marked boxes by
    -- name or index. The C loader's encapsulation pass is the
    -- canonical resolver; this schema only checks lexical shape.
    if box.kind == "map" then
        if box.ref == nil or type(box.ref) ~= "string" or #box.ref == 0 then
            err(errors, "map box missing 'ref' (path to sub-map directory)")
        end
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
