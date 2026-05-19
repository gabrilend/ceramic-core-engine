-- CLI tool that validates a SoraMech map directory for correctness.
-- Checks schema of all box files, validates connection endpoints exist
-- and agree on both ends, and enforces branch box rules.
-- Run: luajit src/002-validate-map.lua <map-dir>
-- Exits 0 if valid, non-zero with error report if not.

local DIR = "/mnt/mtwo/programs/sora/soramech"

-- allow running from any directory by finding libs relative to DIR
package.path = DIR .. "/libs/?.lua;" ..
               DIR .. "/src/?.lua;" ..
               package.path

local json   = require("dkjson")
local schema = require("001-schema")

-- {{{ read_file
local function read_file(path)
    local f, ferr = io.open(path, "r")
    if not f then return nil, ferr end
    local contents = f:read("*a")
    f:close()
    return contents
end
-- }}}

-- {{{ parse_json_file
local function parse_json_file(path)
    local raw, ferr = read_file(path)
    if not raw then return nil, "cannot open " .. path .. ": " .. ferr end
    local obj, _, jerr = json.decode(raw)
    if not obj then return nil, "JSON error in " .. path .. ": " .. jerr end
    return obj
end
-- }}}

-- {{{ load_boxes
local function load_boxes(map_dir)
    -- returns table of {id -> box_table} and a list of errors
    local boxes = {}
    local errors = {}
    local boxes_dir = map_dir .. "/boxes"

    local handle = io.popen("ls " .. boxes_dir .. "/*.json 2>/dev/null")
    if not handle then
        return boxes, { "cannot list " .. boxes_dir }
    end
    local listing = handle:read("*a")
    handle:close()

    for path in listing:gmatch("[^\n]+") do
        local box, berr = parse_json_file(path)
        if not box then
            errors[#errors + 1] = berr
        else
            local box_errors = schema.validate_box(box)
            if #box_errors > 0 then
                for _, e in ipairs(box_errors) do
                    errors[#errors + 1] = path .. ": " .. e
                end
            else
                boxes[box.id] = box
            end
        end
    end

    return boxes, errors
end
-- }}}

-- {{{ check_connections
local function check_connections(boxes, errors)
    -- each connection is stored in BOTH endpoint box files as the same 4-field record.
    -- we only process connections where from_box == this box's id (outgoing connections).
    -- incoming reciprocal records (from_box != id) are skipped here; they are validated
    -- implicitly when we process the source box.
    for id, box in pairs(boxes) do
        local conns = box.connections or {}
        for _, c in ipairs(conns) do
            if c.from_box ~= id then
                -- this is the reciprocal copy stored for reference; skip
                goto next_conn
            end

            local target = boxes[c.to_box]
            if target == nil then
                errors[#errors + 1] = "box '" .. id .. "': connection to unknown box '" ..
                    tostring(c.to_box) .. "'"
            else
                -- find exact match in target: all four fields must agree
                local found = false
                for _, tc in ipairs(target.connections or {}) do
                    if tc.from_box    == c.from_box   and
                       tc.from_output == c.from_output and
                       tc.to_box      == c.to_box      and
                       tc.to_input    == c.to_input    then
                        found = true
                        break
                    end
                end
                if not found then
                    errors[#errors + 1] = "box '" .. id .. "': connection " ..
                        c.from_box .. "." .. c.from_output ..
                        " -> " .. c.to_box .. "." .. c.to_input ..
                        " not found in target box file (both ends must store the same record)"
                end
            end

            ::next_conn::
        end
    end
end
-- }}}

-- {{{ check_branch_else
local function check_branch_else(boxes, errors)
    -- branch boxes must have an "else" port (enforced by schema, re-checked here
    -- for clarity), and numeric/string-predicate branch boxes must wire "else"
    for id, box in pairs(boxes) do
        if box.kind ~= "branch" then goto continue end

        local has_else_port = false
        local has_else_wire = false
        for _, p in ipairs(box.ports or {}) do
            if p.name == "else" then
                has_else_port = true
            end
        end
        for _, c in ipairs(box.connections or {}) do
            if c.from_port == "else" then
                has_else_wire = true
            end
        end

        if not has_else_port then
            errors[#errors + 1] = "branch box '" .. id .. "' missing 'else' port"
        end

        -- if else is unwired, the box must have retry_vary upstream to be valid;
        -- we cannot check retry_vary here without traversing upstream, so we emit
        -- a warning rather than a hard error — the runner will enforce at execute time
        if has_else_port and not has_else_wire then
            errors[#errors + 1] = "branch box '" .. id ..
                "': 'else' port is unwired (valid only for LLM retry; runner will " ..
                "require upstream box to have retry_vary defined)"
        end

        ::continue::
    end
end
-- }}}

-- {{{ validate_map
local function validate_map(map_dir)
    local errors = {}

    -- meta.json
    local meta, merr = parse_json_file(map_dir .. "/meta.json")
    if not meta then
        errors[#errors + 1] = merr
    else
        local meta_errors = schema.validate_meta(meta)
        for _, e in ipairs(meta_errors) do
            errors[#errors + 1] = "meta.json: " .. e
        end
    end

    -- drivers.json
    local drivers, derr = parse_json_file(map_dir .. "/drivers.json")
    if not drivers then
        errors[#errors + 1] = derr
    else
        local driver_errors = schema.validate_drivers(drivers)
        for _, e in ipairs(driver_errors) do
            errors[#errors + 1] = "drivers.json: " .. e
        end
    end

    -- boxes
    local boxes, box_errors = load_boxes(map_dir)
    for _, e in ipairs(box_errors) do
        errors[#errors + 1] = e
    end

    -- cross-box checks (only if boxes loaded cleanly enough)
    if #errors == 0 then
        check_connections(boxes, errors)
        check_branch_else(boxes, errors)

        -- entry box must exist
        if meta and meta.entry_box_id then
            if not boxes[meta.entry_box_id] then
                errors[#errors + 1] = "meta.json entry_box_id '" ..
                    meta.entry_box_id .. "' does not match any box file"
            end
        end

        -- Issue 230: every input port must be wired, littered, or
        -- flagged optional. Catches the silent-nil class at validation
        -- time so a quiet-runtime-failure can't escape the editor.
        local bind_errors = schema.check_input_bindings(boxes)
        for _, e in ipairs(bind_errors) do
            errors[#errors + 1] = e
        end
    end

    return errors
end
-- }}}

-- {{{ main
local function main(args)
    local map_dir = args[1]
    if map_dir == nil then
        io.stderr:write("usage: luajit 002-validate-map.lua <map-dir>\n")
        os.exit(1)
    end

    local errors = validate_map(map_dir)

    if #errors == 0 then
        print("OK: " .. map_dir .. " is valid")
        os.exit(0)
    else
        io.stderr:write("INVALID: " .. map_dir .. "\n")
        for _, e in ipairs(errors) do
            io.stderr:write("  error: " .. e .. "\n")
        end
        os.exit(1)
    end
end
-- }}}

main(arg)
