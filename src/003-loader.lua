-- Graph loader for the SoraMech runner.
-- Reads a map directory into an in-memory graph structure and validates
-- its integrity before execution begins. The graph is the runner's
-- internal representation of the map.

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path = DIR .. "/libs/?.lua;" ..
               DIR .. "/src/?.lua;" ..
               package.path

local json   = require("dkjson")
local schema = require("001-schema")

local M = {}

-- {{{ read_json_file
local function read_json_file(path)
    local f, ferr = io.open(path, "r")
    if not f then return nil, "cannot open " .. path .. ": " .. ferr end
    local raw = f:read("*a")
    f:close()
    local obj, _, jerr = json.decode(raw)
    if not obj then return nil, "JSON error in " .. path .. ": " .. tostring(jerr) end
    return obj
end
-- }}}

-- {{{ load_drivers
local function load_drivers(map_dir)
    -- map-local drivers take priority; fall back to global drivers alongside the runner
    local local_path  = map_dir .. "/drivers.json"
    local global_path = DIR .. "/drivers.json"

    local drivers = {}

    -- load global first so local can override
    local gd, _ = read_json_file(global_path)
    if gd then
        for ext, path in pairs(gd) do drivers[ext] = path end
    end

    local ld, _ = read_json_file(local_path)
    if ld then
        for ext, path in pairs(ld) do drivers[ext] = path end
    end

    return drivers
end
-- }}}

-- {{{ list_box_files
local function list_box_files(boxes_dir)
    local files = {}
    local handle = io.popen("ls " .. boxes_dir .. "/*.json 2>/dev/null")
    if not handle then return files end
    local listing = handle:read("*a")
    handle:close()
    for path in listing:gmatch("[^\n]+") do
        files[#files + 1] = path
    end
    return files
end
-- }}}

-- {{{ extension_of
local function extension_of(path)
    return path:match("%.([^./]+)$") and ("." .. path:match("%.([^./]+)$")) or ""
end
-- }}}

-- {{{ detect_cycles
-- depth-first search from start_id; all cycles are errors
local function detect_cycles(boxes, start_id)
    local visited  = {}
    local in_stack = {}
    local errors   = {}

    local function dfs(id)
        if in_stack[id] then
            errors[#errors + 1] = "cycle detected at box '" .. id .. "'"
            return
        end
        if visited[id] then return end
        visited[id]  = true
        in_stack[id] = true

        local box = boxes[id]
        if not box then
            in_stack[id] = false
            return
        end

        for _, c in ipairs(box.connections or {}) do
            if c.from_box == id then
                dfs(c.to_box)
            end
        end

        in_stack[id] = false
    end

    dfs(start_id)
    return errors
end
-- }}}

-- {{{ load_map
function M.load_map(map_dir)
    local errors = {}

    -- meta.json
    local meta, merr = read_json_file(map_dir .. "/meta.json")
    if not meta then
        return nil, { merr }
    end
    local meta_errs = schema.validate_meta(meta)
    for _, e in ipairs(meta_errs) do errors[#errors + 1] = "meta.json: " .. e end

    -- drivers
    local drivers = load_drivers(map_dir)

    -- boxes
    local boxes = {}
    local box_files = list_box_files(map_dir .. "/boxes")
    for _, path in ipairs(box_files) do
        local box, berr = read_json_file(path)
        if not box then
            errors[#errors + 1] = berr
        else
            local box_errs = schema.validate_box(box)
            if #box_errs > 0 then
                for _, e in ipairs(box_errs) do
                    errors[#errors + 1] = path .. ": " .. e
                end
            else
                boxes[box.id] = box
            end
        end
    end

    if #errors > 0 then return nil, errors end

    local graph = {
        meta    = meta,
        boxes   = boxes,
        entry   = meta.entry_box_id,
        drivers = drivers,
        map_dir = map_dir,
    }

    return graph, {}
end
-- }}}

-- {{{ validate_graph
function M.validate_graph(graph)
    local errors = {}
    local boxes  = graph.boxes
    local drivers = graph.drivers

    -- entry box must exist
    if not boxes[graph.entry] then
        errors[#errors + 1] = "entry_box_id '" .. tostring(graph.entry) ..
            "' does not match any box"
    end

    for id, box in pairs(boxes) do
        -- connection integrity: reciprocal check (outgoing connections only)
        for _, c in ipairs(box.connections or {}) do
            if c.from_box ~= id then goto next_conn end

            local target = boxes[c.to_box]
            if target == nil then
                errors[#errors + 1] = "box '" .. id ..
                    "': connection to unknown box '" .. tostring(c.to_box) .. "'"
                goto next_conn
            end

            local found = false
            for _, tc in ipairs(target.connections or {}) do
                if tc.from_box    == c.from_box    and
                   tc.from_branch == c.from_branch and
                   tc.to_box      == c.to_box      and
                   tc.to_input    == c.to_input    then
                    found = true
                    break
                end
            end
            if not found then
                local branch_str = c.from_branch and ("." .. c.from_branch) or ""
                errors[#errors + 1] = "box '" .. id .. "': connection " ..
                    c.from_box .. branch_str ..
                    " -> " .. c.to_box .. "." .. c.to_input ..
                    " not reciprocated in target"
            end

            ::next_conn::
        end

        -- driver must exist for call/data boxes with a ref
        if (box.kind == "call" or box.kind == "data") and box.ref then
            local ext = extension_of(box.ref)
            -- binary callers (no fn) don't need a driver — they are the driver
            if box.fn and ext ~= "" and not drivers[ext] then
                errors[#errors + 1] = "box '" .. id ..
                    "': no driver defined for extension '" .. ext .. "'"
            end
        end
    end

    -- cycle detection from entry box
    if boxes[graph.entry] then
        local cycle_errors = detect_cycles(boxes, graph.entry)
        for _, e in ipairs(cycle_errors) do
            errors[#errors + 1] = e
        end
    end

    return #errors == 0, errors
end
-- }}}

return M
